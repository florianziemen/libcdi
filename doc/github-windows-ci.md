# Windows CI on GitHub Actions for a GitLab project

libcdi is developed on GitLab. Windows builds are tested for free on GitHub-hosted
runners:

1. GitLab **push-mirrors** the repository to a public GitHub repository.
2. On every push, GitHub Actions runs `.github/workflows/windows.yml`. It builds
   and installs libcdi with MSVC (VS 2022) + Ninja, using the CDI options of the
   ParaView superbuild (`projects/cdi.cmake`), the main Windows consumer of libcdi.
   A smoke test (`.github/smoke`) then uses the installed library the way
   ParaView's CDIReader does (`find_package(CDI)`, `CDI::CDI`, C++,
   `CDI_SIZE_TYPE=size_t`). It writes netCDF classic and netCDF4 files with a
   lon/lat and an unstructured grid, reads them back and compares the data.
   The unit tests in `tests/` are not built on Windows: they use C99
   variable-length arrays and POSIX APIs (`getopt`, `unistd.h`, `gettimeofday`)
   that MSVC does not provide.
3. A small reporter (`.github/scripts/gitlab_report.py`) writes the result back to
   GitLab:
   - failed build → issue *"Windows CI failing on &lt;branch&gt;"* (label `windows-ci`);
     further failures add comments
   - passing build → the issue gets a comment and is closed
   - an open MR whose source branch lives in this project gets one comment with a
     live badge for that branch

## Cost

- GitHub Actions on **public** repositories: standard runners, including Windows,
  are free with unlimited minutes. The Windows license comes with the runner.
- GitLab push mirroring is part of GitLab Free/CE.
- The only upkeep is rotating the two tokens below before they expire.

## Setup

Replace `<gh-owner>` with the GitHub user or organisation, and `<gitlab-project>`
with the GitLab project path (e.g. `mpim-sw/libcdi`).

1. **GitHub repository.** Create an empty public repository `<gh-owner>/libcdi`.
   Disabling issues and wiki avoids confusion, because the repository is only a
   mirror.

2. **GitHub token for the mirror.** Create a *fine-grained personal access token*,
   restricted to that repository, with the permissions
   - *Contents*: read and write
   - *Workflows*: read and write (needed because the mirror pushes `.github/workflows/`)

3. **Push mirror.** In GitLab, open *Settings → Repository → Mirroring repositories*:
   - URL: `https://github.com/<gh-owner>/libcdi.git`
   - Direction: Push. Authentication: password. User: the GitHub user.
     Password: the token from step 2.
   - Leave *Keep divergent refs* off.

   GitLab pushes after every push, at most once every 5 minutes. Use the
   *Update now* button to force a push.

4. **GitLab token for reporting** (optional; without it nothing is reported).
   In GitLab, open *Settings → Access tokens* and create a project access token
   with **role Reporter** and **scope `api`**. Reporter may create, comment on and
   close issues and comment on MRs, which is all the reporter needs. Store the
   token on GitHub:

   ```sh
   gh secret set GITLAB_ISSUE_TOKEN -R <gh-owner>/libcdi
   ```

5. **Point the workflow at the project.** In `.github/workflows/windows.yml`,
   set `GITLAB_PROJECT: <gitlab-project>` (and `GITLAB_API_URL` if the project
   is on a different GitLab host).

6. **Project badge** (optional). In GitLab, open *Settings → General → Badges*:
   - Link: `https://github.com/<gh-owner>/libcdi/actions/workflows/windows.yml`
   - Badge image URL:
     `https://github.com/<gh-owner>/libcdi/actions/workflows/windows.yml/badge.svg?branch=%{default_branch}`

## Limitations

- Only branches of the GitLab project itself are mirrored. MRs from personal forks
  are tested once they are merged, or after a maintainer pushes the branch to
  the project.
- The per-MR badge comment is posted by the first run that finds the MR open.
  An MR opened after the last push of its branch gets the badge with the next
  push. Running the workflow manually (*Run workflow* on GitHub) also works.
- Never push to the GitHub repository directly. The mirror overwrites it.

## Running the checks locally

```sh
# reporter unit tests
python3 -m unittest discover -s .github/scripts -v

# smoke test against an installed libcdi (any platform)
cmake -S .github/smoke -B smoke -DCMAKE_PREFIX_PATH=<libcdi install prefix>
cmake --build smoke && ctest --test-dir smoke --output-on-failure
```
