# Third-party software and license tracking

Openbrowser is licensed under MPL-2.0. Third-party components remain under their own licenses and must not be silently relicensed as Openbrowser code.

This file starts the dependency ledger before browser-engine binaries are committed or distributed.

| Component | Role | License / notice status | Repository status |
| --- | --- | --- | --- |
| Chromium Embedded Framework (CEF) | Initial Chromium-family engine adapter | BSD-style CEF license; Chromium and bundled third-party notices also apply to distributions | Planned; not vendored yet |
| Chromium | Rendering, JavaScript runtime, networking and browser platform transitively used by CEF | Chromium BSD license plus numerous third-party licenses | Planned transitively through CEF; not vendored yet |

## Rules for adding dependencies

Every runtime dependency added to Openbrowser must record:

- exact project name and upstream location;
- exact pinned version/commit when shipped;
- license identifier and required notices;
- whether source or binary artifacts are redistributed;
- integrity hash for downloaded release artifacts where practical;
- reason the dependency is needed and the architectural boundary that owns it.

The default core build must not fetch browser-engine binaries. Engine acquisition belongs to an explicit adapter/bootstrap workflow.

## Distribution requirements

A distributable Openbrowser build must include all license and notice material required by CEF, Chromium and any other shipped third-party component. Generated Chromium third-party notices should be preserved rather than replaced by this summary.

When a dependency is removed, its historical license obligations still apply to any older binaries that included it.
