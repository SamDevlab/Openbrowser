# ADR-0001: Keep the browser core independent of the rendering engine

- Status: Accepted
- Date: 2026-09-06

## Context

Openbrowser needs mature web compatibility early, while its long-term design may require progressively deeper control of Chromium networking, privacy, process isolation and browser UI behavior.

Coupling tab state, Focus Queue, provider logic or policy directly to Chromium/CEF types would make an eventual move from an embedding layer to deeper Chromium integration disproportionately expensive.

## Decision

The core owns stable browser-domain types and exposes an engine port. Rendering-engine implementations live under `src/engine/` (or later platform-specific repositories/modules) and translate between engine handles/events and Openbrowser domain commands/events.

The initial implementation target is Chromium-family. An early CEF adapter is allowed for browser-shell bring-up, but CEF is not considered the domain API and may be replaced by deeper Chromium integration.

## Consequences

Positive:

- core behavior is testable without a renderer;
- engine migration has an explicit boundary;
- Focus Queue and privacy/provider policy remain Openbrowser concepts;
- differential tests can compare adapters against a reference browser.

Costs:

- adapter code must normalize engine events;
- some Chromium-specific capabilities will require carefully designed extension points rather than leaking types inward;
- not every engine feature can or should be abstracted upfront.

## Guardrail

A core header importing Chromium, CEF or platform browser-engine headers is an architecture regression unless this ADR is superseded.
