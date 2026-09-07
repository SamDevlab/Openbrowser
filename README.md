# Openbrowser

[![Core CI](https://github.com/SamDevlab/Openbrowser/actions/workflows/ci.yml/badge.svg)](https://github.com/SamDevlab/Openbrowser/actions/workflows/ci.yml)
[![CEF Desktop Smoke](https://github.com/SamDevlab/Openbrowser/actions/workflows/cef-smoke.yml/badge.svg)](https://github.com/SamDevlab/Openbrowser/actions/workflows/cef-smoke.yml)

Openbrowser is an experimental open-source desktop browser focused on **local control, privacy-by-architecture, intentional tab management, native developer observability, and replaceable browser-engine boundaries**.

The project uses Chromium through a pinned Chromium Embedded Framework (CEF) adapter, but Chromium does not own Openbrowser's domain model. Tabs, sessions, workspaces, capabilities, focus state, transfers, compatibility policy, and browser-owned network observations live behind engine-independent C++ interfaces.

> **Status: pre-alpha / active M7 development.** The CEF desktop shell is executable and already includes real navigation, tabs, workspaces, permission controls, downloads, bookmarks, focus tooling, content filtering, session recovery, and a native Network Lab. Openbrowser is **not yet a daily-driver or security-hardened browser** and should not be used to protect sensitive browsing sessions.

## Why Openbrowser

Openbrowser is built around a few non-negotiable rules:

- **Local-first by default.** Core browsing state must not require project-owned infrastructure or a mandatory account.
- **Engine-independent browser state.** Chromium/CEF is an adapter, not the owner of tabs, session lifecycle, policy, or application state.
- **Explicit browser-owned egress.** Browser-initiated network behavior is represented through declared capabilities instead of hidden service calls.
- **Deny-by-default capability policy.** Unknown capabilities do not become implicitly allowed because a new provider or component was linked.
- **Replaceable providers.** Sync, filters, update sources, DNS, and future hosted services belong behind ports/interfaces.
- **Observable privacy behavior.** Filtering and browser-owned network activity should be inspectable and explainable locally.
- **Compatibility is measured.** Site-specific mitigations are scoped policy records rather than silent global privacy exceptions.
- **Sensitive exports are conservative.** Network traces and configuration must not silently expose credentials, cookies, tokens, or vault data.

## What works today

### Desktop browser shell

The current CEF Views desktop application includes:

- a top-level native browser window;
- one CEF browser surface per Openbrowser `TabId`;
- address-bar navigation;
- Back, Forward, and Reload controls;
- a horizontal tab strip driven by `BrowserSession`;
- a separate Focus Queue sidebar;
- title/navigation/crash events normalized back into Openbrowser-owned events;
- versioned session snapshots and clean-shutdown detection;
- session restore on startup;
- workspace switching with workspace-specific CEF request contexts;
- site permission prompts with Allow / Block / Dismiss actions;
- a site security badge and origin permission inspection/reset;
- a command palette overlay;
- a workspace-aware bookmarks bar;
- a downloads/transfer drawer;
- an experimental profile/private-session toggle;
- a native Network Lab drawer.

The UI and CEF adapter live in `apps/desktop/`. CEF/Chromium headers are intentionally forbidden from `src/core/`.

### Tab lifecycle, focus, and workspaces

`BrowserSession` is the application-level authority for logical tab state. Engine handles never become persistent Openbrowser identifiers.

Implemented behavior includes:

- deterministic active/background tab transitions;
- tab suspension/resume contracts;
- LRU-style tab discard policies for memory pressure;
- on-demand revival of discarded tabs;
- Focus Queue ordering independent from visual tab order;
- focus sprint/timer and attention metrics;
- workspace metadata on tabs;
- per-workspace CEF request contexts for cookie/storage partitioning;
- session persistence of tabs, workspaces, and Focus Queue items.

### Capability and privacy policy

Openbrowser has an engine-independent `CapabilityPolicy` with rules that can be scoped globally, per workspace, per origin, or per session.

Current capability categories include page networking, sync, DNS, filter updates, external services, camera, microphone, geolocation, notifications, clipboard access, persistent/third-party storage, automation, and userscripts.

The CEF adapter already enforces capability checks at navigation and resource-load boundaries and exposes interactive permission prompts for supported site capabilities.

### Content filtering

The current filtering stack includes:

- native allow/block evaluation;
- tracker categories for advertising, analytics, fingerprinting, cryptomining, and social tracking;
- domain allowlists;
- CEF pre-flight resource blocking;
- EasyList / Adblock Plus-style parsing for common rule forms;
- domain anchors such as `||example.com^`;
- exception rules with `@@`;
- resource modifiers such as `$script`, `$image`, `$stylesheet`, `$xmlhttprequest`, `$subdocument`, and `$third-party`;
- a filter-decision model for explaining policy results.

Live CEF content blocking evaluates through `ContentFilter::EvaluateWithId()` and correlates decisions into `FilterDecisionLog`, displaying block/allow decisions and explanations directly in the Network Lab Decisions view.

### Downloads and file safety

CEF downloads are connected to Openbrowser's transfer layer through `TransferBroker`.

Implemented pieces include:

- transfer lifecycle tracking;
- progress and transfer-rate calculation;
- pause, resume, and cancel controls;
- a desktop Downloads Panel;
- `FileBroker` destination containment;
- path-traversal defenses;
- Windows/POSIX filename sanitization;
- collision-safe destination naming;
- basic executable/script risk classification.

BitTorrent / `magnet:` support is part of the long-term transfer architecture but is **not implemented yet**.

### History, bookmarks, and local sync primitives

Openbrowser currently contains:

- a history manager with visit counting, deduplication, search, and ranking;
- a bookmark manager with tags, search, URL deduplication, and workspace scope;
- a desktop bookmarks bar with quick-add and one-click navigation;
- a `SyncPort` abstraction;
- a `LocalFilesystemSyncProvider` with versioned records, conflict handling, manifests, and disk serialization.

History and bookmark managers are persisted to disk in the desktop runtime (`history.json` and `bookmarks.json`), and the local filesystem sync provider is integrated into `DesktopApp` with the `sync.trigger_local` action (`Ctrl+Shift+S`).

### Profiles and compatibility policy

The core contains:

- persistent and ephemeral profile models;
- an in-memory secret/session-data vault;
- explicit purge of ephemeral profile data;
- site-scoped compatibility mitigation records;
- expiration/scoping for compatibility rules;
- User-Agent / Client Hints policy modes for standard Chromium, normalized anti-fingerprinting output, and site-scoped overrides.

The desktop UI supports Default and Private profiles managed by `SessionPrivacyOrchestrator`. Private profiles are backed by dedicated in-memory `CefRequestContext` instances with no disk cache or persistent cookies, automatic exclusion of private tabs from session snapshots, suppression of history recording via `SessionHistoryBridge`, TabStrip isolation (hiding normal tabs during private sessions), programmatic activation guards rejecting persistent tabs in private mode, and strict context purging upon session exit before persistent tabs are restored or created. Dynamic User-Agent and client hint policies, along with site-scoped compatibility mitigations, are enforced directly at the live request boundary.

### Native Network Lab

Network Lab is a local developer-observability subsystem rather than an extension.

The live CEF path currently provides:

- Openbrowser-owned request IDs;
- request start/redirect/response/completion/failure events;
- request and response headers;
- bounded POST body previews;
- transferred-byte totals;
- request filtering and inspection;
- browser/page network attribution;
- HAR 1.2 export;
- batch `.obtrace` export with a versioned `obtrace/1` format and sensitive-header redaction.

The M7 diagnostic layer also contains:

- connection/TLS diagnostic models and a `ConnectionRegistry`;
- DNS/connect/TLS timing fields;
- protocol, ALPN, endpoint, certificate, and connection-reuse metadata models;
- a filter/policy decision log;
- Network Lab Requests / Connections / Decisions views;
- a streaming `ObtraceRecorder` core;
- command actions for HAR and `.obtrace` export.

Connection endpoints and identifiers are observed directly from live CEF response headers and registered in `ConnectionRegistry`, linking `NetworkEvent::connection_id` across request summaries. Detailed TLS metadata (such as version, cipher suite, and cert subject) and protocol negotiation are not fabricated from standard HTTP callbacks and remain unpopulated until deeper transport or NetLog/CDP telemetry is integrated. Per-request filter decisions correlate via `request_id`, and live `.obtrace` recording can be toggled via `network_lab.toggle_recorder` (`Ctrl+Shift+R`) with sensitive header redaction and private-mode write suppression.

### Compatibility engineering

The compatibility layer already includes:

- deterministic `CompatibilityScenario` definitions;
- a `CompatibilityRunner`;
- expected-status/header checks;
- blocked-tracker assertions;
- site-scoped mitigation records.

A full Web Platform Tests pipeline, pinned-reference differential browser execution, and rendering/reftest comparison infrastructure remain future work.

## Architecture

```text
+----------------------------+
|     Desktop UI / CEF       |
| chrome, tabs, panels       |
+-------------+--------------+
              |
              | application commands
              v
+-------------+--------------+
|       BrowserSession       |
| tab lifecycle / intent     |
+-------------+--------------+
              |
              v
+-------------+--------------+
|      Openbrowser Core      |
| focus / policy / history   |
| bookmarks / transfers      |
| profiles / sync / compat   |
+------+---------------+-----+
       |               |
       | provider port | engine port
       v               v
+------+-------+   +---+----------------+
|  Providers   |   | Engine Adapter     |
| local/self   |   | CEF -> Chromium    |
| hosted opt.  |   | replaceable later  |
+--------------+   +---------+-----------+
                            |
                            | normalized observations
                            v
                  +---------+-----------+
                  | Network / Compat    |
                  | traces + diagnostics|
                  +---------------------+
```

The core dependency rule is simple: **dependencies point inward**. Chromium, CEF, platform UI types, and hosted-service SDKs do not belong in the domain core.

See [`docs/architecture.md`](docs/architecture.md) for the detailed boundaries and invariants.

## Technology

### Core

- **C++20**
- **CMake 3.24+**
- **CTest**

### Desktop engine adapter

The current desktop bootstrap is pinned to:

```text
CEF 151.3.17+gf059e67+chromium-151.0.7922.138
Chromium 151.0.7922.138
```

The exact pin is deliberate. Openbrowser does not build against `latest` or silently move Chromium versions underneath the project.

CEF is not vendored into this repository. The ordinary core build remains network-independent.

## Build and test the core

Requirements:

- CMake 3.24+
- a C++20 compiler

```bash
cmake -S . -B build -DOPENBROWSER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

The default build does not need Chromium/CEF and does not download browser-engine binaries.

## Build the desktop CEF shell

Obtain the **exact pinned CEF Standard Distribution** for your platform and extract it outside the repository. Then point `CEF_ROOT` at that directory:

```bash
cmake -S . -B build-desktop \
  -DOPENBROWSER_BUILD_DESKTOP=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCEF_ROOT=/absolute/path/to/cef_binary_151.3.17+gf059e67+chromium-151.0.7922.138_<platform>

cmake --build build-desktop --target openbrowser --config Release --parallel
```

Configuration validates the CEF version and required distribution files before the desktop target is generated.

See [`docs/cef-bootstrap.md`](docs/cef-bootstrap.md) for the complete bootstrap and distribution rules.

### Runtime switches

The desktop shell currently recognizes:

```text
--url=<startup-url>
--storage-dir=<path>
--session-file=<path>
--network-lab
```

Without `--url`, the current pre-alpha startup URL is `https://example.com/`.

## Platform status

| Layer | Linux | Windows | macOS |
| --- | --- | --- | --- |
| Core build + tests | ✅ CI | ✅ CI | ✅ CI |
| CEF desktop target | ✅ | ✅ | 🚧 not enabled yet |
| Dedicated CEF smoke CI | ✅ Linux x64 | — | — |

The desktop CEF target has Linux and Windows source/build paths. macOS desktop packaging/helper-process support is intentionally not declared yet, although the engine-independent core is tested on macOS.

## Testing and CI

The current CMake configuration registers **36 CTest suites** spanning session invariants, navigation, permissions, workspaces, transfers, filtering, history/bookmarks, synchronization, compatibility, profiles, private profile isolation, desktop-integration contracts, and M7 Network Lab diagnostics.

GitHub Actions currently runs:

- **Core CI** on Linux, Windows, and macOS;
- **CEF Desktop Smoke** on Linux x64 against the exact pinned CEF distribution;
- strict compiler warnings (`/W4` on MSVC and `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion` on GCC/Clang-oriented builds).

The CEF smoke workflow downloads only the pinned artifact, verifies its repository checksum against upstream metadata, verifies the downloaded archive, and then builds the desktop target.

## Repository layout

```text
apps/desktop/                 CEF Views desktop shell and Chromium adapter
src/core/                     engine-independent browser domains
  bookmarks/                  bookmarks model and search
  capabilities/               deny-by-default capability policy
  commands/                   action registry and command palette core
  compatibility/              scenarios, mitigations, UA policy
  filters/                    content filter and adblock parser
  focus_queue/                Focus Queue and focus sprint logic
  history/                    browsing-history model
  navigation/                 address-input normalization
  network/                    attribution, queries, decisions, waterfall
  profiles/                   persistent/ephemeral profile models
  session/                    BrowserSession, persistence, discard policy
  sync/                       sync ports and local filesystem provider
  tabs/                       tab domain types
  transfers/                  transfer broker and safe file broker
  workspaces/                 workspace model and management
src/devtools/network/         network trace, connection diagnostics, obtrace
src/engine/                   rendering-engine command/event contracts
src/providers/                replaceable provider boundary
cmake/                        CEF bootstrap integration
tests/                        executable invariants and subsystem tests
docs/                         architecture, security, compatibility and ADRs
third_party/                  pinned third-party integrity metadata
```

## Known limitations

Openbrowser is intentionally explicit about what is not complete yet:

- it is pre-alpha and not suitable for sensitive browsing sessions;
- no packaged stable release or daily-driver support contract exists yet;
- the dedicated CEF smoke workflow currently validates Linux x64 only;
- macOS desktop support is not enabled yet;
- detailed TLS version/cipher/certificate telemetry and protocol negotiation are not exposed by standard high-level CEF callbacks (fields remain unpopulated/unknown rather than fabricated);
- private browsing isolates RequestContext and suppresses disk history/snapshots, but full anonymity guarantees against hardware/memory side channels are not claimed;
- multi-device remote synchronization backend is future work (currently local filesystem sync only);
- full WPT/reference-browser differential testing is not implemented yet;
- BitTorrent/magnet transfers and optional raw packet capture are not implemented.

## Security posture

Openbrowser assumes web content is untrusted and treats security boundaries as architecture inputs rather than post-release cleanup.

Key invariants include:

- unknown capabilities deny by default;
- renderer/CEF identifiers do not become browser-domain identities;
- hosted services are not required for local source-of-truth state;
- transfer destinations are brokered rather than granting unrestricted filesystem authority;
- browser-owned egress must be attributable;
- developer traces are local artifacts and sensitive headers are redacted by default;
- site compatibility workarounds must be narrow and reviewable rather than globally weakening privacy policy.

Openbrowser privacy controls are **not a claim of anonymity**.

See [`SECURITY.md`](SECURITY.md) and [`docs/threat-model.md`](docs/threat-model.md).

## Documentation

- [`docs/implementation-progress.md`](docs/implementation-progress.md) — current milestone implementation state
- [`docs/architecture.md`](docs/architecture.md) — dependency rules and system boundaries
- [`docs/cef-bootstrap.md`](docs/cef-bootstrap.md) — exact CEF pin and desktop bootstrap
- [`docs/threat-model.md`](docs/threat-model.md) — trust boundaries and abuse cases
- [`docs/developer-network-inspector.md`](docs/developer-network-inspector.md) — Network Lab design
- [`docs/web-compatibility.md`](docs/web-compatibility.md) — compatibility strategy
- [`docs/adr/`](docs/adr/) — architecture decision records
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — contribution and architecture rules
- [`THIRD_PARTY.md`](THIRD_PARTY.md) — dependency/license ledger

## Contributing

Changes should preserve the engine/provider boundaries instead of taking shortcuts through CEF or hosted-service APIs.

Before merging a material change, ask whether it:

1. keeps Chromium/CEF/platform UI types out of `src/core/`;
2. gives new browser-owned network behavior an explicit capability and observable reason;
3. keeps local state correct without requiring a hosted service;
4. avoids leaking renderer/provider identifiers into stable domain state;
5. adds negative tests when security/privacy behavior changes;
6. records material architecture changes in an ADR when appropriate.

See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## License

Openbrowser-authored source code is licensed under the **Mozilla Public License 2.0 (MPL-2.0)**. See [`LICENSE`](LICENSE).

CEF, Chromium, and other third-party components remain under their respective licenses and notice requirements. See [`THIRD_PARTY.md`](THIRD_PARTY.md).