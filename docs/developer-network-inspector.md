# Native Developer Network Inspector

## Goal

Openbrowser should eventually include a first-class network inspection tool for developers that is deeper and more transparent than a conventional request table while remaining scoped to browser activity by default.

The design is **Wireshark-inspired**, not a copy of Wireshark and not a hidden system-wide packet sniffer.

The default tool observes traffic owned by Openbrowser and maps lower-level network activity back to browser concepts such as tabs, frames, service workers, downloads and browser-owned services.

## Working name

`Network Lab` is the current architectural name for the feature.

It may later appear in the UI under Developer Tools -> Network Lab.

## Core principle

Observation is separate from transport implementation.

```text
CEF / Chromium network sources
        |
        +--> CDP Network events
        +--> CEF request/resource callbacks
        +--> Chromium net-log style events (when available)
        +--> browser-owned provider/transfer telemetry
        |
        v
NetworkObservationAdapter
        |
        v
Openbrowser Network Trace Model
        |
        +--> live table
        +--> waterfall
        +--> connection graph
        +--> request detail
        +--> filters/search
        +--> local trace recorder
        `--> export
```

CEF/CDP identifiers must not become durable Openbrowser trace IDs.

## Capture levels

### Level 1 — Request capture

Default developer mode.

Capture normalized browser-level activity such as:

- request URL and method;
- initiator type and frame/tab;
- request headers;
- response status/headers;
- protocol (`http/1.1`, `h2`, `h3` where observable);
- MIME/content type;
- cache/service-worker source;
- redirects;
- request/response sizes;
- timing phases;
- failures/cancellations;
- WebSocket connection/frame metadata;
- downloads and browser-owned requests where applicable.

Sensitive header values are redacted by default.

### Level 2 — Connection/transport diagnostics

Optional deeper view for browser-owned connections.

Where the Chromium/CEF surface exposes the data, Network Lab should correlate:

- hostname resolution;
- remote endpoint;
- proxy route;
- TCP/QUIC connection reuse;
- TLS version/cipher/certificate metadata;
- ALPN/protocol selection;
- connection timing;
- connection pool reuse;
- HTTP/2 stream or HTTP/3 connection relationship;
- network errors and retry decisions.

This level is intended to answer questions such as "why was this request slow?" or "which connection carried these requests?" without requiring packet capture.

### Level 3 — Raw packet capture (future, optional)

A future privileged extension may provide real packet capture using an OS-specific capture backend such as libpcap/Npcap or another audited capture API.

This mode must be:

- disabled by default;
- explicitly started by the user;
- isolated in a dedicated privileged helper process;
- visibly indicated while active;
- separately permissioned from normal browser developer tools;
- scoped to Openbrowser processes/interfaces when the OS permits;
- bounded by capture size/time limits;
- never required for normal Network Lab operation.

Packet capture is a diagnostic capability, not part of ordinary browsing.

## Trace model

A future normalized trace should distinguish at least:

```text
TraceSession
  |- Tab / Browser service
      |- Navigation
          |- Request
              |- Redirect(s)
              |- DNS/Connection metadata
              |- Request headers/body metadata
              |- Response headers/body metadata
              |- Timing
              `- Failure/Completion
```

Suggested identity types:

```text
TraceSessionId
NetworkRequestId
ConnectionId
FrameId
TabId
```

Only `TabId` is shared with the core browser domain. Engine-specific request IDs are adapter-private mappings.

## Developer UI

The first useful UI should contain four complementary views.

### Requests

A sortable/filterable request table similar to browser DevTools but with Openbrowser-specific context.

Candidate columns:

- time;
- tab/workspace;
- method;
- host/path;
- status;
- type;
- protocol;
- source (network/cache/service worker);
- transferred size;
- duration;
- privacy/blocking decision;
- connection ID.

### Waterfall

Visual phases such as:

```text
queue -> DNS -> connect -> TLS -> request -> TTFB -> download
```

Unavailable engine phases should be shown as unavailable rather than estimated silently.

### Connection graph

A Wireshark-inspired topology view linking:

```text
Tab
  -> Origin
      -> DNS result
          -> Connection
              -> Streams / Requests
```

