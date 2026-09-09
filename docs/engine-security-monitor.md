# Engine security and currency monitor

Openbrowser embeds Chromium through a supported Chromium Embedded Framework (CEF) binary distribution. Engine updates therefore have two distinct upstream signals that must not be conflated:

1. the newest **stable CEF** build available for the release platform;
2. the newest **Chrome Stable** version published upstream.

A newer Chrome Stable version is a useful security/currency signal, but it is not permission to replace the supported CEF distribution with a Chrome binary or an arbitrary Chromium build.

## Automated monitor

`.github/workflows/engine-security-monitor.yml` runs weekly, on demand, and whenever the monitor or CEF pin changes.

The workflow executes `scripts/engine_security_monitor.py`, which:

- reads the exact CEF/Chromium pin from `cmake/OpenbrowserCEF.cmake`;
- queries the official CEF automated-build manifest at `https://cef-builds.spotifycdn.com/index.json`;
- selects the newest `stable` CEF entry for `windows64` and ignores beta/newer development channels;
- queries the official Chrome for Testing last-known-good Stable channel;
- compares the embedded Chromium version with both supported stable CEF and Chrome Stable;
- writes a versioned `engine-security-report.json` artifact and a GitHub job summary.

## Failure policy

The scheduled check fails when a **newer stable CEF build** is available. A failing monitor is a request for an explicit engine-upgrade review; it does not automatically change the repository pin.

When Chrome Stable is newer but the stable CEF channel has not moved, the monitor emits a warning and remains green. The correct response is to track upstream CEF and relevant Chrome/Chromium security releases, not to bypass the CEF support boundary.

A network/manifest/schema failure is also a real monitor failure. The workflow must not silently report the engine as current when upstream state could not be read.

## Upgrade review

A CEF pin change remains a normal engineering change and should include, at minimum:

- exact new CEF version and Chromium revision;
- official distribution checksum/trust-anchor updates;
- third-party notice review;
- Core CI;
- CEF Desktop Smoke;
- Windows Package and Product Smoke;
- Web Compatibility Differential;
- relevant WPT smoke evidence;
- real manual browser interaction and graceful-shutdown validation before release.

The lifecycle invariant remains strict: process exit `0` and persisted `clean_shutdown: true` are both required for PASS.
