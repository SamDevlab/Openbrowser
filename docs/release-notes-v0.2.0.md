# Openbrowser v0.2.0

Openbrowser v0.2.0 is the second public Windows x64 release and the first release focused on turning the native CEF shell into a more recognizable browser product surface.

The release remains experimental/pre-alpha and is not positioned as a security-hardened daily driver.

## Highlights

- Browser UI foundation with a compact browser-style toolbar and cleaner tab presentation.
- First-party local New Tab page with no remote scripts or assets.
- Native Settings for startup behavior, search provider, home page, and download directory.
- Native History & Bookmarks library with workspace-aware bookmark browsing.
- Native Find in Page backed by Chromium/CEF search primitives.
- Per-tab page zoom from 25% to 500%.
- Standard shortcuts including `Ctrl+F`, `Ctrl+H`, `Ctrl+-`, `Ctrl++`, `Ctrl+0`, and `F5`.
- Bookmarks bar hidden by default while remaining available on demand.
- Product-facing security and permission presentation refinements.
- CI efficiency improvements that avoid heavy package builds for documentation-only edits.

## Privacy and architecture

- New Tab remains entirely local and script-free.
- Private tabs continue using ephemeral CEF request contexts and suppress persistent history/session state.
- Workspace request-context isolation remains intact.
- Find in Page and page zoom are browser-owned/transient and do not add network activity.
- Chromium/CEF remains isolated behind the desktop adapter while the core browser state stays engine-independent.

## Distribution

The release workflow publishes only after the exact Windows x64 portable ZIP passes:

1. Core CI.
2. CEF Desktop Smoke.
3. Extracted-package startup smoke.
4. Product Smoke with real local navigation, clean shutdown persistence, restore, and second clean shutdown.
5. SHA-256 verification.

Expected assets:

- `Openbrowser-0.2.0-windows-x64.zip`
- `Openbrowser-0.2.0-windows-x64.zip.sha256`

## Known limitations

- Windows x64 is the only published desktop package.
- macOS desktop packaging is not enabled.
- Openbrowser is not yet security-hardened for sensitive daily use.
- Full Web Platform Tests/reference-browser differential testing is not yet implemented.
- Deep TLS/transport telemetry is incomplete.
- Hosted multi-device sync and BitTorrent/`magnet:` transfers are not implemented.
