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
- network and proxy settings.

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

## Security invariants

- Browser-owned egress is attributable to a declared capability.
- Unknown capabilities deny by default.
- Core browsing functions do not require project-owned infrastructure.
- Renderer compromise is assumed possible and must not grant vault or unrestricted filesystem authority.
- Imported configuration cannot silently import secrets or executable behavior.
- Disabling sync cannot delete or invalidate local source-of-truth state.

## Not a claim of anonymity

Openbrowser privacy controls do not make every protocol anonymous. Peer-to-peer protocols can expose network addresses to peers, and external services have their own data handling. The implementation must expose those boundaries rather than imply otherwise.
