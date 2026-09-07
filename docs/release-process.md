# Release process

Openbrowser releases are cut from `main` and use the version declared by the top-level CMake project.

## Release invariant

A release is publishable only when all of the following are true:

1. `main` contains the intended release commit.
2. `project(Openbrowser VERSION X.Y.Z ...)` in `CMakeLists.txt` matches the Git tag `vX.Y.Z` exactly.
3. Core CI is green for the release commit.
4. CEF Desktop Smoke is green for the release commit.
5. The Windows Package workflow builds the portable Windows x64 ZIP from the tag.
6. The extracted package passes prerequisite/runtime smoke checks.
7. Product Smoke passes against that exact artifact, including real local navigation, clean shutdown persistence, session restore, and a second clean shutdown.
8. The ZIP SHA-256 checksum verifies before publication.

The release job runs only after the package and Product Smoke jobs succeed. It does not rebuild the browser; it publishes the already-tested artifact.

## Cutting a release

For version `X.Y.Z`:

```bash
git switch main
git pull --ff-only

git tag -a vX.Y.Z -m "Openbrowser vX.Y.Z"
git push origin vX.Y.Z
```

Pushing the tag starts the Windows Package workflow. The workflow rejects the release when the tag does not match the CMake version.

After all release gates pass, GitHub Actions creates the GitHub Release and attaches:

- `Openbrowser-X.Y.Z-windows-x64.zip`
- `Openbrowser-X.Y.Z-windows-x64.zip.sha256`

Release notes are generated from the repository history. User-visible changes should also be recorded in `CHANGELOG.md` before the tag is created.

## Verifying a downloaded release

On PowerShell:

```powershell
Get-FileHash .\Openbrowser-X.Y.Z-windows-x64.zip -Algorithm SHA256
Get-Content .\Openbrowser-X.Y.Z-windows-x64.zip.sha256
```

The hashes must match before extracting or running the package.

## Failure policy

Do not publish around a failing gate. Fix the defect on a branch, merge it through the normal PR/CI flow, then create a new release tag. A failed or partially published release must not be treated as valid merely because the ZIP can be downloaded.
