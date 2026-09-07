# CEF desktop bootstrap

Openbrowser uses Chromium Embedded Framework (CEF) as its first browser-engine adapter. CEF is an implementation detail of the desktop adapter, not the owner of Openbrowser tab, Focus Queue, policy, profile, or configuration state.

## Pinned version

The current desktop bootstrap is pinned to:

```text
CEF 151.3.17+gf059e67+chromium-151.0.7922.138
Chromium 151.0.7922.138
```

This was the preferred Stable build when the pin was selected on 2026-09-06. CEF 152 was already available as a beta and was intentionally not selected for the first shell.

The pin lives in `cmake/OpenbrowserCEF.cmake` and is intentionally exact. Do not loosen it to `151`, `latest`, or an unbounded range.

## Why an exact pin

CEF/Chromium is a security-sensitive runtime with frequent releases. Silent major-version movement would make failures difficult to reproduce and could change browser behavior underneath Openbrowser.

An upgrade should be a deliberate change that records:

- old and new CEF versions;
- Chromium milestone/version change;
- desktop smoke-test results;
- navigation/event-contract compatibility;
- privacy/network behavior changes that affect Openbrowser policies;
- platform-specific regressions;
- updated binary integrity hashes for every automated-acquisition platform.

## Obtaining CEF for local development

Use the official CEF Automated Builds page and download the **Standard Distribution** for your platform matching the exact pinned version.

Extract it outside the repository. Do not commit CEF binaries or extracted runtime artifacts to Git.

Set `CEF_ROOT` to the extracted distribution directory, either as an environment variable or a CMake cache variable.

Example:

```bash
cmake -S . -B build-desktop \
  -DOPENBROWSER_BUILD_DESKTOP=ON \
  -DCEF_ROOT=/absolute/path/to/cef_binary_151.3.17+gf059e67+chromium-151.0.7922.138_<platform>
```

The configuration step validates:

1. `include/cef_version.h` exists;
2. `CEF_VERSION` exactly matches the Openbrowser pin;
3. `LICENSE.txt` exists;
4. `CREDITS.html` exists;
5. the CEF CMake integration exposes `CEF_LIBCEF_DLL_WRAPPER_PATH`.

A mismatch fails configuration instead of silently building against an untested engine.

## No implicit downloads in the ordinary build

The ordinary Openbrowser build remains network-independent:

```bash
cmake -S . -B build -DOPENBROWSER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`OPENBROWSER_BUILD_DESKTOP` is `OFF` by default. Enabling it locally never means “download something from the internet”.

Automated CEF acquisition exists only in explicit CI adapter/package workflows. It is not a CMake side effect. Those workflows use concrete platform archive names and the versioned integrity pins in `third_party/cef/checksums.sha1`, compare the local pin with the official CEF checksum metadata, and verify the archive before extraction.

The Windows package workflow additionally produces a user-facing SHA-256 for the final portable ZIP.

## Runtime style

The first Openbrowser shell should use CEF as a controlled Chromium surface. CEF exposes Chrome and Alloy runtime styles. Openbrowser should not adopt Chrome's product-level tabs/session model because those concepts already belong to `BrowserSession`.

The initial implementation may use CEF Views for cross-platform window/browser hosting where it is sufficient. Runtime-style selection is an adapter decision and may change as the project needs deeper control over UI, windowless rendering, networking, fingerprinting defenses, or process behavior.

## Event mapping

CEF callbacks must be normalized before reaching core code. The adapter maps CEF behavior into `BrowserEngineEventSink` events such as:

```text
CEF callback / observed state
        |
        v
CEF adapter
        |
        +--> NavigationStartedEvent
        +--> NavigationCommittedEvent
        +--> NavigationFailedEvent
        +--> TitleChangedEvent
        `--> RendererCrashedEvent
                 |
                 v
            BrowserSession
```

No `CefBrowser`, `CefFrame`, `CefRequest`, Chromium handle, thread enum, or callback type may cross into `src/core/`.

## Threading rule

CEF callbacks execute on documented CEF threads. The adapter owns that threading model. `BrowserSession` must eventually receive normalized events on the Openbrowser application/UI sequence rather than being called concurrently from arbitrary CEF threads.

The first adapter implementation must therefore make event dispatch/sequence ownership explicit before it is considered production-safe.

## Distribution notices

CEF and Chromium remain under their own licenses. Any distributed Openbrowser desktop build must preserve the notices required by the exact CEF/Chromium distribution, including upstream license/credits material. `THIRD_PARTY.md` is a ledger; it does not replace upstream notice files.

The Windows portable package fails closed if the expected CEF license/credits material is missing from the exact pinned distribution.
