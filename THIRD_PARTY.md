# Third-party software and license tracking

Openbrowser is licensed under MPL-2.0. Third-party components remain under their own licenses and must not be silently relicensed as Openbrowser code.

This file is the dependency ledger for browser-engine binaries that Openbrowser builds against or redistributes.

| Component | Role | License / notice status | Pinned/bootstrap status |
| --- | --- | --- | --- |
| Chromium Embedded Framework (CEF) | Initial Chromium-family engine adapter | BSD-style CEF license; Chromium and bundled third-party notices also apply to distributions | `152.0.6+g708dc14+chromium-152.0.7977.83`; not vendored |
| Chromium | Rendering, JavaScript runtime, networking and browser platform transitively used by CEF | Chromium BSD license plus numerous third-party licenses | `152.0.7977.83` transitively through the pinned CEF build; not vendored |

The CEF pin was selected from the preferred Stable channel on 2026-09-06. A newer beta is not automatically a better production bootstrap. Engine upgrades require explicit compatibility/security review and a pin change.

## Rules for adding dependencies

Every runtime dependency added to Openbrowser must record:

- exact project name and upstream location;
- exact pinned version/commit when shipped;
- license identifier and required notices;
- whether source or binary artifacts are redistributed;
- integrity hash for downloaded release artifacts where practical;
- reason the dependency is needed and the architectural boundary that owns it.

The default core build must not fetch browser-engine binaries. Engine acquisition belongs to an explicit adapter/bootstrap or packaging workflow.

## CEF acquisition policy

Local desktop development still requires the exact CEF distribution to be supplied explicitly through `CEF_ROOT`. See `docs/cef-bootstrap.md`.

CI is allowed to acquire CEF only in explicit browser-adapter/package workflows. Those workflows must:

- use the exact `OPENBROWSER_CEF_VERSION` family pin;
- resolve a concrete per-platform archive name rather than `latest`;
- read the expected archive digest from the versioned `third_party/cef/checksums.sha1` trust anchor;
- compare that local pin with the checksum metadata published by the official CEF binary distribution service;
- verify the downloaded archive against the pinned digest before extraction;
- reject any mismatch before configuration or packaging.

Current pinned CEF archives:

```text
linux64   SHA-1 9ff369279d2c5ddfe491370ed3a10a7c756f5eca
windows64 SHA-1 f21afbaaeb82c02a9e13dbb012c6fd0b2f11f005
```

SHA-1 is used here because it is the digest format published by the official CEF binary distribution service. Release packages produced by Openbrowser use SHA-256 for the final user-facing ZIP.

## Distribution requirements

A distributable Openbrowser build must include all license and notice material required by CEF, Chromium and any other shipped third-party component. Generated Chromium third-party notices should be preserved rather than replaced by this summary.

The Windows portable package includes the Openbrowser license/ledger plus the exact CEF distribution's `LICENSE.txt` and `CREDITS.html`. Packaging fails if those files are absent.

CEF Windows sandboxed builds also require the LPAC read/execute ACL identified by SID `S-1-15-2-2` on the runtime directory. A normal ZIP extraction does not reliably preserve NTFS ACLs, so the Openbrowser Windows runtime verifies and, when necessary, reapplies this ACL before CEF initialization. Startup fails closed when the required permission cannot be established. The Windows package workflow validates this behavior after extracting the final ZIP into a fresh directory.

When a dependency is removed, its historical license obligations still apply to any older binaries that included it.
