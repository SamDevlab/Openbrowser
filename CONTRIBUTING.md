# Contributing to Openbrowser

Openbrowser is at architecture-bootstrap stage. Small changes that preserve explicit boundaries are preferred over broad feature additions that make later engine/provider replacement harder.

## Build and test

```bash
cmake -S . -B build -DOPENBROWSER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Architectural rules

Before merging a change, verify:

1. Core code does not import Chromium/CEF/platform UI headers.
2. A new browser-owned network request has a declared capability and an inspectable reason.
3. Hosted services are behind interfaces and are not required for local state correctness.
4. Exported configuration uses allowlists rather than attempting to blacklist every possible secret.
5. Focus Queue semantics remain independent from visual vertical-tab layout.
6. Renderer/worker identifiers do not become stable domain identifiers.
7. Security or privacy behavior changes include negative tests, not only happy-path tests.

## Code style

- C++20 for the current core.
- Keep headers narrow and dependencies explicit.
- Prefer value/domain types over raw renderer/provider handles.
- Treat compiler warnings as defects in changed code.
- Add tests for new invariants.

## Architecture decisions

Material changes to engine choice, persistence authority, capability precedence, sync trust or process isolation should add or supersede an ADR under `docs/adr/`.
