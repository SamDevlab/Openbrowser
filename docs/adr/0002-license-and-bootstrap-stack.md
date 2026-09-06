# ADR 0002 — License and bootstrap stack

- Status: Accepted
- Date: 2026-09-06

## Context

Openbrowser is intended to remain genuinely open source while integrating a browser engine and other mature components that have their own licenses. The project also needs a practical path to a working desktop browser without making Chromium, a hosted service, or a UI toolkit part of the domain model.

The bootstrap stack must therefore optimize for four things at once:

1. web compatibility;
2. control over browser behavior;
3. replaceable architectural boundaries;
4. license clarity for contributors and downstream forks.

## Decision

### Project license

Openbrowser-authored source code is licensed under **Mozilla Public License 2.0 (MPL-2.0)**.

MPL-2.0 is a file-level copyleft license. Changes to MPL-covered files remain available under MPL-2.0 when distributed, while separately licensed files and larger works may retain compatible terms. This is a better fit than a purely permissive license for a browser because improvements to Openbrowser files should remain available to the community, without forcing unrelated third-party components into a single project-wide license.

Third-party code is not relicensed. Its original notices and license obligations must be preserved and tracked.

### Core language

The engine-independent core remains **C++20**.

Reasons:

- Chromium and CEF expose native C/C++ integration surfaces;
- it avoids an unnecessary mandatory FFI layer at the hottest browser boundary;
- modern C++ is adequate for deterministic domain objects such as tabs, sessions, policies and queue state;
- isolated components may still use memory-safe languages later when there is a concrete benefit.

Using C++ for the core does not permit Chromium/CEF types to cross into the core domain.

### Core build system

The Openbrowser core remains on **CMake + CTest**.

CMake is not intended to replace Chromium's native build system. If Openbrowser later maintains a direct Chromium integration or fork, that adapter may use Chromium's GN/Ninja toolchain and expose a narrow boundary to the CMake-built core.

### Initial browser engine adapter

The first executable browser shell will target **Chromium Embedded Framework (CEF)**.

CEF is used as an adapter, not as the Openbrowser architecture. It gives the project a mature Chromium-family rendering/runtime surface while allowing Openbrowser to own tabs, Focus Queue, policies, configuration and UI behavior.

The first shell should prefer:

- CEF's current Chrome bootstrap/runtime;
- a custom-controlled browser surface rather than inheriting Chrome product UI;
- CEF Views for the initial cross-platform shell where it is sufficient;
- a pinned CEF release and integrity checksum once binary acquisition is automated.

If requirements such as fingerprinting defenses, network stack control, process isolation, extension behavior or browser internals exceed the CEF surface, the engine adapter may be replaced by a deeper Chromium integration without changing core domain APIs.

### UI toolkit

Qt WebEngine and Electron are not bootstrap dependencies.

Qt WebEngine would add another Chromium abstraction and additional LGPL/GPL compliance surface without solving a core requirement that CEF does not already address. Electron would make Node/JavaScript runtime assumptions central to the desktop shell and reduce the value of the native browser boundary.

This decision does not prohibit purpose-specific UI technologies later. UI remains an outer adapter.

## Dependency rules

- No CEF or Chromium headers in `src/core/`.
- No hosted-service SDKs in `src/core/`.
- Default core builds must not download remote binaries.
- Automated CEF acquisition must pin an exact version and verify integrity.
- Every shipped third-party component must be recorded in `THIRD_PARTY.md` or generated third-party notices.
- Browser-owned network access must remain capability-governed regardless of engine implementation.

## Consequences

### Positive

- Strong web compatibility is available early.
- Openbrowser can ship a custom browser model instead of rebuilding Blink/V8.
- Core tests remain fast and independent of a multi-hundred-megabyte engine distribution.
- A future Chromium fork does not require rewriting tab/session/policy logic.
- MPL-2.0 keeps modifications to Openbrowser files open while remaining practical for a mixed-license browser stack.

### Costs

- CEF is not a complete drop-in browser product; Openbrowser must implement substantial browser-shell behavior.
- CEF/Chromium updates become an ongoing security and compatibility responsibility.
- A future direct Chromium integration may require maintaining GN/Ninja build glue and patches separately from the CMake core.
- Distribution must preserve Chromium/CEF and other third-party notices in addition to the MPL-2.0 license for Openbrowser code.
