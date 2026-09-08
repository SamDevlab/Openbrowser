# Changelog

All notable user-visible changes to Openbrowser are documented in this file.

Openbrowser follows semantic versioning for published releases.

## Unreleased

### Added

- Started Project Aura, the v0.3.0 design campaign built around Personal, Focused, Elegant and content-first Progressive Chrome principles.
- Added the Aura design foundation for retractable browser surfaces with Expanded, Compact, Focus and Pinned/Collapsed/Auto-hide behavior models.
- Added the first retractable Aura sidebar with compact and expanded states for Workspaces, Focus, History & Bookmarks, Downloads, Settings, Commands and Network Lab.
- Added a browser-owned sidebar visibility action with `Ctrl+Shift+\\` so a fully hidden sidebar can be restored without sacrificing browsing space.
- Added New Tab 2.0 with responsive quick-access cards, compact browser-context hints and CSS-only per-tab controls for wallpaper and optional modules.
- Added the Aura Personalization Center with locally persisted theme, accent, sidebar default, New Tab wallpaper and optional module preferences.

### Changed

- Repaired the internal New Tab response MIME handling and introduced the first Aura visual treatment with local-only styling and no JavaScript or remote assets.
- The omnibox now hides Openbrowser internal blank/new-tab implementation URLs and reclaims the site-security slot on those launch surfaces.
- Simplified the permanent toolbar around navigation, the omnibox, bookmarks, downloads, profile and overflow; Find and Zoom remain available through standard shortcuts and browser actions instead of consuming permanent width.
- New Tab surfaces focus the omnibox for immediate search/navigation.
- The tab strip now gives the active tab the only dedicated close control and uses the first-party New Tab URL directly.
- Workspace switching moved out of the tab strip and into the Aura sidebar so workspace context no longer competes with tabs for horizontal space.
- Focus is hidden by default and opens as a temporary side surface from the Aura rail instead of permanently consuming content width.
- Focus, History & Bookmarks, Downloads, Settings and Network Lab now behave as mutually exclusive transient surfaces so opening one automatically returns space used by another.
- Desktop-created tabs and startup fallbacks now use the first-party New Tab URL instead of the legacy synthetic placeholder.
- New Tab 2.0 keeps search/navigation as the dominant action, lets secondary modules retract without JavaScript, provides three local-only wallpaper treatments, and adapts the composition for narrow or short viewports.
- Aura appearance settings remain backward-compatible with existing v0.2 `settings.json` files, apply sidebar defaults live, and become the defaults for newly loaded or reloaded New Tab surfaces.

## [0.2.0] - 2026-09-08

### Added

- Added a first-party Openbrowser new-tab page served locally by the desktop runtime with no remote assets or scripts.
- Added a native Settings drawer for local startup, search-provider, and downloads preferences, accessible through the command palette.
- Added a native History & Bookmarks library with recent-history controls and workspace-scoped bookmark browsing.
- Added native Find in Page with live highlighting, match counts, previous/next navigation, and a dedicated toolbar entry point.
- Added browser-owned per-tab page zoom controls with visible percentage, zoom-out, reset, and zoom-in actions.
- Added browser-standard desktop shortcuts for Find in Page (`Ctrl+F`), History (`Ctrl+H`), zoom out/in/reset (`Ctrl+-`, `Ctrl++`, `Ctrl+0`), and reload (`F5`).

### Changed

- Documentation-only Markdown changes no longer run the Core CI matrix, and README/changelog/release-process edits no longer rebuild and smoke the Windows portable package.
- Browser-standard desktop shortcuts are registered as high-priority CEF window accelerators so focused web content cannot consume them before browser chrome.
- Page zoom now uses CEF browser-host zoom levels directly, remains transient per tab, and never changes persisted browser settings.
- Find in Page follows the active tab, clears Chromium find selection when closed, and keeps search state browser-owned without persistence or network activity.
- History can now be opened, removed entry-by-entry, or cleared from browser-owned UI; Private mode keeps persistent history hidden and immutable.
- Bookmarks can now be opened and removed from the library while Private mode exposes persistent bookmarks as read-only.
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

### Release readiness

- Version metadata is advanced to `0.2.0` and the tag-driven release workflow will require an exact `v0.2.0` tag match.
- The Windows x64 ZIP and `.sha256` asset are built once, smoke-tested from the extracted package, reused by Product Smoke, and only then published.
- The release remains experimental/pre-alpha; this version improves product usability but does not claim daily-driver security hardening.

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
