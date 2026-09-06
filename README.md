# Openbrowser

Openbrowser is an experimental open-source desktop browser focused on local control, privacy-by-architecture, deep customization, intentional tab management, and native capabilities that normally require extensions.

> Status: **pre-alpha / M1 browser-shell work**. The engine-independent core is executable and tested; the Chromium/CEF desktop adapter is the next integration step.

## Engineering direction

Openbrowser follows a few non-negotiable architectural rules:

1. **Local state is authoritative by default.** Core browsing data must not require project-owned infrastructure.
2. **No mandatory account.** The browser must remain functional without an Openbrowser identity or cloud service.
3. **Network egress is explicit.** Browser-initiated traffic must pass through declared capabilities/providers rather than hidden service calls.
4. **Core domain logic is engine-independent.** Tabs, Focus Queue, capabilities, profiles, policies and configuration must not depend directly on Chromium APIs.
5. **External services are replaceable.** Sync, filters, update sources, DNS and registries sit behind provider interfaces so self-hosted implementations can replace hosted ones.
6. **Sensitive exports are deny-by-default.** Portable configuration must not silently include credentials, cookies, tokens, sessions or vault secrets.
7. **Differences are intentional and testable.** Where Openbrowser diverges from upstream browser behavior, the divergence should be classified, reproducible and covered by tests.

## Current core

The current C++20 core already contains executable models for:

- tab identity and lifecycle;
- `BrowserSession` orchestration;
- Focus Queue ordering and priority state;
- browser/page capability policy;
- replaceable engine/provider boundaries.

`BrowserSession` owns the logical tab collection. Horizontal tabs and vertical tabs will therefore be two projections of the same model rather than separate tab systems.

Current session invariants include:

- unique, non-empty tab IDs;
- non-empty navigation targets at the application boundary;
- at most one active tab;
- opening/activating a tab demotes the previous active tab;
- active tabs cannot be suspended;
- activating a suspended tab resumes it first;
- closing the active tab deterministically selects a surviving fallback;
- engine handles never become Openbrowser `TabId` values.

## Product concepts

### Tabs and vertical tabs

Openbrowser will support conventional horizontal tabs and conventional vertical tabs. Both are views over the same browser-session state.

### Focus Queue

The Focus Queue is separate from vertical tabs. It represents what the user explicitly intends to handle next.

Planned behavior:

- multi-select tabs and enqueue them;
- reorder with drag-and-drop or keyboard controls;
- queue a URL without keeping a live tab allocated;
- states such as `now`, `next`, `later` and `paused`;
- workspace-aware and optionally global queues;
- optional resource hints so lower-priority queued items can be suspended while the next item can be prepared.

The core already allows a Focus Queue item to exist without a live tab.

### Deep customization

Customization is planned as structured, portable configuration rather than one opaque preferences database:

- theme and appearance;
- toolbar/sidebar composition;
- horizontal/vertical tab layout;
- shortcuts and command palette;
- workspaces and containers;
- Focus Queue behavior;
- privacy policies;
- search engines;
- filter sources;
- custom CSS/userscripts with explicit capabilities.

Configuration import must show a diff/preview before applying changes.

### Native privacy controls

Privacy is treated as a core policy system, not an optional skin over the browser. Planned areas include tracker/ad blocking, storage isolation, cookie policy, permission policy, HTTPS controls, network policy and fingerprinting defenses.

Openbrowser does **not** define privacy as anonymity. Features such as BitTorrent have protocol-level privacy characteristics that the UI and documentation must make explicit.

### Transfers

The long-term transfer architecture unifies HTTP(S) downloads and BitTorrent under a common broker while keeping protocol engines isolated.

Planned capabilities include:

- `.torrent` and `magnet:` handling;
- file selection and priorities;
- pause/resume;
- bandwidth and seeding controls;
- hash verification;
- optional isolated background transfers;
- explicit network/privacy controls.

Torrent functionality is for legitimate peer-to-peer distribution. The browser does not bypass content authorization or DRM.

## Architecture

```text
                 +---------------------------+
                 |        Desktop UI         |
                 +-------------+-------------+
                               |
                     application commands
                               |
                 +-------------v-------------+
                 |      BrowserSession       |
                 | active tab / lifecycle    |
                 +-------------+-------------+
                               |
                 +-------------v-------------+
                 |      Openbrowser Core     |
                 | tabs / focus / policies   |
                 | profiles / config / state |
                 +------+------+-------------+
                        |      |
               providers|      |engine port
                        |      |
              +---------v--+  +-v----------------+
              | Providers  |  | Engine Adapter   |
              | local/self |  | CEF -> Chromium  |
              | hosted opt |  | deeper later     |
              +------------+  +------------------+
```

