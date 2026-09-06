# Openbrowser

Openbrowser is an experimental open-source desktop browser focused on local control, privacy-by-architecture, deep customization, intentional tab management, and native capabilities that normally require extensions.

> Status: **pre-alpha / architecture bootstrap**. The repository is being built from the core outward. It is not yet a daily-use browser.

## Engineering direction

Openbrowser follows a few non-negotiable architectural rules:

1. **Local state is authoritative by default.** Core browsing data must not require project-owned infrastructure.
2. **No mandatory account.** The browser must remain functional without an Openbrowser identity or cloud service.
3. **Network egress is explicit.** Browser-initiated traffic must pass through declared capabilities/providers rather than hidden service calls.
4. **Core domain logic is engine-independent.** Tabs, Focus Queue, capabilities, profiles, policies and configuration must not depend directly on Chromium APIs.
5. **External services are replaceable.** Sync, filters, update sources, DNS and registries sit behind provider interfaces so self-hosted implementations can replace hosted ones.
6. **Sensitive exports are deny-by-default.** Portable configuration must not silently include credentials, cookies, tokens, sessions or vault secrets.
7. **Differences are intentional and testable.** Where Openbrowser diverges from upstream browser behavior, the divergence should be classified, reproducible and covered by tests.

## Product concepts

### Tabs and vertical tabs

Openbrowser will support conventional horizontal tabs and conventional vertical tabs. These remain normal views of the set of open tabs.

### Focus Queue

The Focus Queue is separate from vertical tabs. It represents what the user explicitly intends to handle next.

Planned behavior:

- multi-select tabs and enqueue them;
- reorder with drag-and-drop or keyboard controls;
- queue a URL without keeping a live tab allocated;
- optional states such as `now`, `next`, `later` and `paused`;
- workspace-aware and optionally global queues;
- optional resource hints so lower-priority queued items can be suspended while the next item can be prepared.

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
                 |      Openbrowser Core     |
                 | tabs / focus / policies   |
                 | profiles / config / state |
                 +------+------+-------------+
                        |      |
               providers|      |engine port
                        |      |
              +---------v--+  +-v----------------+
              | Providers  |  | Engine Adapter   |
              | local/self |  | Chromium-family |
              | hosted opt |  | implementation  |
              +------------+  +------------------+
```

The first code milestone intentionally builds the engine-independent core before binding it to a renderer.

See [`docs/architecture.md`](docs/architecture.md), [`docs/threat-model.md`](docs/threat-model.md) and the ADRs in [`docs/adr/`](docs/adr/).

## Repository layout

```text
apps/desktop/              desktop browser shell (future engine integration)
src/core/                  engine-independent domain logic
src/providers/             replaceable local/remote provider interfaces
src/engine/                rendering-engine ports/adapters
tests/                     executable invariants and core tests
docs/architecture.md       system boundaries and data flow
docs/threat-model.md       initial security model
docs/adr/                  architecture decision records
```

## Bootstrap stack

The core starts in **C++20** with CMake/CTest. Keeping the core in the same language family as Chromium reduces unnecessary FFI at the browser boundary while still allowing isolated components to use other languages later when that provides a concrete safety or maintenance advantage.

The renderer integration is intentionally behind an `Engine` port. The first desktop bring-up is expected to target a Chromium-family adapter; direct upstream coupling is not allowed inside core domain modules.

## Build the current core

Requirements:

- CMake 3.24+
- a C++20 compiler

```bash
cmake -S . -B build -DOPENBROWSER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Initial milestones

### M0 — Architecture bootstrap

- [x] define local-first boundaries and invariants;
- [ ] executable core model for tabs, Focus Queue and capabilities;
- [ ] provider interfaces for replaceable services;
- [ ] cross-platform core CI;
- [ ] threat model and engine ADR.

### M1 — Browser shell

- engine adapter and basic navigation;
- horizontal + vertical tab models;
- session persistence and crash recovery contract;
- permission/capability enforcement boundary;
- local configuration store.

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

See `CONTRIBUTING.md` and `SECURITY.md` as they are added during M0.

## License

A project license will be selected explicitly before the first distributable browser release. Until then, do not assume rights beyond what GitHub's repository access permits.