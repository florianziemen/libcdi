"""Unit tests for gitlab_report.py (run: python3 -m unittest discover -s .github/scripts)."""

from __future__ import annotations

import io
import os
import tempfile
import unittest
import urllib.error
from collections.abc import Mapping
from typing import Any, Self

import gitlab_report as gr

BRANCH = "feat/x"
ENV = {
    "GITHUB_SERVER_URL": "https://github.com",
    "GITHUB_REPOSITORY": "owner/libcdi",
    "GITHUB_RUN_ID": "42",
    "GITHUB_REF_NAME": BRANCH,
    "GITHUB_SHA": "0123456789abcdef",
    "BUILD_RESULT": "failure",
    "GITLAB_API_URL": "https://gitlab.example/api/v4",
    "GITLAB_PROJECT": "group/libcdi",
}


class FakeApi:
    """Records calls; GET answers come from ``pages`` keyed by path."""

    def __init__(self, pages: dict[str, list[Any]] | None = None) -> None:
        self.pages = pages or {}
        self.calls: list[tuple[str, str, Any]] = []

    def get(self, path: str, params: Mapping[str, Any] | None = None) -> Any:
        self.calls.append(("GET", path, dict(params or {})))
        page = (params or {}).get("page", 1)
        items = self.pages.get(path, [])
        start = (page - 1) * gr.PER_PAGE
        return items[start : start + gr.PER_PAGE]

    def post(self, path: str, data: Mapping[str, Any]) -> Any:
        self.calls.append(("POST", path, dict(data)))
        return {"iid": 7}

    def put(self, path: str, data: Mapping[str, Any]) -> Any:
        self.calls.append(("PUT", path, dict(data)))
        return {}

    def writes(self) -> list[tuple[str, str, Any]]:
        return [c for c in self.calls if c[0] != "GET"]


def run_info(**overrides: str) -> gr.RunInfo:
    return gr.RunInfo.from_env({**ENV, **overrides})


def open_issue(branch: str = BRANCH, iid: int = 3) -> dict[str, Any]:
    return {"iid": iid, "title": gr.issue_title(branch)}


class RunInfoTest(unittest.TestCase):
    def test_urls_encode_branch(self) -> None:
        run = run_info()
        self.assertEqual(run.run_url, "https://github.com/owner/libcdi/actions/runs/42")
        self.assertEqual(
            run.badge_url,
            "https://github.com/owner/libcdi/actions/workflows/windows.yml/badge.svg"
            "?branch=feat%2Fx",
        )
        self.assertTrue(run.runs_url.endswith("windows.yml?query=branch%3Afeat%2Fx"))

    def test_reads_summary_file(self) -> None:
        with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as fh:
            fh.write("1 tests failed\n")
        try:
            self.assertEqual(run_info(SUMMARY_FILE=fh.name).summary, "1 tests failed\n")
        finally:
            os.unlink(fh.name)

    def test_missing_summary_file_is_empty(self) -> None:
        self.assertEqual(run_info(SUMMARY_FILE="/does/not/exist").summary, "")


class TailTest(unittest.TestCase):
    def test_limits_lines_and_chars(self) -> None:
        text = "\n".join(f"line {i}" for i in range(1000))
        out = gr.tail(text)
        self.assertEqual(len(out.splitlines()), gr.SUMMARY_MAX_LINES)
        self.assertTrue(out.endswith("line 999"))
        self.assertLessEqual(len(gr.tail("x" * 20000)), gr.SUMMARY_MAX_CHARS)


class PaginationTest(unittest.TestCase):
    def test_follows_pages(self) -> None:
        api = FakeApi({"issues": list(range(gr.PER_PAGE + 5))})
        self.assertEqual(len(gr.get_all(api, "issues")), gr.PER_PAGE + 5)
        self.assertEqual([c[2]["page"] for c in api.calls], [1, 2])


class ReportIssueTest(unittest.TestCase):
    def test_failure_opens_issue(self) -> None:
        api = FakeApi()
        gr.report_issue(api, run_info(FAILED_STEP="Test"))
        [(method, path, data)] = api.writes()
        self.assertEqual((method, path), ("POST", "issues"))
        self.assertEqual(data["title"], "Windows CI failing on feat/x")
        self.assertEqual(data["labels"], gr.ISSUE_LABEL)
        self.assertIn("actions/runs/42", data["description"])
        self.assertIn("Failed step: Test", data["description"])

    def test_failure_comments_on_open_issue(self) -> None:
        api = FakeApi({"issues": [open_issue("other"), open_issue()]})
        gr.report_issue(api, run_info())
        self.assertEqual([(m, p) for m, p, _ in api.writes()], [("POST", "issues/3/notes")])

    def test_issue_lookup_filters_by_label(self) -> None:
        api = FakeApi()
        gr.report_issue(api, run_info())
        self.assertEqual(api.calls[0][2]["labels"], gr.ISSUE_LABEL)
        self.assertEqual(api.calls[0][2]["state"], "opened")

    def test_success_closes_open_issue(self) -> None:
        api = FakeApi({"issues": [open_issue()]})
        gr.report_issue(api, run_info(BUILD_RESULT="success"))
        self.assertEqual(
            [(m, p, d.get("state_event")) for m, p, d in api.writes()],
            [("POST", "issues/3/notes", None), ("PUT", "issues/3", "close")],
        )

    def test_success_without_issue_writes_nothing(self) -> None:
        api = FakeApi()
        gr.report_issue(api, run_info(BUILD_RESULT="success"))
        self.assertEqual(api.writes(), [])

    def test_cancelled_does_nothing(self) -> None:
        api = FakeApi({"issues": [open_issue()]})
        gr.report_issue(api, run_info(BUILD_RESULT="cancelled"))
        self.assertEqual(api.calls, [])


