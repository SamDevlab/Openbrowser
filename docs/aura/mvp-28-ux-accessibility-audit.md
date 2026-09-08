# Project Aura — MVP-28 UX & Accessibility Audit

MVP-28 is the release-quality pass before Openbrowser v0.3.0. It does not add a new product feature; it removes avoidable interaction ambiguity from the Aura surfaces introduced in MVP-20 through MVP-27.

## Audit principles

1. **Keyboard first** — browser-owned actions must remain usable without a pointer.
2. **Visible focus** — keyboard focus must be distinguishable from hover and selection.
3. **State is not color-only** — active, loading, failed and selected states retain text or symbolic cues in addition to accent color.
4. **Assistive-technology naming** — glyph-only native controls expose meaningful action names.
5. **Motion is optional** — cosmetic movement must respect reduced-motion preferences.
6. **Content keeps priority** — accessibility changes must not add permanent chrome or consume additional browsing space.

## Native Aura surfaces

### Aura sidebar

- All interactive sidebar buttons are explicitly focusable.
- Sidebar controls share a CEF accessibility group so keyboard arrow navigation can move through the logical rail/drawer actions.
- Compact glyph controls expose action-oriented accessible names instead of relying on their visible symbol.
- Workspace entries expose the workspace name and whether the workspace is currently active.
- Active workspace state remains represented by the existing non-color marker as well as the workspace accent.
- Transient confirmation text remains passive and is removed from the keyboard focus order.

### Tab strip

- Tab actions are explicitly focusable and grouped for keyboard traversal.
- Accessible names expose active, sleeping, loading and failed states when applicable.
- The active-tab close control identifies the tab it will close.
- The `+` control exposes `New tab` rather than relying on the symbol alone.
- Existing keyboard accelerators such as `Ctrl+T`, `Ctrl+W`, `Ctrl+Tab` and `Ctrl+Shift+Tab` remain authoritative.

### Browser chrome and Focus

The audit keeps the existing native CEF controls and browser-standard accelerators intact. The omnibox remains directly reachable with `Ctrl+L`; Find, History, zoom, reload and Aura visibility continue to use their registered window accelerators. Focus retains its native labeled controls and the compact timer remains a secondary surface rather than permanent chrome.

No new pointer-only interaction was introduced.

## New Tab 2.0

- Quick-access links now use a visible high-contrast keyboard focus ring instead of suppressing the outline.
- `Customize this tab` exposes a visible focus indicator.
- CSS-only wallpaper and module controls keep their no-JavaScript implementation while transferring keyboard focus visibility to their associated visible labels.
- Hidden preference inputs have explicit accessible labels.
- Decorative wallpaper swatches are hidden from the accessibility tree.
- Dark and light appearances use separate high-contrast focus tokens.
- Windows/high-contrast environments receive a `forced-colors` focus outline.
- `prefers-reduced-motion: reduce` disables both transitions and hover/focus translation.

## Existing responsive and state behavior reviewed

- New Tab already collapses to one column at narrow widths and reduces non-essential presentation in short windows.
- Focus has an explicit empty-queue state.
- Tabs expose loading and failed navigation states without relying on color.
- Aura transient panels remain mutually exclusive and retractable, preserving the web-content-first layout.

## Intentionally unchanged

- No JavaScript or remote assets were added to New Tab.
- No network, persistence, Workspace, Private Mode or FocusSprint semantics changed.
- No `CefBrowserView` teardown ownership changed.
- No pointer-driven auto-hide behavior was added.
- No claim of formal WCAG certification is made by this MVP; platform assistive-technology testing remains useful as Openbrowser matures.

## v0.3.0 acceptance gate

MVP-28 is complete only when the exact final commit passes:

- Core CI
- CEF Desktop Smoke
- Windows x64 portable package smoke
- Product Smoke / Windows x64

After those gates are green, Aura enters feature freeze and MVP-29 may contain release-readiness changes only.
