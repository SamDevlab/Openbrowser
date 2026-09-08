# Openbrowser v0.3.0 — Project Aura

Openbrowser v0.3.0 is the third public Windows x64 release and the first release centered on **Project Aura**, the design campaign that turns Openbrowser from a functional native CEF shell into a more personal, focused and recognizable browser experience.

The release remains experimental/pre-alpha and is not positioned as a security-hardened daily driver.

## Highlights

- **Project Aura design system** built around Personal, Focused, Elegant and content-first Progressive Chrome principles.
- **Retractable Aura sidebar** with compact, expanded and hidden states so browser chrome can give space back to web content.
- **New Tab 2.0** with responsive quick access, local-only wallpapers, optional modules and a cleaner search-first composition.
- **Personalization Center** with locally persisted theme, accent, sidebar default, New Tab wallpaper and module preferences.
- **Visual Workspaces** with persistent glyph and color identity plus direct switching from the Aura sidebar.
- **Focus Experience** that retracts secondary chrome during a sprint and keeps a compact active-session timer available without ending the session.
- **Motion and microinteraction pass** using native CEF Views feedback, consistent hover/pressed/selected states and short transient confirmations.
- **UX and accessibility hardening** with explicit focusable controls, accessible names/states, visible keyboard focus, forced-colors treatment and reduced-motion support.
- **Web Compatibility Differential Harness** with deterministic local fixtures, a versioned runner-result protocol, and distinct incompatibility versus crash/timeout classifications.
- Browser chrome remains intentionally compact: Find, Zoom, developer tools and secondary surfaces stay available without permanently consuming content width.

## Product behavior

- The internal New Tab implementation URL stays hidden from the omnibox on launch surfaces.
- New Tab focuses the omnibox for immediate search or navigation.
- Workspaces live in the Aura sidebar instead of competing with tabs for horizontal space.
- Focus, History & Bookmarks, Downloads, Settings and Network Lab behave as transient surfaces rather than permanent chrome.
- Starting a Focus sprint retracts the Aura sidebar, tabs, bookmarks bar and other secondary surfaces while keeping the omnibox and browsing content available.
- Pausing, resetting or completing Focus restores the browser chrome visibility state that existed before the sprint.
- Existing `settings.json` and `workspaces.json` files remain backward-compatible with the new Aura preferences and workspace identity metadata.

## Privacy and architecture

- Aura personalization remains local-first and does not require an Openbrowser account or hosted service.
- The New Tab page remains script-free and uses no remote assets.
- Private tabs continue using ephemeral CEF request contexts and suppress persistent history/session state.
- Workspace request-context isolation remains intact.
- Chromium/CEF types remain confined to the desktop adapter while engine-independent browser state stays in the Openbrowser core.
- No Aura release work changes the established `CefBrowserView` teardown ownership invariant.

## Accessibility and motion

The MVP-28 release-quality audit hardened key Aura surfaces without claiming formal WCAG certification:

- explicit keyboard focusability and traversal grouping for sidebar and tab controls;
- assistive-technology names for glyph-only actions and workspace/tab states;
- non-color cues retained for active, loading, failed and selected states;
- high-contrast visible focus on New Tab controls;
- forced-colors focus treatment;
- reduced-motion handling that removes cosmetic translation and transitions where requested.

## Web compatibility validation

The release includes the initial M8 compatibility-testing slice:

- reproducible local navigation, redirect and JavaScript/storage fixtures;
- a Windows lifecycle adapter that drives the packaged browser and requires zero exit plus `clean_shutdown`;
- deterministic JSON reports that keep incompatibility separate from runner crashes and timeouts.

This is an execution and comparison boundary, not a claim of full Web Platform Tests coverage, rendering reftests or a bundled pinned reference-browser adapter.

## Distribution

The release workflow publishes only after the exact Windows x64 portable ZIP passes:

1. Core CI.
2. CEF Desktop Smoke.
3. Extracted-package startup smoke.
4. Product Smoke with real local navigation, clean shutdown persistence, restore, and second clean shutdown.
5. SHA-256 verification.

Expected assets:

- `Openbrowser-0.3.0-windows-x64.zip`
- `Openbrowser-0.3.0-windows-x64.zip.sha256`

## Known limitations

- Windows x64 is the only published desktop package.
- macOS desktop packaging is not enabled.
- Openbrowser is not yet security-hardened for sensitive daily use.
- This release improves accessibility behavior but does not claim formal WCAG certification.
- Full Web Platform Tests/reference-browser differential testing is not yet implemented.
- Deep TLS/transport telemetry is incomplete.
- Hosted multi-device sync and BitTorrent/`magnet:` transfers are not implemented.