class MrBadgeTest(unittest.TestCase):
    @staticmethod
    def mr(iid: int, source: int = 1, target: int = 1) -> dict[str, Any]:
        return {"iid": iid, "source_project_id": source, "target_project_id": target}

    def test_posts_badge_once(self) -> None:
        api = FakeApi({"merge_requests": [self.mr(5)]})
        gr.ensure_mr_badges(api, run_info())
        [(method, path, data)] = api.writes()
        self.assertEqual((method, path), ("POST", "merge_requests/5/notes"))
        self.assertIn(gr.BADGE_MARKER, data["body"])
        self.assertIn("badge.svg?branch=feat%2Fx", data["body"])

    def test_skips_mr_that_has_badge(self) -> None:
        api = FakeApi(
            {
                "merge_requests": [self.mr(5)],
                "merge_requests/5/notes": [{"body": "hi"}, {"body": gr.BADGE_MARKER}],
            }
        )
        gr.ensure_mr_badges(api, run_info())
        self.assertEqual(api.writes(), [])

    def test_skips_mr_from_other_fork(self) -> None:
        api = FakeApi({"merge_requests": [self.mr(5, source=2, target=1)]})
        gr.ensure_mr_badges(api, run_info())
        self.assertEqual(api.writes(), [])

    def test_mr_lookup_uses_branch(self) -> None:
        api = FakeApi()
        gr.ensure_mr_badges(api, run_info())
        self.assertEqual(api.calls[0][2]["source_branch"], BRANCH)


class FakeResponse(io.BytesIO):
    def __enter__(self) -> Self:
        return self

    def __exit__(self, *exc: object) -> None:
        self.close()


class GitLabClientTest(unittest.TestCase):
    def test_request_shape(self) -> None:
        seen = []

        def opener(req: Any) -> FakeResponse:
            seen.append(req)
            return FakeResponse(b'[{"iid": 1}]')

        client = gr.GitLabClient("https://gl/api/v4/", "group/libcdi", "secret", opener=opener)
        self.assertEqual(client.get("issues", {"state": "opened"}), [{"iid": 1}])
        client.put("issues/1", {"state_event": "close"})
        self.assertEqual(
            seen[0].full_url, "https://gl/api/v4/projects/group%2Flibcdi/issues?state=opened"
        )
        self.assertEqual(seen[0].get_header("Private-token"), "secret")
        self.assertEqual(seen[1].get_method(), "PUT")
        self.assertEqual(seen[1].data, b'{"state_event": "close"}')

    def test_http_error_is_reported_without_token(self) -> None:
        def opener(req: Any) -> FakeResponse:
            raise urllib.error.HTTPError(
                req.full_url, 403, "Forbidden", {}, io.BytesIO(b'{"message":"403 Forbidden"}')
            )

        client = gr.GitLabClient("https://gl/api/v4", "group/libcdi", "secret", opener=opener)
        with self.assertRaises(gr.GitLabError) as ctx:
            client.post("issues", {"title": "t"})
        message = str(ctx.exception)
        self.assertIn("POST https://gl/api/v4/projects/group%2Flibcdi/issues", message)
        self.assertIn("HTTP 403", message)
        self.assertIn("403 Forbidden", message)
        self.assertNotIn("secret", message)

    def test_network_error_is_wrapped(self) -> None:
        def opener(req: Any) -> FakeResponse:
            raise urllib.error.URLError("no route")

        client = gr.GitLabClient("https://gl/api/v4", "p", "t", opener=opener)
        with self.assertRaisesRegex(gr.GitLabError, "no route"):
            client.get("issues")


class MainTest(unittest.TestCase):
    def test_without_token_does_nothing(self) -> None:
        self.assertEqual(gr.main({**ENV, "GITLAB_TOKEN": ""}), 0)

    def test_runs_both_actions(self) -> None:
        api = FakeApi({"merge_requests": [MrBadgeTest.mr(5)]})
        self.assertEqual(gr.main(ENV, api=api), 0)
        self.assertEqual([p for _, p, _ in api.writes()], ["issues", "merge_requests/5/notes"])

    def test_error_in_one_action_still_runs_other(self) -> None:
        class FailingIssues(FakeApi):
            def get(self, path: str, params: Mapping[str, Any] | None = None) -> Any:
                if path == "issues":
                    raise gr.GitLabError("GET issues -> HTTP 500")
                return super().get(path, params)

        api = FailingIssues({"merge_requests": [MrBadgeTest.mr(5)]})
        with self.assertLogs("gitlab_report", level="ERROR"):
            self.assertEqual(gr.main(ENV, api=api), 1)
        self.assertEqual([p for _, p, _ in api.writes()], ["merge_requests/5/notes"])


if __name__ == "__main__":
    unittest.main()
