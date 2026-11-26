#!/usr/bin/env python3
"""Report a GitHub Actions run of the Windows CI back to GitLab.

Needs a GitLab project access token with role *Reporter* and scope ``api``
(stored as GitHub secret, passed in as ``GITLAB_TOKEN``). Without a token the
script does nothing.

* failed run:  open the issue "Windows CI failing on <branch>", or comment on
  it if it is already open
* passed run:  comment on and close that issue, if it is open
* every run:   make sure each open MR of the branch carries exactly one comment
  with the live GitHub badge for that branch

Only the Python standard library is used.
"""

from __future__ import annotations

import json
import logging
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
from collections.abc import Callable, Mapping
from dataclasses import dataclass
from typing import Any, Protocol

log = logging.getLogger("gitlab_report")

ISSUE_LABEL = "windows-ci"
BADGE_MARKER = "<!-- windows-ci-badge -->"
PER_PAGE = 100
SUMMARY_MAX_LINES = 60
SUMMARY_MAX_CHARS = 8000


class GitLabError(RuntimeError):
    """A GitLab API call failed."""


class Api(Protocol):
    """The subset of the GitLab project API this script needs."""

    def get(self, path: str, params: Mapping[str, Any] | None = None) -> Any: ...

    def post(self, path: str, data: Mapping[str, Any]) -> Any: ...

    def put(self, path: str, data: Mapping[str, Any]) -> Any: ...


Opener = Callable[[urllib.request.Request], Any]


class GitLabClient:
    """Minimal client for ``/projects/<project>/...`` endpoints."""

    def __init__(
        self,
        api_url: str,
        project: str,
        token: str,
        opener: Opener | None = None,
        timeout: float = 30.0,
    ) -> None:
        quoted = urllib.parse.quote(project, safe="")
        self._base = f"{api_url.rstrip('/')}/projects/{quoted}"
        self._token = token
        self._opener: Opener = opener or (lambda req: urllib.request.urlopen(req, timeout=timeout))

    def get(self, path: str, params: Mapping[str, Any] | None = None) -> Any:
        return self._request("GET", path, params=params)

    def post(self, path: str, data: Mapping[str, Any]) -> Any:
        return self._request("POST", path, data=data)

    def put(self, path: str, data: Mapping[str, Any]) -> Any:
        return self._request("PUT", path, data=data)

    def _request(
        self,
        method: str,
        path: str,
        params: Mapping[str, Any] | None = None,
        data: Mapping[str, Any] | None = None,
    ) -> Any:
        url = f"{self._base}/{path.lstrip('/')}"
        if params:
            url += "?" + urllib.parse.urlencode(params)
        headers = {"PRIVATE-TOKEN": self._token, "Accept": "application/json"}
        body = None
        if data is not None:
            body = json.dumps(data).encode()
            headers["Content-Type"] = "application/json"
        request = urllib.request.Request(url, data=body, method=method, headers=headers)
        # The token travels in a header only, so ``url`` is safe to log.
        try:
            with self._opener(request) as response:
                payload = response.read()
        except urllib.error.HTTPError as exc:
            detail = exc.read().decode(errors="replace")
            raise GitLabError(f"{method} {url} -> HTTP {exc.code}: {detail}") from exc
        except urllib.error.URLError as exc:
            raise GitLabError(f"{method} {url} -> {exc.reason}") from exc
        return json.loads(payload) if payload else None


@dataclass(frozen=True)
class RunInfo:
    """What happened in the GitHub Actions run."""

    branch: str
    sha: str
    result: str  # needs.build.result: success | failure | cancelled | skipped
    run_url: str
    badge_url: str
    runs_url: str
    failed_step: str = ""
    summary: str = ""

    @classmethod
    def from_env(cls, env: Mapping[str, str]) -> RunInfo:
        server = env.get("GITHUB_SERVER_URL", "https://github.com")
        repo = env["GITHUB_REPOSITORY"]
        workflow = env.get("WORKFLOW_FILE", "windows.yml")
        branch = env["GITHUB_REF_NAME"]
        summary = ""
        summary_file = env.get("SUMMARY_FILE", "")
        if summary_file and os.path.isfile(summary_file):
            with open(summary_file, encoding="utf-8", errors="replace") as fh:
                summary = fh.read()
        workflow_url = f"{server}/{repo}/actions/workflows/{workflow}"
        return cls(
            branch=branch,
            sha=env["GITHUB_SHA"],
            result=env["BUILD_RESULT"],
            run_url=f"{server}/{repo}/actions/runs/{env['GITHUB_RUN_ID']}",
            badge_url=f"{workflow_url}/badge.svg?branch={urllib.parse.quote(branch, safe='')}",
            runs_url=f"{workflow_url}?query={urllib.parse.quote('branch:' + branch, safe='')}",
            failed_step=env.get("FAILED_STEP", ""),
            summary=summary,
        )


