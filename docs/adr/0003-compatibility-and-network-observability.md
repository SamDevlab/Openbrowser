# ADR 0003 — Compatibility and network observability boundaries

- Status: Accepted
- Date: 2026-09-06

## Context

Openbrowser plans intentional differences from upstream Chromium in privacy, browser policy, native tooling and UI. Those differences create two engineering requirements:

1. site compatibility regressions must be detected and classified;
2. developers need native visibility into browser network behavior without depending on extensions or remote services.

Using engine-private data structures directly for either requirement would couple long-lived Openbrowser behavior to CEF/Chromium implementation details.

A Wireshark-like developer experience also creates security risk if ordinary developer inspection is implemented as an always-on privileged packet sniffer.

## Decision

### Compatibility

Openbrowser will build a future compatibility system around normalized observations and pinned references.

- WPT is the primary standards/interoperability suite.
- Deterministic site scenarios and rendering/reftests supplement WPT.
- Differential runs compare Openbrowser against a pinned Chromium reference from the same major where practical.
- Divergences are classified before permanent mitigation.
- Compatibility mitigations are versioned/scoped policy records with regression tests, not hidden ad-hoc code paths.
- A site-specific mitigation does not silently disable global privacy/security controls.

### Network observability

Openbrowser will expose a native `Network Lab` built on an Openbrowser-owned trace model.

Initial data sources may include:

- CEF request/resource callbacks;
- Chrome DevTools Protocol Network events;
- browser-owned provider/transfer telemetry;
- deeper Chromium diagnostics when required.

These are adapters. Their IDs and schemas do not become the public/persisted Openbrowser trace API.

The first implementation levels are:

1. request/response observation;
2. connection/transport diagnostics;
3. optional raw packet capture through a separate privileged helper in a later milestone.

Normal Network Lab functionality must not depend on raw packet capture.

### Trace safety

- traces are local by default;
- capture buffers are bounded;
- sensitive headers are redacted by default;
- raw packet capture is opt-in and visibly active;
- trace files are untrusted input;
- developer traces are not configuration and are not silently synchronized/exported.

## Consequences

### Positive

- Compatibility can be measured without treating Chrome behavior as an unquestioned specification.
- Privacy changes can be distinguished from accidental regressions.
- Network tooling survives a future CEF -> deeper Chromium adapter migration.
- Openbrowser can provide a Wireshark-inspired browser-centric view without requiring permanent privileged capture.
- Network traces can feed differential compatibility diagnostics.

### Costs

- Normalization requires adapter maintenance when CEF/CDP changes.
- Differential test infrastructure will be expensive compared with unit tests.
- Deep transport diagnostics may require Chromium surfaces below normal CEF APIs.
- Packet capture, if added, requires platform-specific security engineering and packaging.

## Rejected alternatives

### Depend exclusively on Chrome DevTools UI

Rejected because Openbrowser needs privacy/filter explanations, browser-owned egress attribution, open trace formats and compatibility integration beyond an embedded DevTools panel.

### Persist raw CDP JSON as the Openbrowser trace format

Rejected because the tip-of-tree CDP protocol can change and engine-specific schemas would become permanent project debt.

### Make packet capture the default network monitor

Rejected because it introduces privilege, privacy and performance costs that are unnecessary for most browser debugging.

### Fix compatibility with broad user-agent/site hacks

Rejected because unclassified site hacks accumulate hidden behavior and can weaken privacy globally.
