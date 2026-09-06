# Initial threat model

This document starts before the renderer integration on purpose. Security boundaries are architecture inputs, not cleanup work for a later release.

## Assets

Openbrowser may eventually handle:

- browsing history and session state;
- cookies and site storage;
- passwords, passkeys and autofill data;
- downloaded files;
- Focus Queue and workspace context;
- custom scripts and CSS;
- sync/configuration data;
- network and proxy settings;
- developer network traces and captures;
- compatibility regression artifacts and site-specific mitigation rules.

## Trust boundaries

### Untrusted web content

All page content is untrusted, including pages the user visits intentionally. Renderer compromise must not imply unrestricted access to browser state, vault data or the filesystem.

### Extensions, userscripts and native tools

Extension-like capabilities are not trusted merely because they ship with the browser. Native tools and user scripts receive explicit capabilities and narrow data access.

### Remote providers

Sync, filter, update, DNS and registry providers are external trust domains. A provider compromise must not grant arbitrary access to unrelated local browser state.

### Imported configuration

Configuration packs are untrusted input. Imports require schema validation and a preview/diff. Executable content such as userscripts requires separate review and capability approval.

### Transfers

Torrent metadata, peers, trackers, HTTP response metadata and downloaded files are untrusted. Transfer engines should run with narrow network/filesystem permissions and write through a brokered destination policy.

### Developer network traces

Network traces are sensitive even when captured only from the browser. They can contain authentication headers, cookies, API payloads, internal hostnames, URLs, personal data and security tokens.

Network Lab capture therefore has its own trust boundary:

- capture is off/bounded by default;
- secrets are redacted by default;
- trace files are user-approved local artifacts;
- imported trace files are untrusted input;
- optional raw packet-capture helpers do not gain browser-vault authority.

### Compatibility mitigations

Compatibility rules can alter observable browser behavior and therefore can weaken privacy or security if over-broad.

A compatibility mitigation must be scoped, versioned, reviewable and covered by a regression test. A single site breakage cannot justify silently disabling protections globally.

## Initial abuse cases

1. A new component silently adds telemetry or analytics egress.
   - Mitigation: browser-owned network categories require explicit capabilities; deny unknown capabilities.

2. A hosted sync implementation becomes a hard dependency.
   - Mitigation: local state remains source of truth; sync is a provider behind a port.

3. A renderer ID leaks into domain state and prevents engine replacement.
   - Mitigation: stable Openbrowser IDs are owned by core; adapters maintain renderer-handle mappings.

4. A configuration export leaks authentication state.
   - Mitigation: secret classes are excluded by default; export schemas use explicit allowlists.

5. A malicious configuration pack changes privacy policy or installs executable scripts invisibly.
   - Mitigation: import diff, separate high-risk sections and explicit approval.

6. A BitTorrent component obtains unrestricted filesystem access.
   - Mitigation: isolated worker and file broker; approved destination capability only.

7. Customization weakens fingerprinting protections by leaking local UI choices to web-visible surfaces.
   - Mitigation: keep browser chrome customization separate from normalized web-exposed properties; test the boundary.

8. Network Lab records credentials or session secrets and persists them indefinitely.
   - Mitigation: sensitive-header redaction, bounded body capture, bounded in-memory buffers, explicit recording/export and stricter private-profile retention.

9. A malicious trace file exploits the developer-tool parser.
   - Mitigation: treat trace imports as untrusted, use versioned schemas, bounds checks, parser fuzzing and no implicit execution of embedded content.

10. A packet-capture feature expands into a permanently privileged background process.
    - Mitigation: raw capture is optional, explicitly started, visibly active, time/size bounded and isolated from browser vault/profile authority.

11. A site-specific compatibility workaround weakens privacy/security for unrelated sites.
    - Mitigation: origin/version scoping, privacy-impact classification, expiration/review condition and automated regression coverage.

12. Compatibility telemetry uploads a user's failing URL or network trace automatically.
    - Mitigation: compatibility artifacts remain local by default; any report submission is explicit and redacted.

## Security invariants

- Browser-owned egress is attributable to a declared capability.
- Unknown capabilities deny by default.
- Core browsing functions do not require project-owned infrastructure.
- Renderer compromise is assumed possible and must not grant vault or unrestricted filesystem authority.
- Imported configuration cannot silently import secrets or executable behavior.
- Disabling sync cannot delete or invalidate local source-of-truth state.
- Enabling Network Lab does not grant unrestricted access to vault/autofill secrets.
- Raw packet capture, if implemented, is never required for normal browser operation.
- Developer traces are not silently synchronized, uploaded or included in configuration exports.
- A compatibility exception cannot silently broaden beyond its declared origin/version scope.
- Site compatibility does not justify globally disabling privacy/security policy without an explicit architectural decision.

## Not a claim of anonymity

Openbrowser privacy controls do not make every protocol anonymous. Peer-to-peer protocols can expose network addresses to peers, and external services have their own data handling. The implementation must expose those boundaries rather than imply otherwise.
