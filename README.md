# Openbrowser

[![Core CI](https://github.com/SamDevlab/Openbrowser/actions/workflows/ci.yml/badge.svg)](https://github.com/SamDevlab/Openbrowser/actions/workflows/ci.yml)
[![CEF Desktop Smoke](https://github.com/SamDevlab/Openbrowser/actions/workflows/cef-smoke.yml/badge.svg)](https://github.com/SamDevlab/Openbrowser/actions/workflows/cef-smoke.yml)
[![Windows Package](https://github.com/SamDevlab/Openbrowser/actions/workflows/windows-package.yml/badge.svg)](https://github.com/SamDevlab/Openbrowser/actions/workflows/windows-package.yml)

Openbrowser is an experimental open-source desktop browser focused on **local control, privacy-by-architecture, intentional tab management, native developer observability, and replaceable browser-engine boundaries**.

It uses Chromium through a pinned Chromium Embedded Framework (CEF) adapter, while keeping tabs, sessions, workspaces, privacy policy, transfers, focus state, compatibility rules, and browser-owned observations in Openbrowser-controlled C++ layers.

> **Status: pre-alpha / active product hardening.** Openbrowser has a public Windows x64 release and a working native browser shell, but it is **not yet a security-hardened daily-driver browser**. Do not rely on it for sensitive browsing sessions.

## Download

The current packaged release is **[Openbrowser v0.3.0](https://github.com/SamDevlab/Openbrowser/releases/tag/v0.3.0)** for Windows x64.

Release assets:

- `Openbrowser-0.3.0-windows-x64.zip`
- `Openbrowser-0.3.0-windows-x64.zip.sha256`

Verify the downloaded ZIP against the published `.sha256` file before running it. In PowerShell:

```powershell
Get-FileHash .\Openbrowser-0.3.0-windows-x64.zip -Algorithm SHA256
Get-Content .\Openbrowser-0.3.0-windows-x64.zip.sha256
```

The hashes must match. Then extract the ZIP and run `openbrowser.exe` from the extracted directory.

> v0.3.0 is the first **Project Aura** release, centered on retractable browser chrome, personalization, visual workspaces, Focus and a more recognizable Openbrowser identity. It remains experimental/pre-alpha and is not a security-hardened daily driver.

## What works today

The current `main` branch includes a native CEF Views browser with:

- tabbed browsing with Back, Forward, Reload, address-bar navigation, and session restore;
- **Project Aura** browser chrome with a retractable compact/expanded sidebar designed to return space to web content;
- **New Tab 2.0** with local-only Aura styling, responsive quick access, optional modules and wallpaper treatments;
- a persistent **Personalization Center** for theme, accent, sidebar defaults, New Tab wallpaper and module visibility;
- **visual Workspaces** with persistent glyph/color identity and direct switching from the Aura sidebar;
- an **Aura Focus Experience** that retracts secondary chrome during a sprint and keeps a compact active-session timer;
- keyboard-visible focus, reduced-motion handling and explicit accessibility names/states across key Aura controls;
- browser-owned search/address resolution;
- native Find in Page with live match highlighting, previous/next navigation, and match counts;
- per-tab page zoom controls from 25% to 500%;
- standard browser shortcuts including `Ctrl+F`, `Ctrl+H`, `Ctrl+-`, `Ctrl++`, `Ctrl+0`, and `F5`;
- native Settings for startup behavior, search provider, home page, downloads directory and Aura appearance preferences;
- persistent browsing history and bookmarks plus a native History & Bookmarks library;
- workspace-aware browsing with isolated CEF request contexts;
- private browsing backed by ephemeral request contexts with history/session persistence suppression;
- site permission prompts and origin permission inspection/reset;
- downloads with pause/resume/cancel tracking and destination safety checks;
- Focus Queue and focus-session tooling;
- content filtering with common EasyList / Adblock Plus rule forms;
- a command palette and browser-owned actions;
- a native Network Lab for request inspection, decision tracing, HAR export, and `.obtrace` recording;
- a deterministic Web Compatibility Differential Harness that runs **real Openbrowser against pinned Chromium 152** across a 10-scenario local corpus and keeps incompatibility separate from crash/timeout/lifecycle failures;
- a pinned upstream **Web Platform Tests smoke lane** with exact WPT, Chrome and ChromeDriver revisions/hashes and machine-readable `wptreport` output.

CEF/Chromium types remain isolated to the desktop adapter under `apps/desktop/`; the core under `src/core/` stays engine-independent.

## Why Openbrowser

Openbrowser is built around a few non-negotiable rules:

- **Local-first by default.** Core browsing state does not require a project-owned account or hosted service.
- **Engine-independent browser state.** Chromium/CEF is an adapter, not the owner of the application domain model.
- **Explicit browser-owned egress.** Browser-initiated network behavior is represented through declared capabilities instead of hidden service calls.
- **Deny-by-default capability policy.** Unknown capabilities do not become implicitly allowed because a new component was linked.
- **Replaceable providers.** Sync, filters, updates, DNS, and future hosted services belong behind ports/interfaces.
- **Observable privacy behavior.** Filtering and browser-owned network activity should be inspectable locally.
- **Measured compatibility.** Site-specific mitigations are explicit scoped records rather than silent global exceptions.
- **Conservative exports.** Diagnostics should not silently leak credentials, cookies, tokens, or vault data.

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

The dependency rule is simple: **dependencies point inward**. Chromium, CEF, platform UI types, and hosted-service SDKs do not belong in the domain core.

See [`docs/architecture.md`](docs/architecture.md) for the detailed boundaries and invariants.

## Core subsystems

### Tabs, sessions, focus, and workspaces

`BrowserSession` owns logical tab state independently of CEF handles. Current behavior includes deterministic active/background transitions, closed-tab reopening, suspend/resume/discard contracts, Focus Queue ordering, workspace metadata, versioned session snapshots, crash recovery, and per-workspace CEF request contexts.

### Privacy and capabilities

The engine-independent `CapabilityPolicy` supports global, workspace, origin, and session scope. The desktop adapter enforces navigation/resource policy and exposes interactive site permission prompts. Private sessions use dedicated in-memory contexts and suppress persistent history/session writes.

### History, bookmarks, and local sync

History and bookmarks are persisted locally through browser-owned managers. The project also contains a `SyncPort` abstraction and `LocalFilesystemSyncProvider` with versioned records and conflict handling. Hosted multi-device synchronization is not required by the browser core and is not implemented as a mandatory service.

### Downloads and file safety

CEF downloads flow through Openbrowser's transfer layer. The current implementation tracks lifecycle/progress, supports pause/resume/cancel, contains destinations through `FileBroker`, sanitizes filenames, avoids collisions, and classifies basic executable/script risks.

### Content filtering

The filtering stack supports native allow/block evaluation, domain allowlists, common EasyList/Adblock Plus rule forms, exception rules, resource modifiers, tracker categories, and decision logging correlated with Network Lab observations.

### Native Network Lab

Network Lab is browser-native developer observability rather than an extension. It currently exposes request lifecycle events, headers, bounded POST previews, transferred-byte totals, filter decisions, request/connection attribution, HAR 1.2 export, and versioned `.obtrace` export/recording with sensitive-header redaction.

Detailed TLS metadata that cannot be truthfully derived from standard CEF HTTP callbacks is intentionally left unpopulated until deeper transport telemetry is integrated.

### Web compatibility

M8.1 executes deterministic local scenarios through both real Openbrowser and a pinned Chromium 152 reference, comparing normalized browser/page observations while requiring `exit code == 0` and persisted `clean_shutdown: true` from Openbrowser. The current PR-sized corpus contains 10 scenarios.

M8.2 adds a strict, pinned WPT smoke lane for DOM, Encoding, URLSearchParams and Web Storage evidence. WPT currently runs against the pinned Chromium reference environment; direct Openbrowser execution through `wptrunner` remains a future adapter boundary.

See [`docs/web-compatibility.md`](docs/web-compatibility.md), [`docs/m8.1-real-browser-differential.md`](docs/m8.1-real-browser-differential.md), and [`docs/m8.2-wpt-smoke.md`](docs/m8.2-wpt-smoke.md).

## Technology

### Core

- C++20
- CMake 3.24+
- CTest

### Desktop engine adapter

The desktop build is pinned to:

```text
CEF 152.0.6+g708dc14+chromium-152.0.7977.83
Chromium 152.0.7977.83
```

CEF is not vendored into the repository, and ordinary core builds do not download browser-engine binaries.

## Build and test the core

Requirements:

- CMake 3.24+
- a C++20 compiler

```bash
cmake -S . -B build -DOPENBROWSER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

## Build the desktop CEF shell

Obtain the **exact pinned CEF Standard Distribution** for your platform and extract it outside the repository. Then point `CEF_ROOT` at that directory.

Example:

```bash
cmake -S . -B build-desktop \
  -DOPENBROWSER_BUILD_DESKTOP=ON \
  -DCEF_ROOT=/path/to/cef \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-desktop --config Release --parallel
```

The desktop target intentionally fails configuration if the CEF pin is wrong instead of silently building against an unverified browser-engine version.

## CI and release guarantees

Openbrowser uses multiple validation layers:

1. **Core CI** validates engine-independent code across Linux, Windows and macOS and reports an always-present `CI Gate` for pull requests targeting `main`.
2. **CEF Desktop Smoke** builds the real CEF desktop adapter against the pinned distribution.
3. **Windows Package / Product Smoke** builds the portable Windows artifact, extracts that exact ZIP, launches the packaged browser, performs real local navigation, validates persistence/session restoration, and verifies clean shutdown behavior.
4. **Web Compatibility Differential** executes real Openbrowser against the pinned Chromium reference on deterministic local scenarios.
5. **WPT Smoke** verifies the pinned WPT/Chromium standards-test environment and fails on unexpected results.

Tag-driven releases publish the **same artifact that passed Product Smoke** rather than rebuilding a separate release binary. See [`docs/release-process.md`](docs/release-process.md).

## Known limitations

Openbrowser is still experimental. Current limitations include:

- Windows x64 is the only published desktop package;
- macOS desktop packaging is not enabled;
- Linux desktop distribution/sandbox hardening is not release-qualified;
- the browser is not security-hardened for sensitive daily use;
- the compatibility system now includes real Openbrowser-vs-Chromium differential execution and a pinned WPT smoke lane, but direct Openbrowser `wptrunner` integration, broad WPT coverage, rendering reftests and a broader deterministic site corpus are not yet implemented;
- deep TLS/transport telemetry remains incomplete without a lower-level observation source;
- hosted multi-device sync is not implemented;
- BitTorrent / `magnet:` transfer support is not implemented;
- Windows artifacts are integrity-checked with SHA-256 but are not yet documented as Authenticode-signed release binaries.

## Project structure

```text
apps/desktop/        Native CEF Views browser shell and engine adapter
src/core/            Engine-independent browser domain and policies
src/engine/          Browser-engine ports/events
src/devtools/network/ Network Lab models, tracing, diagnostics, exports
tests/               Core, integration, compatibility and WPT smoke inputs
docs/                Architecture, compatibility, milestones, release process
.github/workflows/   Core, CEF, packaging, compatibility and WPT CI
scripts/             Packaging, product smoke, compatibility and reference-runner tooling
```

## License

Openbrowser is licensed under the [Mozilla Public License 2.0](LICENSE).

CEF/Chromium and bundled third-party runtime components remain subject to their respective licenses and notices.
