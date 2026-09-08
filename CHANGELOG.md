# Changelog

All notable user-visible changes to Openbrowser are documented in this file.

Openbrowser follows semantic versioning for published releases.

## Unreleased

### Added

- Added a first-party Openbrowser new-tab page served locally by the desktop runtime with no remote assets or scripts.
- Added a native Settings drawer for local startup, search-provider, and downloads preferences, accessible through the command palette.

### Changed

- Settings changes now apply immediately to address-bar search behavior and the download destination broker while remaining persisted in `settings.json`.
- Resetting preferences restores the first-party Openbrowser new-tab page as the startup default.
- Fresh profiles now start on the Openbrowser new-tab surface instead of a generic blank/placeholder page.
- Synthetic normal tabs are routed to the internal new-tab surface while private tabs remain `about:blank` and ephemeral.
- Internal blank/new-tab surfaces are excluded from history and the closed-tab stack.
- Reworked the desktop browser chrome into a compact browser-style toolbar with symbolic back, forward, reload, security, bookmark, downloads, profile, and overflow controls.
- Removed the always-visible Network Lab control from the primary toolbar; developer tools remain available through the command palette and registered actions.
- Simplified tab presentation, including active-tab state and close controls.
- Hid the bookmarks bar by default while preserving one-click access from the toolbar.
- Simplified permission and site-security banner copy to match the new product-facing chrome.

## [0.1.0] - 2026-09-07

### Added

- Native CEF desktop browser shell with tabbed browsing, address-bar navigation, back/forward/reload controls, keyboard shortcuts, and closed-tab reopening.
- Local session, history, bookmark, workspace, and settings persistence with recovery safeguards.
- Workspace-isolated browsing contexts and private browsing sessions backed by ephemeral CEF request contexts.
- Capability and permission policy with deny-by-default behavior and browser-owned permission prompts.
- Downloads and transfer management with destination containment and filename safety checks.
- Focus Queue, focus sessions, command palette, bookmarks bar, and workspace controls.
- Content filtering with common EasyList/Adblock Plus rule forms and decision logging.
- Native Network Lab with request inspection, connection attribution, HAR export, and `.obtrace` recording.
- Windows x64 portable ZIP packaging with bundled CEF runtime notices and SHA-256 checksum.
- Product-level Windows smoke coverage using the exact packaged ZIP, including real navigation, clean shutdown persistence, session restore, and second clean shutdown.

### Release readiness

- Tag-driven release publishing is gated by the same Windows package and Product Smoke jobs used for pull requests.
- A release tag must exactly match the CMake project version (`v0.1.0` for this release).
- The published GitHub Release attaches both the portable ZIP and its SHA-256 checksum.

### Known limitations

- Openbrowser remains an experimental pre-alpha browser and is not positioned as a security-hardened daily driver.
- The downloadable desktop release currently targets Windows x64 only.
- macOS desktop packaging is not enabled.
- Full Web Platform Tests/reference-browser differential testing is not yet implemented.
- Multi-device hosted synchronization, BitTorrent/magnet transfers, and deeper TLS/transport telemetry remain future work.
