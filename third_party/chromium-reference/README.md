# Chromium compatibility reference

M8.1 uses an upstream Chrome for Testing headless-shell build as the real-browser reference for deterministic differential scenarios.

Current pin:

- Chromium / Chrome for Testing: `151.0.7922.138`
- revision: `1654411`
- platform: `win64`
- archive: `chrome-headless-shell-win64.zip`
- source family: Google Chrome for Testing public artifacts
- SHA-256: `714f4b9d17cb82a41ee10734dde15fd8c27c08b3229ce799c0c6a7dcc7730e1e`

This version matches the Chromium version carried by the pinned Openbrowser CEF build: `151.3.17+gf059e67+chromium-151.0.7922.138`.

The compatibility workflow must verify the downloaded archive against `checksums.sha256` before extraction and must also confirm that the executable reports the expected version. A checksum or version mismatch is a runner failure, not a compatibility result.

The reference executable is test infrastructure only. It is not shipped in the Openbrowser package.
