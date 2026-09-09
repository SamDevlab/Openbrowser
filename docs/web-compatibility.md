# Web compatibility system

## Purpose

Openbrowser intentionally differs from upstream browsers in privacy, UI, native tooling and policy. Those differences must not silently become website breakage.

The Web Compatibility System answers two separate questions:

1. **Does Openbrowser implement the Web platform correctly?**
2. **Did an intentional Openbrowser policy change break a site that works in the pinned upstream Chromium reference?**

Compatibility work is therefore treated as an engineering subsystem, not as a collection of ad-hoc user-agent hacks.

## Current implementation

The repository now contains three complementary compatibility layers.

### M8 protocol and deterministic harness

`scripts/compatibility_harness.py` owns local fixture serving, per-run isolation, runner timeouts, process-tree cleanup, normalized observations, normalized request comparison and machine-readable result classification.

Runner results use a versioned JSON protocol. A scenario is classified as one of:

- `passed`;
- `incompatible`;
- `runner-failure`;
- `timeout`;
- `runner-crash`;
- `observation-error`;
- `fixture-not-observed`.

A crash, timeout or bad shutdown is never converted into a compatibility pass.

### M8.1 real Openbrowser vs pinned Chromium

The Windows differential workflow executes the same deterministic local scenarios against:

- a native Openbrowser build using CEF `151.3.17+gf059e67+chromium-151.0.7922.138`;
- Chrome for Testing `151.0.7922.138`, pinned by exact archive SHA-256 and checked again by the executable-reported version.

Fixtures publish bounded test-only observations to a per-run localhost collector protected by an unguessable token. This avoids adding a production DOM-inspection backdoor to the browser.

The current PR-sized corpus contains 10 scenarios covering:

1. navigation;
2. redirect chains;
3. JavaScript DOM mutation and localStorage;
4. fragment/hash navigation;
5. History API state replacement;
6. same-origin JSON XHR;
7. external same-origin scripts;
8. JavaScript cookie visibility;
9. localStorage/sessionStorage lifecycle behavior;
10. core DOM creation and mutation APIs.

Observed fields can include final URL, title, DOM markers/text, deterministic fixture events, localStorage, sessionStorage, cookies and normalized fixture-server requests.

The Openbrowser side additionally requires:

- successful native launch;
- fixture observation;
- graceful close acceptance;
- process exit code `0`;
- persisted `clean_shutdown: true`.

The expanded corpus has completed with `10 passed, 0 incompatible, 0 runner failures` against the pinned Chromium reference.

See:

- [`m8.1-real-browser-differential.md`](m8.1-real-browser-differential.md)
- [`m8.1-compatibility-corpus.md`](m8.1-compatibility-corpus.md)

### M8.2 pinned WPT smoke

M8.2 adds the first upstream Web Platform Tests smoke lane.

The workflow pins:

- an exact `web-platform-tests/wpt` commit;
- Chrome for Testing `151.0.7922.138`;
- ChromeDriver `151.0.7922.138`;
- SHA-256 values for both downloaded browser archives.

The initial smoke subset covers selected DOM, Encoding, URLSearchParams and Web Storage tests. Unexpected WPT failures remain failures; broad expectations and `--no-fail-on-unexpected` are not used to hide regressions.

The WPT lane currently validates standards-test ingestion and the pinned Chromium reference environment. It does **not** yet run Openbrowser directly through `wptrunner` because Openbrowser does not expose a WebDriver-compatible product adapter.

M8.1 remains the layer that directly executes Openbrowser itself.

See [`m8.2-wpt-smoke.md`](m8.2-wpt-smoke.md).

## Local differential execution

The harness accepts one command for Openbrowser and one for the reference runner. Commands receive these environment variables:

```text
OPENBROWSER_COMPAT_SCENARIO
OPENBROWSER_COMPAT_URL
OPENBROWSER_COMPAT_RESULT_FILE
OPENBROWSER_COMPAT_STORAGE_DIR
OPENBROWSER_COMPAT_STATUS_URL
```

The `{url}`, `{result_file}`, `{storage_dir}`, `{scenario_id}` and `{status_url}` placeholders are also available in command arguments.

A generic local invocation looks like:

```text
python scripts/compatibility_harness.py \
  --manifest tests/compatibility/fixtures/manifest.json \
  --openbrowser-command "pwsh -NoProfile -File scripts/compatibility-openbrowser-driver.ps1 -Executable C:/path/to/openbrowser.exe" \
  --reference-command "<reference-browser-adapter-command>" \
  --output compatibility-report.json
```

Temporary storage and process trees are cleaned after each scenario, including timeout paths.

## Reference principle

