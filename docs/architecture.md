# Openbrowser architecture

## Goal

Openbrowser must be able to adopt mature components without letting those components become the domain architecture. The renderer, sync backend, filter source, DNS implementation or transfer engine may change; browser intent and policy must remain stable.

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
```

The core must not include Chromium, CEF, WebView, hosted-service SDKs or platform UI headers.

## Core domains

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

The browser engine is an adapter. Core code issues browser-domain commands (`CreateTab`, `Navigate`, `Suspend`, `Resume`) and receives normalized events back through an application boundary.

The first bring-up is Chromium-family because compatibility and mature rendering are higher priority than implementing a web engine. This does not imply that Chromium types are permitted in the core.

A CEF-based adapter may be useful for early executable bring-up; deeper Chromium integration/forking can replace that adapter when privacy, process-model, fingerprinting or networking requirements exceed the embedding surface. The migration boundary must therefore exist before either implementation.

## Persistence

Local persistence will be split by sensitivity:

- normal configuration/state;
- browsing state/history;
- temporary/session state;
- secrets/vault data.

Portable configuration exports must be schema-versioned and must exclude secret classes by default.

## Browser-owned network activity

Browser-owned egress is separate from page network activity. Every browser-owned category must be attributable, inspectable and disable-able where technically possible:

- update checks;
- filter updates;
- sync;
- DNS/provider traffic;
- crash reports;
- pack/registry access;
- external services.

The long-term UI should be able to explain why Openbrowser itself initiated a connection.

## Transfer boundary

HTTP(S) downloads and BitTorrent share a high-level transfer model but protocol workers remain isolated from browser state and unrestricted filesystem access.

```text
UI -> TransferBroker -> protocol worker -> FileBroker -> approved destination
```

A BitTorrent worker must not gain general browser-profile access merely because it runs in the browser distribution.
