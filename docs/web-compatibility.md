# Web compatibility system

## Purpose

Openbrowser intends to diverge from upstream browser behavior in privacy, UI, native tooling and policy. Those differences must not silently become web-site breakage.

The future Web Compatibility System exists to answer two separate questions:

1. **Does Openbrowser implement the Web platform correctly?**
2. **Did an intentional Openbrowser policy change break a site that works in the pinned upstream Chromium reference?**

Compatibility work is therefore treated as an engineering subsystem, not as a collection of ad-hoc user-agent hacks.

## Reference principle

For every shipped engine milestone, Openbrowser should have a pinned upstream Chromium/CEF reference build from the same Chromium major whenever practical.

```text
Openbrowser build
      |
      +--> standards tests
      |
      +--> deterministic site scenarios
      |
      +--> rendering comparisons
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

Openbrowser should consume Web Platform Tests (WPT) as the primary cross-browser standards suite.

Planned execution tiers:

- **PR smoke subset** — fast tests around areas touched by the change;
- **nightly compatibility run** — broader WPT coverage;
- **release candidate matrix** — pinned Openbrowser and reference browser results;
- **regression quarantine** — known upstream failures are recorded separately from Openbrowser-specific failures.

WPT results are evidence, not a reason to copy browser-specific bugs.

### 2. Rendering / reftest checks

Where deterministic rendering matters, Openbrowser should support reference-image or reference-page comparisons.

Comparisons must record:

- engine version;
- operating system;
- display scale;
- font environment;
- feature flags;
- privacy mode/profile;
- viewport dimensions.

A pixel difference without environmental metadata is not a useful compatibility result.

### 3. Differential browser scenarios

A deterministic scenario runner should execute the same scenario against Openbrowser and a pinned Chromium reference.

Candidate observations include:

- navigation and redirect chain;
- final committed URL;
- DOM-visible results;
- cookies/storage outcomes;
- permissions behavior;
- request/response sequence;
- service-worker involvement;
- console errors;
- downloads;
- renderer crashes/hangs;
- screenshots/reftests where deterministic.

The runner should compare normalized observations, not engine-private IDs.

### 4. Site compatibility corpus

Openbrowser may maintain a curated set of deterministic site scenarios for high-impact or historically fragile applications.

The corpus should prefer:

- local fixtures;
- self-hosted test applications;
- open test endpoints;
- reproducible mock services.

Live third-party production sites should not be the only evidence for a regression because they can change independently of Openbrowser.

## Compatibility profiles and mitigations

Openbrowser may eventually support narrowly scoped compatibility mitigations, but they must be transparent and reversible.

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

- an unexplained compatibility delta appears in critical WPT areas;
- a known-good deterministic site scenario regresses;
- a compatibility workaround broadens beyond its declared origin/version scope;
- privacy protections are weakened globally to fix a site-specific bug;
- Openbrowser differs from its reference build without classification in a tested critical path.

## Storage and reporting

Compatibility results should be local artifacts by default.

Optional report submission, if ever implemented, must be explicit and redact browsing data. A failing site URL or capture must never be uploaded automatically merely because compatibility diagnostics are enabled.

## Non-goal

The compatibility system does not promise that every site will behave identically to Chrome. Openbrowser may intentionally differ. The goal is that differences are **known, classified, testable and narrowly controlled** rather than accidental.