For every shipped engine milestone, Openbrowser should have a pinned upstream Chromium/CEF reference build from the same Chromium major whenever practical.

```text
Openbrowser build
      |
      +--> deterministic scenarios
      |
      +--> standards-test evidence
      |
      +--> future rendering comparisons
      |
      `--> differential comparison
                  |
                  v
         pinned Chromium reference
```

A difference is not automatically a bug. It must be classified.

## Difference classes

Every reproducible divergence should be placed in one of these classes:

- **standards regression** — Openbrowser violates expected Web-platform behavior;
- **engine regression** — the pinned engine itself regressed relative to the prior release;
- **Openbrowser regression** — browser policy/core/adapters introduced unintended behavior;
- **intentional privacy divergence** — behavior differs deliberately for tracking/fingerprinting/storage protection;
- **intentional product divergence** — behavior differs deliberately for a documented browser feature;
- **site assumption / site bug** — the site depends on browser-specific or non-standard behavior;
- **unknown** — evidence is not yet sufficient.

Unknown differences are never automatically added as compatibility exceptions.

## Test layers

### 1. Web Platform Tests

WPT is the primary upstream standards suite.

Current state:

- **PR smoke subset** — implemented against the pinned Chromium reference;
- **direct Openbrowser wptrunner product adapter** — not implemented yet;
- **nightly broader WPT coverage** — future work;
- **release candidate matrix** — future work;
- **regression quarantine** — future work.

WPT results are evidence, not a reason to copy browser-specific bugs.

### 2. Rendering / reftest checks

Deterministic rendering/reftest comparison is not implemented yet.

When added, comparisons must record at minimum:

- engine version;
- operating system;
- display scale;
- font environment;
- feature flags;
- privacy mode/profile;
- viewport dimensions.

A pixel difference without environmental metadata is not a useful compatibility result.

### 3. Differential browser scenarios

Implemented through M8.1. The same local scenario is run against Openbrowser and the pinned Chromium reference, and stable observations are compared rather than engine-private IDs.

High-value future observations include:

- Fetch/CORS behavior;
- CSP behavior;
- IndexedDB;
- workers and service workers;
- WebSockets;
- module loading;
- permissions behavior;
- downloads;
- console errors;
- deterministic screenshots/reftests.

### 4. Site compatibility corpus

A broader curated corpus is future work. It should prefer:

- local fixtures;
- self-hosted test applications;
- open test endpoints;
- reproducible mock services.

Live third-party production sites should not be the only evidence for a regression because they can change independently of Openbrowser.

## Compatibility profiles and mitigations

Openbrowser may support narrowly scoped compatibility mitigations, but they must remain transparent and reversible.

A compatibility mitigation must have:

- a stable ID;
- affected origin/site scope;
- engine/Openbrowser version range;
- reason/evidence;
- exact behavior changed;
- privacy impact classification;
- expiration/review condition;
- regression test.

Example conceptual record:

```text
compat.example-001
scope: https://app.example.test
reason: site rejects normalized UA despite standards-compatible behavior
change: site-scoped UA compatibility token
privacy-impact: medium
expires: when site fix is verified
```

## Compatibility mode

A future per-site Compatibility Mode may temporarily relax selected Openbrowser-specific behaviors for diagnosis.

It must not mean "disable all privacy protections".

The UI should show exactly what changes, for example:

```text
Compatibility Mode for example.com

[on] standard storage behavior
[on] upstream user-agent behavior
[off] disable tracker protection
[off] disable HTTPS policy
```

High-impact privacy relaxations require separate explicit approval.

## User-agent and Client Hints

Openbrowser must avoid creating a unique fingerprint merely to identify itself to sites.

Any user-agent or Client Hints strategy should be evaluated against both:

- site compatibility;
- fingerprinting resistance.

Site-specific UA overrides are a last-resort compatibility mechanism and must be versioned/tested like every other mitigation.

## Release gates

A future release should be blocked when:

- an unexplained compatibility delta appears in a tested critical path;
- a known-good deterministic scenario regresses;
- a compatibility workaround broadens beyond its declared origin/version scope;
- privacy protections are weakened globally to fix a site-specific bug;
- Openbrowser differs from its reference build without classification in a tested critical path.

As the WPT and rendering layers mature, their critical subsets should become release-candidate gates as well.

## Storage and reporting

Compatibility results are local/CI artifacts by default.

Optional report submission, if ever implemented, must be explicit and redact browsing data. A failing site URL or capture must never be uploaded automatically merely because compatibility diagnostics are enabled.

## Non-goal

The compatibility system does not promise that every site will behave identically to Chrome. Openbrowser may intentionally differ. The goal is that differences are **known, classified, testable and narrowly controlled** rather than accidental.
