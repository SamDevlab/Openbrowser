# Project Aura — MVP-27 Motion & Microinteractions

MVP-27 adds restrained interaction polish without turning browser chrome into an animation layer.

## Interaction rule

Motion exists to confirm state change, not to decorate it.

Openbrowser continues to prioritize web content, so native browser surfaces use short platform-owned feedback instead of long layout animations, bouncing controls or pointer-driven auto-hide behavior.

## Native button motion

Aura-owned tab and sidebar controls enable CEF's native ink-drop state transition. This gives hover and press actions one consistent response while keeping rendering in the native Views layer.

Workspace buttons keep their own persisted workspace accent:

- active workspace: accent remains visible in normal, hover and pressed states;
- inactive workspace: the native normal state stays theme-compatible and the workspace accent appears on hover/press;
- non-workspace actions retain native theme colors and native motion.

This avoids hard-coding dark-mode foreground colors into browser chrome while Aura's light/system theme work is still evolving.

## Selected-state hierarchy

Selected state remains explicit and redundant rather than color-only:

- active workspace keeps the `●` marker plus its workspace accent;
- active tab keeps its existing active marker;
- close action remains exposed only for the active tab;
- tooltips name the target/action and preserve standard shortcut discoverability.

## Transient feedback

Expanded Aura sidebar actions can show a short browser-owned confirmation row such as:

- `Workspace · Work`
- `Sidebar · compact`
- `Downloads`
- `Settings`

Feedback auto-dismisses after 1600 ms. A generation guard prevents an older delayed task from hiding a newer message, and a weak lifetime token prevents delayed CEF tasks from touching a destroyed sidebar.

The compact rail intentionally does not grow to display feedback; keeping its width stable is more important than surfacing status text there.

## Safety and architecture constraints

- no JavaScript or remote assets;
- no timers on web content;
- no change to FocusSprint, FocusQueue, workspace isolation or Private Mode rules;
- no browser-view ownership changes during CEF teardown;
- no null-delegate `CefLabelButton` placeholders;
- no hover-driven sidebar auto-hide or layout flicker;
- native CEF Views remain the owner of browser-chrome interaction.

## Acceptance criteria

1. Tabs, close/new-tab controls and Aura sidebar buttons use consistent native press feedback.
2. Active workspace remains visually distinguishable without relying on color alone.
3. Workspace hover/press uses its existing persisted accent instead of introducing a second color source.
4. Expanded sidebar actions surface short confirmations and dismiss them automatically.
5. Repeated actions cannot let an older dismissal task clear newer feedback.
6. Compact sidebar width never changes to accommodate transient feedback.
7. Core CI, CEF Desktop Smoke, Windows package smoke and Product Smoke stay green.
