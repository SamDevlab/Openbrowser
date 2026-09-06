# Openbrowser architecture

## Goal

Openbrowser must be able to adopt mature components without letting those components become the domain architecture. The renderer, sync backend, filter source, DNS implementation, compatibility harness, developer observability source or transfer engine may change; browser intent and policy must remain stable.

## Dependency rule

Dependencies point inward:

```text
UI / platform shell
        |
        v
application orchestration
        |
        v
Openbrowser core <---- provider ports / engine ports
        ^                       ^
        |                       |
provider adapters        engine adapters
                                |
                       normalized observations
                                |
                 compatibility / developer tools
```

The core must not include Chromium, CEF, WebView, hosted-service SDKs or platform UI headers.

Compatibility and developer tooling may consume normalized engine/network observations, but they must not become an alternate source of browser-domain truth.

## Core domains

### BrowserSession

`BrowserSession` is the application-level authority for the logical set of tabs and their lifecycle. Renderer/browser handles are projections of that state through `BrowserEngine`.

Initial session invariants:

- tab IDs are non-empty and unique inside a session;
- navigation requests with empty targets are rejected at the application boundary;
- at most one tab is `Active`;
- activating a tab demotes the previous active tab to `Background`;
- an active tab cannot be suspended directly;
- activating a suspended tab resumes it before activation;
- closing the active tab deterministically selects a surviving fallback when one exists;
- a discarded tab is not activated until a future restore contract recreates its engine state.

Horizontal tabs and vertical tabs are UI projections over this same model. They must never own competing tab lifecycle state.

Normalized asynchronous engine events exist before navigation commit state, crash recovery and renderer-failure handling are considered complete. Calling `Navigate()` only issues intent; engine events confirm what actually happened.

### Tabs

A tab is a browser-domain object. Renderer handles are adapter details and must not become `TabId`.

### Focus Queue

The Focus Queue is not a vertical-tabs implementation. It is an ordered intent model. A queue item can reference a live tab, but can also retain only a URL/context and therefore survive tab suspension or discard.

Initial invariants:

- queue item IDs are unique;
- invalid/empty URLs are rejected at the application boundary before engine navigation;
- a Focus Queue item does not require a live renderer;
- explicit queue order is independent of visual tab order;
- promotion to `Now` leaves only one promoted item and moves it to the front.

### Capabilities

Capabilities are policy inputs for both page permissions and browser-owned egress. The initial resolution order is:

```text
session > origin > workspace > global > deny
```

Unknown/undeclared capabilities resolve to `deny`.

This is deliberately deny-by-default so new outbound behavior cannot silently appear merely because a provider was linked into the binary.

### Providers

Project-owned infrastructure must be optional from the core perspective. Providers describe what they are and whether they require network access. Later provider-specific ports will define operations for sync, updates, filters, DNS and configuration-pack registries.

Examples:

```text
SyncPort
  |- LocalFilesystemSync
  |- LanSync
  |- WebDavSync
  |- SelfHostedSync
  `- HostedSync (optional)
```

No UI feature may depend directly on one of those concrete implementations.

## Engine boundary

The browser engine is an adapter. Core/application code issues browser-domain commands such as `CreateTab`, `ActivateTab`, `Navigate`, `Suspend` and `Resume`. Engine-specific handles remain inside the adapter.

The first bring-up is Chromium-family because compatibility and mature rendering are higher priority than implementing a web engine. This does not imply that Chromium types are permitted in the core.

The selected bootstrap adapter is Chromium Embedded Framework (CEF). The initial desktop shell may use CEF Views and a custom-controlled browser surface to get a cross-platform executable running without adopting Chrome's product-level tab/session model.

A deeper Chromium integration or maintained fork can replace CEF when privacy, process-model, fingerprinting, extension or networking requirements exceed the embedding surface. That migration is an adapter change, not a rewrite of BrowserSession, Focus Queue or capability policy.

CMake is the Openbrowser core build system. A future direct Chromium adapter may use Chromium's native GN/Ninja toolchain behind this boundary.

## Observation boundary

Engine observations are intentionally separated from engine commands.

The same normalized-observation principle extends to developer network tooling and compatibility diagnostics:

```text
CEF callbacks / CDP / Chromium diagnostics
                  |
                  v
          adapter normalization
                  |
          +-------+--------+
          |                |
          v                v
 BrowserSession events   Network Trace Model
                             |
                    +--------+---------+
                    |                  |
                    v                  v
              Network Lab       Compatibility tests
```

A CDP request ID, CEF browser handle or Chromium net-log identifier is adapter state. It must not become a stable persisted Openbrowser identity.

## Web compatibility boundary

Openbrowser deliberately changes browser policy, so compatibility must be measured rather than assumed.

The future compatibility system will combine:

- Web Platform Tests;
- deterministic site scenarios;
- rendering/reftest comparisons;
- normalized network/browser observations;
- differential execution against a pinned upstream Chromium reference.

Every reproducible difference must be classified before it becomes a permanent workaround.

Compatibility mitigations are data/policy records with narrow origin/version scope. They are not arbitrary patches hidden in UI code.

A compatibility fix must never silently weaken global privacy policy to repair one site.

See [`web-compatibility.md`](web-compatibility.md).

## Developer Network Lab boundary

The future Network Lab is a native developer-observability subsystem.

It operates at three possible depths:

1. request/response observation;
2. connection/transport diagnostics;
3. optional privileged packet capture through a dedicated helper.

The first two levels should work without system-wide packet capture. Browser-level data should be sourced from CEF request/resource callbacks, CDP Network events and deeper Chromium diagnostics when available, then normalized into an Openbrowser-owned trace model.

Raw packet capture, if implemented, is a separate privileged adapter. It must never be a prerequisite for ordinary browsing or normal developer tools.

Network Lab is also the explanation surface for native privacy/filter decisions and browser-owned egress.

See [`developer-network-inspector.md`](developer-network-inspector.md).

## Persistence

Local persistence will be split by sensitivity:

- normal configuration/state;
- browsing state/history;
- temporary/session state;
- secrets/vault data;
- optional developer traces/diagnostic captures.

Portable configuration exports must be schema-versioned and must exclude secret classes by default.

Developer traces are not configuration and must never be silently included in configuration exports.

## Browser-owned network activity

Browser-owned egress is separate from page network activity. Every browser-owned category must be attributable, inspectable and disable-able where technically possible:

- update checks;
- filter updates;
- sync;
- DNS/provider traffic;
- crash reports;
- pack/registry access;
- external services;
- HTTP transfers;
- BitTorrent tracker/peer traffic where supported.

The long-term UI should be able to explain why Openbrowser itself initiated a connection.

Network Lab consumes this attribution data so a developer/user can filter browser-owned traffic separately from page traffic.

## Dependency acquisition

The engine is intentionally absent from the default core build.

When automated CEF acquisition is introduced it must:

1. pin an exact CEF version;
2. record its upstream source;
3. verify an integrity hash before extraction/use;
4. keep downloaded engine artifacts outside version control;
5. preserve all required CEF/Chromium third-party notices in distributable builds.

A normal `OPENBROWSER_BUILD_TESTS=ON` core build must remain network-independent.

## Transfer boundary

HTTP(S) downloads and BitTorrent share a high-level transfer model but protocol workers remain isolated from browser state and unrestricted filesystem access.

```text
UI -> TransferBroker -> protocol worker -> FileBroker -> approved destination
```

A BitTorrent worker must not gain general browser-profile access merely because it runs in the browser distribution.

Network Lab may observe transfer metadata through an explicit telemetry port, but observation does not grant control over transfer workers or access to unrestricted payload data.
