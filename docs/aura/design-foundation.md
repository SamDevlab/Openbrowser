# Openbrowser Aura — Design Foundation

Aura is the product-design campaign that moves Openbrowser from a functional browser shell to a personal, elegant browser experience.

The campaign begins after v0.2.0 and targets v0.3.0 as the first release where visual identity, personalization and content-first interaction are first-class product features.

## North Star

**Your browser, your space.**

Openbrowser should adapt to the amount of browser UI the user needs instead of permanently taking space away from web content.

The governing interaction rule is:

> The deeper the user moves into content, the less browser chrome should remain visible.

A new UI element must justify why it needs permanent screen space. If it does not, it should be a drawer, overlay, compact rail, transient indicator or auto-hidden surface.

## Product pillars

### Personal

The browser can adapt visually, structurally, contextually and behaviorally without forcing users to manage dozens of options at once.

### Focused

The web page remains the primary surface. Focus mode is not only a timer; it is also a lower-chrome interface state.

### Elegant

The product uses restrained hierarchy, consistent spacing, deliberate typography, soft surfaces and subtle accent color instead of visual noise.

## Progressive Chrome

Aura defines three levels of browser chrome.

### Expanded

Used for organization and configuration.

- full workspace/sidebar labels
- normal tab presentation
- browser toolbar
- contextual drawers when requested

### Compact

Expected to become the normal browsing state.

- sidebar becomes an icon rail
- tabs remain available but visually quieter
- primary toolbar contains navigation, security state, omnibox and a small set of high-frequency actions
- secondary tools remain available through drawers or the command center

### Focus

Optimized for maximum content area.

- sidebar hidden
- optional compact tab treatment
- bookmarks hidden
- secondary browser controls hidden
- focus status reduced to a small transient indicator
- omnibox remains available directly or through keyboard/edge reveal

## Retractable surfaces

Each non-content surface should eventually support a deliberate visibility model.

| Surface | Target states |
| --- | --- |
| Sidebar | expanded / rail / hidden |
| Tabs | normal / compact |
| Toolbar | normal / minimal / auto-hide |
| Bookmarks | visible / hidden |
| Workspaces | expanded / rail / hidden with sidebar |
| Focus | drawer / compact indicator / hidden |
| Downloads | drawer / transient status / hidden |
| History | drawer / hidden |
| Settings | drawer / hidden |
| Network Lab | drawer / detached specialist surface / hidden |
| New Tab widgets | expanded / compact / disabled |

The default product should favor content space while keeping discoverability through the sidebar rail, command center and standard shortcuts.

## Panel behavior

Retractable surfaces use one of three behavior policies:

- **Pinned** — remains visible because the user explicitly chose it.
- **Collapsed** — remains represented by a compact affordance such as an icon rail.
- **Auto-hide** — leaves the content area and appears only through an explicit trigger or edge reveal.

Auto-hide must never cause unpredictable pointer-driven flicker. Explicit user preference wins over automatic behavior.

## Primary toolbar rule

The primary toolbar is reserved for high-frequency browser actions.

Target composition:

`Back · Forward · Reload · Security · Omnibox · Bookmark · Downloads · Menu`

Find, zoom, Focus, workspace controls, Settings and Network Lab should not consume permanent primary-toolbar space when keyboard shortcuts, menu actions or drawers already provide access.

## Sidebar role

The Aura sidebar becomes the primary browser-owned navigation surface and the visual identity of Openbrowser.

Target modules:

- Workspaces
- Home / New Tab
- Bookmarks
- History
- Downloads
- Focus
- Settings
- Network Lab

Users will eventually choose which modules are visible and whether the sidebar is left, right, rail or hidden.

## Workspaces

Workspaces evolve from storage/session structure into visual context.

Each workspace may eventually own:

- name
- icon
- accent color
- tab set
- bookmarks
- optional New Tab treatment
- optional Focus profile

Workspace accent should be subtle and limited to selection, focus rings and small indicators rather than recoloring the whole browser.

## New Tab

New Tab is the showcase of Aura personalization but should start minimal.

Default hierarchy:

1. Openbrowser identity
2. search/navigation cue
3. optional shortcuts
4. optional contextual widgets

Future widgets may include greeting, clock, recent sites, workspace context and Focus status. Every widget is optional.

No remote asset or hosted account is required for the default page.

## Visual language

Initial dark direction:

- deep neutral background rather than pure black
- slightly elevated surfaces
- low-contrast borders
- bright primary text
- muted secondary text
- one user-selectable accent family
- restrained gradients reserved for identity and selected states

Suggested radius family:

- small: 6 px
- medium: 9 px
- large: 12 px
- extra large: 16–24 px for major cards/start-page surfaces

Motion should remain short and functional, generally in the 120–200 ms range once animation support is introduced.

## Personalization model

Personalization is intentionally broader than theme colors.

### Visual

- light / dark / system
- accent
- wallpaper
- dim/contrast treatment

### Structural

- sidebar position/state
- toolbar density
- tab density
- bookmarks visibility

### Contextual

- workspace identity
- workspace-specific visual treatment
- workspace-specific New Tab options

### Behavioral

- startup behavior
- New Tab modules
- Focus behavior
- standard and user-defined shortcuts where supported

Advanced controls should be progressively disclosed. The default Appearance screen should remain understandable at a glance.

## Aura roadmap

### MVP-20 — Design Foundation & New Tab Repair

- codify Aura principles
- repair internal New Tab HTML rendering
- establish first visual language on the local New Tab surface
- preserve zero remote assets and zero required JavaScript

### MVP-21 — Browser Chrome Redesign

- simplify primary toolbar
- remove secondary controls from permanent chrome
- refine tab strip hierarchy
- hide internal New Tab URL from the omnibox
- focus omnibox appropriately on New Tab

### MVP-22 — Retractable Sidebar Foundation

- expanded / rail / hidden states
- migrate Focus/workspace presence away from permanent content width
- make sidebar state user-controlled

### MVP-23 — New Tab 2.0

- shortcuts
- local wallpaper modes
- optional widgets
- content-safe responsive composition

### MVP-24 — Personalization Center

- theme
- accent
- density
- sidebar state/position
- New Tab controls
- live preview/application where practical

### MVP-25 — Visual Workspaces

- icon and accent per workspace
- compact workspace switching
- visual context without excessive recoloring

### MVP-26 — Focus Experience

- Focus as a drawer
- compact active-session indicator
- Focus interface state that retracts nonessential chrome

### MVP-27 — Motion & Microinteractions

- controlled transitions
- hover/pressed/selected state consistency
- transient notifications

### MVP-28 — UX & Accessibility Audit

- keyboard navigation
- focus visibility
- contrast
- density/accessibility checks
- empty/error states

### MVP-29 — v0.3.0 Release Readiness

- product polish freeze
- documentation and release notes
- full desktop/package/Product Smoke validation

## Definition of success for v0.3.0

The campaign succeeds when all of the following are true:

1. Web content receives clearly more visual priority than browser-owned tooling.
2. Core browser surfaces can retract without losing discoverability.
3. A normal user can keep developer-oriented tools out of sight.
4. Personalization changes the browser’s feel without making the interface inconsistent.
5. Workspaces and Focus feel integrated into the product rather than bolted-on utilities.
6. The interface remains recognizable as Openbrowser even when the product name is hidden.

That final criterion is the visual North Star for Aura.
