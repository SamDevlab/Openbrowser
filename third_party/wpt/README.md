# Pinned Web Platform Tests revision

Openbrowser's M8.2 WPT smoke uses a fixed upstream Web Platform Tests commit instead of following `master` at runtime.

Pinned revision:

`3a1177e6c76f1a7f854f3ae4b93a00835e171fad`

Source repository:

`web-platform-tests/wpt`

The CI workflow checks out that exact Git object and verifies `HEAD` before executing the smoke subset. The WPT repository is not vendored into Openbrowser and is not distributed with Openbrowser binaries.

The smoke subset is intentionally small and lives in `tests/wpt/smoke-tests.txt`. Broad WPT execution, expectations management, chunking, and direct Openbrowser/wptrunner product integration are follow-up work.