See [`docs/architecture.md`](docs/architecture.md), [`docs/threat-model.md`](docs/threat-model.md) and the ADRs in [`docs/adr/`](docs/adr/).

## Bootstrap stack

### Openbrowser core

- **C++20**
- **CMake 3.24+**
- **CTest**

C++ keeps the core close to the native Chromium/CEF boundary without requiring a mandatory FFI layer. CEF/Chromium types are still forbidden inside `src/core/`.

### First browser engine

The first desktop adapter is planned around **Chromium Embedded Framework (CEF)**. CEF provides the mature Chromium rendering/runtime surface while Openbrowser remains responsible for browser-session state, custom UI, policies, Focus Queue and configuration.

The initial shell will prefer CEF's current Chromium/Chrome runtime and CEF Views where useful. If deeper privacy, process-model, networking, extension or fingerprinting requirements exceed the CEF API, the adapter boundary allows migration to a deeper Chromium integration.

CMake remains the build system for the Openbrowser core. A future direct Chromium integration may use Chromium's native GN/Ninja toolchain behind the adapter boundary.

See [`docs/adr/0002-license-and-bootstrap-stack.md`](docs/adr/0002-license-and-bootstrap-stack.md).

## Repository layout

```text
apps/desktop/              desktop browser shell / CEF integration
src/core/session/          browser-session orchestration
src/core/tabs/             engine-independent tab domain
src/core/focus_queue/      intent/priority queue
src/core/capabilities/     page and browser capability policy
src/providers/             replaceable local/remote provider interfaces
src/engine/                rendering-engine ports/adapters
tests/fakes/               deterministic engine test doubles
tests/                     executable invariants and core tests
docs/architecture.md       system boundaries and data flow
docs/threat-model.md       initial security model
docs/adr/                  architecture decision records
THIRD_PARTY.md             dependency/license ledger
```

## Build the current core

Requirements:

- CMake 3.24+
- a C++20 compiler

```bash
cmake -S . -B build -DOPENBROWSER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Core CI runs on Linux, Windows and macOS.

## Initial milestones

### M0 — Architecture bootstrap

- [x] define local-first boundaries and invariants;
- [x] executable core model for tabs, Focus Queue and capabilities;
- [x] provider interfaces for replaceable services;
- [x] cross-platform core CI;
- [x] threat model and engine ADR;
- [x] select MPL-2.0 and start third-party license tracking.

### M1 — Browser shell

- [x] `BrowserSession` lifecycle/orchestration model;
- [x] deterministic fake engine and session tests;
- [ ] CEF adapter and basic navigation;
- [ ] first desktop window and address bar;
- [ ] horizontal + vertical tab projections;
- [ ] session persistence and crash-recovery contract;
- [ ] permission/capability enforcement at the engine boundary;
- [ ] local configuration store.

### M2 — Focus and organization

- Focus Queue UI and persistence;
- multi-select enqueue;
- workspaces;
- tab suspension/resource hints;
- command palette.

### M3 — Privacy core

- network/filter policy;
- tracker/ad blocking;
- storage and container isolation;
- per-site capability controls;
- browser-initiated network activity inspection.

### M4 — Portable configuration

- schema-versioned configuration format;
- export/import with preview;
- safe handling of secrets;
- optional self-hosted synchronization adapters.

### M5 — Native transfers

- unified transfer model;
- HTTP(S) download broker;
- isolated BitTorrent service;
- magnet/torrent UI;
- bandwidth, integrity and privacy controls.

## Contributing and security

This repository is intentionally strict about architectural boundaries. Changes that introduce undeclared browser egress, direct provider dependencies inside the core, or silent persistence of sensitive state should be treated as design regressions.

See [`CONTRIBUTING.md`](CONTRIBUTING.md), [`SECURITY.md`](SECURITY.md) and [`THIRD_PARTY.md`](THIRD_PARTY.md).

## License

Openbrowser-authored source code is licensed under the **Mozilla Public License 2.0 (MPL-2.0)**. See [`LICENSE`](LICENSE).

Third-party components remain under their respective licenses and notices; see [`THIRD_PARTY.md`](THIRD_PARTY.md).
