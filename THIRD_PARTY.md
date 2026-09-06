# Third-party software and license tracking

Openbrowser is licensed under MPL-2.0. Third-party components remain under their own licenses and must not be silently relicensed as Openbrowser code.

This file starts the dependency ledger before browser-engine binaries are committed or distributed.

| Component | Role | License / notice status | Pinned/bootstrap status |
| --- | --- | --- | --- |
| Chromium Embedded Framework (CEF) | Initial Chromium-family engine adapter | BSD-style CEF license; Chromium and bundled third-party notices also apply to distributions | `151.3.17+gf059e67+chromium-151.0.7922.138`; not vendored |
| Chromium | Rendering, JavaScript runtime, networking and browser platform transitively used by CEF | Chromium BSD license plus numerous third-party licenses | `151.0.7922.138` transitively through the pinned CEF build; not vendored |

The CEF pin was selected from the preferred Stable channel on 2026-09-06. A newer beta is not automatically a better production bootstrap. Engine upgrades require explicit compatibility/security review and a pin change.

## Rules for adding dependencies

Every runtime dependency added to Openbrowser must record:

- exact project name and upstream location;
- exact pinned version/commit when shipped;
- license identifier and required notices;
- whether source or binary artifacts are redistributed;
- integrity hash for downloaded release artifacts where practical;
- reason the dependency is needed and the architectural boundary that owns it.

The default core build must not fetch browser-engine binaries. Engine acquisition belongs to an explicit adapter/bootstrap workflow.

## CEF acquisition policy

The initial desktop bootstrap requires the exact CEF distribution to be supplied explicitly through `CEF_ROOT`. See `docs/cef-bootstrap.md`.

Automated acquisition is intentionally deferred until Openbrowser records per-platform artifact names and integrity hashes. When introduced, a downloader must reject hash mismatches and must not treat `latest` as a valid version constraint.

## Distribution requirements

A distributable Openbrowser build must include all license and notice material required by CEF, Chromium and any other shipped third-party component. Generated Chromium third-party notices should be preserved rather than replaced by this summary.

When a dependency is removed, its historical license obligations still apply to any older binaries that included it.