This makes multiplexing and connection reuse visible without requiring knowledge of engine-private data structures.

### Request detail

Sections may include:

- overview;
- headers;
- payload;
- response preview;
- timing;
- initiator chain;
- cookies/storage interaction;
- security/TLS;
- redirects;
- service worker/cache;
- privacy/filter decisions;
- raw normalized event history.

## Filtering

Network Lab should support both normal text search and a structured filter syntax.

Examples:

```text
host:api.example.com
method:POST
status:>=400
protocol:h3
tab:work-42
workspace:development
blocked:true
source:service-worker
connection:conn-17
```

Filters operate on the normalized trace model rather than CDP payload strings.

## Payload handling

Request and response bodies can contain credentials, personal data and secrets.

Therefore:

- body capture is off or bounded by default;
- large/binary payloads are not retained automatically;
- `Authorization`, `Proxy-Authorization`, `Cookie` and `Set-Cookie` values are redacted by default;
- form fields and request bodies require explicit reveal/capture policy;
- password/autofill/vault contents must never be exposed merely because Network Lab is enabled;
- traces should support a "safe export" redaction pass before writing to disk.

## Local trace recording

Live observation should use a bounded in-memory buffer by default.

Optional recording may write a local trace file with a versioned open schema, tentatively:

```text
*.obtrace
```

The format should store normalized events rather than serialized engine internals.

Exports should eventually include:

- HAR for request-level interoperability;
- JSON/NDJSON Openbrowser trace;
- sanitized diagnostic bundle;
- PCAP/PCAPNG only when real packet-capture mode supplied raw packets.

A request-level trace must never be mislabeled as PCAP.

## Browser-owned traffic

Network Lab should provide a dedicated filter/view for traffic initiated by Openbrowser itself:

- update checks;
- filter updates;
- sync;
- DNS providers;
- registry/pack access;
- crash reporting when explicitly enabled;
- HTTP downloads;
- BitTorrent tracker/peer activity at an appropriate metadata level.

This directly supports Openbrowser's architectural rule that browser-owned egress must be attributable.

## Privacy/filter explanation

Because Openbrowser plans native filtering/privacy controls, every request policy decision should be explainable where possible.

Example:

```text
Request: https://tracker.example/collect
Decision: blocked
Layer: tracking protection
Rule source: list/example#1234
Scope: global privacy profile
```

Network Lab is therefore also the debugging interface for privacy policy, not merely a traffic viewer.

## Capture security

Network traces are sensitive assets.

Security rules:

- capture is local by default;
- no trace is uploaded automatically;
- capture state is visible in the UI;
- persistent traces use normal user-approved filesystem destinations;
- raw capture helpers do not receive browser vault access;
- trace import is untrusted input;
- parser code must be fuzzable and bounds-checked;
- capture data from private/incognito profiles follows stricter retention rules and should not persist unless the user explicitly requests it.

## Engine integration strategy

For the initial CEF adapter, likely data sources include CEF resource/request callbacks and the Chrome DevTools Protocol Network domain.

CDP is an adapter source only. Its tip-of-tree schema is not treated as a stable Openbrowser public API.

A future deeper Chromium adapter may consume lower-level net logging/diagnostics when necessary and normalize it into the same trace model.

## Performance requirements

Inspection must not become a permanent tax on normal browsing.

- capture processing is disabled when Network Lab is off except for minimal counters already required by browser operation;
- event buffering is bounded;
- body capture has explicit size limits;
- UI rendering must be decoupled from network threads;
- high-volume traces may sample visualization while preserving explicitly recorded raw normalized events within configured bounds;
- packet capture, if implemented, runs outside the renderer/browser core process.

## Compatibility integration

Network Lab can become one of the evidence sources for the Web Compatibility System.

A differential test can compare normalized request traces between Openbrowser and the pinned Chromium reference while ignoring expected nondeterministic values such as connection IDs, timestamps or dynamically generated request identifiers.

## Non-goals

Network Lab does not automatically decrypt arbitrary traffic from unrelated applications. It does not silently install a system MITM certificate, and it does not require privileged system-wide packet capture to provide useful browser diagnostics.
