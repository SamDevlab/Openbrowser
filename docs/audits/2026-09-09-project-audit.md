# Openbrowser whole-project audit — 2026-09-09

This audit records the post-v0.3.0 state after M8.1 real-browser differential execution, the expanded 10-scenario compatibility corpus, M8.2 pinned WPT smoke, Node 24 GitHub Actions migration, the refreshed security policy and the always-reported Core CI gate.

It is an engineering hardening snapshot, not a release certification.

## Executive assessment

Openbrowser has a strong pre-alpha architecture and release pipeline. Its primary risk is no longer absence of browser subsystems; it is converting a fast-growing implementation into a consistently reliable product surface.

Strongest areas:

- engine-independent C++ domain architecture;
- explicit CEF/Chromium adapter boundary;
- deterministic session/privacy/workspace models;
- strict Windows packaging and lifecycle validation;
- compatibility evidence from real Openbrowser vs pinned Chromium;
- pinned upstream WPT/Chromium standards-test infrastructure;
- explicit threat model and security invariants.

Highest-priority gaps:

- manual UI interaction remains a release-quality gate that automated smoke does not yet replace;
- `main` has an always-reported `CI Gate` but still requires repository-level branch/ruleset enforcement;
- sanitizer/static-analysis/fuzzing coverage is not yet comparable to the strength of the release pipeline;
- direct Openbrowser execution through `wptrunner` is not implemented;
- rendering/reftest coverage is not implemented;
- release binaries are SHA-256 verified but Authenticode signing is not yet part of the documented release boundary;
- documentation had drifted behind the merged M8.1/M8.2 implementation.

## Architecture

Keep the current dependency direction:

```text
CEF / native UI / platform
          |
          v
 application orchestration
          |
          v
   Openbrowser core
     ^           ^
     |           |
providers     engine ports
```

CEF/Chromium handles must remain adapter state. BrowserSession, capabilities, profiles, workspaces, transfers and persistence remain browser-domain state.

No architectural rewrite is recommended.

## UI reliability

The UI hardening PR exposed a class of defect that ordinary startup/product smoke did not detect: native buttons could appear correctly while their interaction path failed in real use.

Current UI acceptance must therefore retain a manual interaction gate for changes involving:

- Aura sidebar actions;
- transient panels;
- tab activation/close/new-tab actions;
- chrome hit targets;
- keyboard focus;
- layout/hit testing;
- graceful window close.

The current deferred CEF UI dispatch work reduces reentrant view-tree destruction. Future hardening should progressively reduce destructive whole-tree `RemoveAllChildViews()` rebuilds when targeted state updates are sufficient.

## Security hardening priorities

### P1 — automated analyzers

Add independent CI lanes for:

- CodeQL C/C++;
- AddressSanitizer;
- UndefinedBehaviorSanitizer.

These lanes should be advisory while being stabilized, then promoted to required security gates for security-sensitive changes.

### P1 — fuzzing

Initial fuzz targets should prioritize untrusted parsers and state boundaries:

1. `.obtrace`/trace import and JSON parsing;
2. EasyList/Adblock Plus rule parsing;
3. persisted/imported browser JSON structures;
4. compatibility observation/report parsing;
5. address/navigation input normalization where practical.

The threat model already requires imported traces/configuration to be treated as hostile input; fuzzing should make that requirement executable.

### P1 — engine security monitoring

Track the pinned CEF Preferred Stable build separately from upstream Chrome security movement. Do not automatically substitute a Chrome version for a CEF build, but surface when the embedded Chromium revision falls behind important security fixes so an explicit CEF upgrade/review decision can be made.

## Compatibility hardening priorities

The current M8.1 PR corpus covers 10 deterministic scenarios and requires real Openbrowser lifecycle success. M8.2 provides reproducible pinned WPT evidence.

Next high-value areas:

- Fetch/CORS;
- CSP;
- IndexedDB;
- workers/service workers;
- WebSockets;
- module loading;
- permissions;
- deterministic download behavior;
- deterministic rendering/reftest observations.

Direct Openbrowser `wptrunner` integration should be implemented as an explicit automation/product adapter rather than by weakening the existing M8.1 differential contract.

## CI and release

Preserve these invariants:

- Core CI must remain cross-platform;
- `CI Gate` must always report for PRs targeting `main`;
- CEF acquisition remains exact-version and checksum-pinned;
- Windows package smoke validates the extracted final ZIP;
- Product Smoke uses the exact artifact produced by packaging;
- graceful shutdown requires process exit `0` and persisted `clean_shutdown: true`;
- release publishing never rebuilds a different binary after Product Smoke;
- final user-facing ZIPs retain SHA-256 verification.

Recommended repository enforcement for `main`:

- require pull requests;
- require `CI Gate`;
- disable force pushes;
- disable branch deletion;
- optionally require conversation resolution as review volume grows.

## Supply chain

Current strengths include exact CEF pins, versioned CEF trust anchors, commit-pinned GitHub Actions, pinned WPT/Chromium references and release checksums.

Follow-up improvements:

- SBOM generation for packaged releases;
- build provenance / artifact attestation;
- documented Authenticode signing policy for Windows;
- automated engine-security update reports.

## Platform boundary

Windows x64 is the only release-qualified package today. Do not market Linux as release-qualified until the Linux sandbox/runtime boundary is explicitly hardened and validated. macOS packaging remains future work.

## Hardening order

1. Finish the UI hardening PR through real manual smoke before merge.
2. Enforce `main` protection/rules with the existing `CI Gate`.
3. Keep public documentation aligned with merged implementation.
4. Add CodeQL + ASan/UBSan lanes.
5. Add first fuzz harnesses for untrusted parsers.
6. Add engine security monitoring.
7. Build direct Openbrowser WPT automation.
8. Add Authenticode release signing.
9. Reduce destructive native UI rebuild patterns.
10. Add SBOM/provenance and expand compatibility/reftest coverage.

## Scope discipline

The immediate objective is hardening, not feature-count growth. New product features should not displace lifecycle correctness, UI reliability, security automation, compatibility evidence or repository governance.
