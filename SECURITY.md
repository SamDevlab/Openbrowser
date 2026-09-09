# Security policy

Openbrowser is an experimental desktop browser and is not yet suitable for high-risk or highly sensitive browsing sessions.

The current product renders untrusted web content through CEF/Chromium and includes local browser state, downloads, permissions, developer network tooling, compatibility infrastructure, profiles/workspaces and packaged Windows builds. Security work therefore focuses on maintaining strict boundaries between untrusted page content, browser-owned state, local files, developer tooling and optional providers.

## Supported versions

Security fixes should target the latest released version and `main` unless a narrower scope is explicitly documented.

| Version | Supported |
| --- | --- |
| 0.3.x | Yes |
| 0.2.x | Best effort only |
| 0.1.x | No |

## Reporting security issues

Please do not publish a detailed exploit as a public issue while a practical vulnerability is still unpatched.

Use GitHub's private vulnerability reporting or security-advisory flow for this repository when available.

Include:

- affected commit/version;
- operating system;
- reproduction steps or proof of concept;
- expected security boundary;
- observed impact;
- whether the issue reproduces in the packaged Windows build, a local build, or both.

Avoid attaching live credentials, session cookies, access tokens or private browsing data to a report. Redact sensitive values whenever they are not required to reproduce the issue.

## Current security posture

Openbrowser now executes untrusted web content through a pinned CEF/Chromium adapter. A renderer or site must therefore be treated as potentially hostile.

Current security invariants include:

- browser-owned network egress must belong to an explicit capability;
- unknown capabilities deny by default;
- renderer handles are never browser-domain identifiers;
- renderer compromise must not imply unrestricted access to browser state or the filesystem;
- local state remains the source of truth and core browsing does not require project-owned hosted infrastructure;
- transfer/download destinations are brokered rather than granting unrestricted filesystem authority;
- imported configuration and trace data are treated as untrusted input;
- developer Network Lab traces are sensitive and require redaction/bounded handling;
- compatibility mitigations must remain origin/version scoped and reviewable;
- private-session behavior must not silently persist data that is documented as ephemeral;
- normal shutdown must remain explicit and clean rather than treating crashes as success.

## Browser and platform boundaries

### Untrusted web content

All page content is untrusted, including pages intentionally opened by the user. Web content must not gain browser-domain authority merely because it executes inside a renderer process.

### Local browser data

History, session state, bookmarks, workspaces, permissions, downloads metadata, traces and configuration can contain sensitive information. Access to those surfaces should remain explicit and narrowly scoped.

### Downloads and filesystem access

Downloaded files and remote metadata are untrusted. File writes must continue through approved destinations and brokered filesystem policy rather than broad arbitrary-path access.

### Developer tooling

Network Lab, HAR/trace exports and compatibility artifacts can contain URLs, cookies, headers, request bodies, internal hostnames and tokens. They must remain bounded, redacted by default where applicable and local unless the user explicitly exports or shares them.

### Compatibility infrastructure

Site-specific compatibility workarounds must not silently weaken privacy or security globally. Mitigations should remain scoped, versioned, reviewable and covered by regression tests where practical.

### Packaged Windows builds

The portable Windows package is validated in CI for artifact integrity, required runtime files, sandbox prerequisites, startup behavior, product lifecycle and clean shutdown. Passing package smoke tests is a release requirement, but it is not a substitute for security review of browser-boundary changes.

## Out of scope for security claims

Openbrowser does not currently claim:

- anonymity;
- resistance to a fully compromised operating system;
- protection from malicious software already running with the user's privileges;
- complete sandbox equivalence with upstream Chrome across every platform;
- that experimental privacy controls eliminate protocol-level metadata exposure.

## Security-sensitive changes

Changes involving any of the following deserve explicit security review:

- renderer/browser-process boundaries;
- CEF/Chromium version or sandbox configuration;
- permissions/capability policy;
- profile/private-session persistence;
- filesystem or download access;
- Network Lab or trace handling;
- compatibility mitigations;
- provider/sync/network egress;
- release/package/bootstrap logic;
- shutdown/lifecycle behavior that could mask crashes.

See `docs/threat-model.md` for the detailed evolving threat model.
