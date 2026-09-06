# Security policy

Openbrowser is currently pre-alpha and is not suitable for protecting sensitive browsing sessions.

## Reporting security issues

Please do not publish a detailed exploit as a public issue while a practical vulnerability is still unpatched. Use GitHub's private vulnerability reporting/security advisory flow for this repository when available.

Include:

- affected commit/version;
- operating system;
- reproduction steps or proof of concept;
- expected security boundary;
- observed impact.

## Current security posture

The M0 code is an engine-independent core and does not yet render untrusted web content. Security work currently focuses on preserving boundaries that later renderer, provider and transfer implementations must obey.

Key rules:

- no undeclared browser-owned network egress;
- deny unknown capabilities;
- no secret material in portable configuration by default;
- no direct hosted-provider dependencies inside core;
- no unrestricted filesystem access for future transfer workers;
- renderer handles are never browser-domain identifiers.

See `docs/threat-model.md` for the evolving threat model.