def get_all(api: Api, path: str, params: Mapping[str, Any] | None = None) -> list[Any]:
    """Follow GitLab's page-based pagination."""
    items: list[Any] = []
    page = 1
    while True:
        batch = api.get(path, {**(params or {}), "per_page": PER_PAGE, "page": page})
        items.extend(batch)
        if len(batch) < PER_PAGE:
            return items
        page += 1


def issue_title(branch: str) -> str:
    return f"Windows CI failing on {branch}"


def tail(text: str) -> str:
    """Last lines of a (possibly long) log, bounded in lines and characters."""
    lines = text.strip().splitlines()[-SUMMARY_MAX_LINES:]
    return "\n".join(lines)[-SUMMARY_MAX_CHARS:]


def failure_text(run: RunInfo) -> str:
    parts = [
        f"The Windows (MSVC) build of `{run.sha[:12]}` on branch `{run.branch}` failed.",
        "",
        f"- Run: {run.run_url}",
    ]
    if run.failed_step:
        parts.append(f"- Failed step: {run.failed_step}")
    if run.summary.strip():
        parts += [
            "",
            "<details><summary>Log tail</summary>",
            "",
            "```",
            tail(run.summary),
            "```",
            "",
            "</details>",
        ]
    return "\n".join(parts)


def find_open_issue(api: Api, branch: str) -> dict[str, Any] | None:
    title = issue_title(branch)
    issues = get_all(api, "issues", {"state": "opened", "labels": ISSUE_LABEL})
    return next((issue for issue in issues if issue["title"] == title), None)


def report_issue(api: Api, run: RunInfo) -> None:
    if run.result not in ("success", "failure"):
        log.info("Build result is %r; leaving issues alone.", run.result)
        return
    issue = find_open_issue(api, run.branch)
    if run.result == "failure":
        if issue:
            api.post(f"issues/{issue['iid']}/notes", {"body": failure_text(run)})
            log.info("Commented on issue #%s.", issue["iid"])
        else:
            body = failure_text(run) + (
                "\n\nThis issue is closed automatically once the Windows build passes again."
            )
            created = api.post(
                "issues",
                {"title": issue_title(run.branch), "description": body, "labels": ISSUE_LABEL},
            )
            log.info("Opened issue #%s.", created["iid"])
    elif issue:
        api.post(
            f"issues/{issue['iid']}/notes",
            {"body": f"Fixed: `{run.sha[:12]}` passes the Windows build ({run.run_url})."},
        )
        api.put(f"issues/{issue['iid']}", {"state_event": "close"})
        log.info("Closed issue #%s.", issue["iid"])


def badge_text(run: RunInfo) -> str:
    return (
        f"{BADGE_MARKER}\n"
        f"**Windows CI** (GitHub Actions, MSVC): "
        f"[![Windows CI]({run.badge_url})]({run.runs_url})\n\n"
        f"The badge always shows the latest run for `{run.branch}`."
    )


def ensure_mr_badges(api: Api, run: RunInfo) -> None:
    merge_requests = get_all(
        api, "merge_requests", {"state": "opened", "source_branch": run.branch}
    )
    for mr in merge_requests:
        # MRs from other forks may share the branch name but are not mirrored.
        if mr["source_project_id"] != mr["target_project_id"]:
            continue
        notes = get_all(api, f"merge_requests/{mr['iid']}/notes")
        if any(BADGE_MARKER in note.get("body", "") for note in notes):
            continue
        api.post(f"merge_requests/{mr['iid']}/notes", {"body": badge_text(run)})
        log.info("Added badge to MR !%s.", mr["iid"])


def main(env: Mapping[str, str] = os.environ, api: Api | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
    if api is None:
        token = env.get("GITLAB_TOKEN", "")
        if not token:
            log.info("GITLAB_TOKEN is not set; not reporting to GitLab.")
            return 0
        api = GitLabClient(env["GITLAB_API_URL"], env["GITLAB_PROJECT"], token)
    run = RunInfo.from_env(env)
    status = 0
    for action in (report_issue, ensure_mr_badges):
        try:
            action(api, run)
        except GitLabError as exc:
            log.error("%s failed: %s", action.__name__, exc)
            status = 1
    return status


if __name__ == "__main__":
    sys.exit(main())
