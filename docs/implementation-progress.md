# Implementation progress

Openbrowser is pre-alpha. This file records short-lived implementation state while the first desktop shell is being brought up.

## M1 runtime integration

The current CEF runtime branch introduces the first concrete `BrowserEngine` adapter while preserving the engine-independent core.

Implemented in source:

- one top-level CEF Views window with a shared browser host;
- one `CefBrowserView` surface per Openbrowser `TabId`;
- active-surface switching without creating competing tab state in CEF;
- normalized navigation, title and renderer-crash events into `BrowserSession`;
- asynchronous-safe tab creation where logical state exists before engine callbacks can arrive;
- optional Network Lab Level 1 resource observation behind `--network-lab`;
- Openbrowser-owned request trace IDs instead of durable CEF IDs;
- Linux and Windows entrypoints following CEF multi-process/sandbox bootstrap patterns;
- an explicit CEF smoke workflow that downloads only the exact pinned stable distribution and verifies it against the checksum published by the official CEF build service before compiling;
- address bar and navigation chrome controls;
- dual tab projection (horizontal `TabStrip` + vertical `FocusSidebar` over `BrowserSession`);
- session persistence, atomic disk snapshots, and clean shutdown crash detection;
- Network Lab request observation panel drawer toggleable from chrome and `--network-lab`;
- capability enforcement at the engine and network boundary (`CapabilityPolicy`, `OnBeforeBrowse`, `OnBeforeResourceLoad`).

## Deliberate limitations

The current M1 shell is not yet a usable daily browser:

- dynamic capability consent UI prompts ("Ask" decisions) implemented in M2.1;
- desktop runtime support is not considered validated until the dedicated CEF smoke build passes against the pinned distribution.

## M2 implementation state

- **M2.1 Delivered**: Interactive capability permission prompts (`CefPermissionHandler`, `OnRequestMediaAccessPermission`, `OnShowPermissionPrompt`) with responsive `[Allow] [Block] [Dismiss]` banners, and Security Badge UI (`[ 🔒 Secure ]`, `[ ⚠ Insecure ]`) with origin permission inspection and reset.
- **M2.2 Delivered**: Focus session timer (Pomodoro/Sprint state machine) and attention dwell metrics (`FocusSprint`, `AttentionMetrics`, `FocusSessionController::RecordSprintTick`), integrated into `FocusSidebar` with responsive UI timer controls (`[Start]` / `[Pause]` / `[Resume]`, `[Reset]`, dwell percentage tracking).
- **M2.3 Delivered**: Advanced Network Lab inspection, method and status filtering (`[Method: ALL/GET/POST]`, `[Status: ALL/2XX/ERR]`), request & response header separation, body preview extraction from `CefPostData`, and in-drawer request inspector with back navigation.
- **M2.4 Delivered**: Renderer discard and memory pressure lifecycle policies (`TabDiscardPolicy`, `MemoryPressureLevel`), deterministic LRU activation sequence tracking, seamless on-demand tab revival upon activation, and `💤` power-saving indicators in `TabStrip`.
- **M2.5 Delivered**: Workspace isolation for cookies and storage partitions (`Workspace`, `WorkspaceManager`, `CefRequestContext::CreateContext`, partition cache path resolution, workspace capability rule resolution, `[Work]` tab strip badges, and interactive `📁 <workspace>` switcher).

## Milestone M2 Completion

All five M2 milestones have been delivered, tested across unit test suites and the CEF desktop adapter smoke workflow:
1. M2.1: Capability Consent UI & Security Badge
2. M2.2: Focus Session Timer & Attention Metrics
3. M2.3: Advanced Network Lab Inspection & Body Previews
4. M2.4: Renderer Discard & Memory Pressure Lifecycle
5. M2.5: Workspace Isolation & Partitioned Request Contexts

## Milestone M3: Native Subsystems & Protection Engine

- **M3.1 Delivered**: Native Transfer Manager & Download Broker (`TransferBroker`, `TransferItem`, `TransferObserver`, `CefDownloadHandler` integration in `CefTabClient`, thread-safe progress, speed calculation, pause, resume, and cancellation).
- **M3.2 Delivered**: Native Content Filtering & Tracker Protection Engine (`ContentFilter`, domain and pattern matching, tracker categorizations including Advertising, Analytics, Fingerprinting, Cryptomining, Social, allowlists, and `OnBeforeResourceLoad` pre-flight enforcement).
- **M3.3 Delivered**: Native History & Bookmarks Engine (`HistoryManager`, `BookmarkManager`, visit deduplication, recency/visit count ranking, tag filtering, and workspace partition isolation).
- **M3.4 Delivered**: Browser-Owned Network Attribution & HAR Diagnostic Export (`NetworkAttribution` tagging across Page, UpdateCheck, FilterListSync, Telemetry, and Transfer, and standard HAR 1.2 JSON export via `NetworkTraceBuffer::ExportToHar`).

## Milestone M4: Platform Operations, Sync Ports & Transfer Sandboxing

- **M4.1 Delivered**: Transfer Destination Sandboxing & Safe File Broker (`FileBroker`, directory containment checks, path traversal defenses, Windows/POSIX filename sanitization, executable risk assessment, and automatic collision resolution).
- **M4.2 Delivered**: Native Command Palette & Keyboard Action Dispatcher (`ActionRegistry`, `CommandPalette`, categorized actions across navigation, focus, network lab, and settings, with fuzzy query scoring and ranking).
- **M4.3 Delivered**: Native Sync Ports & Local Storage Provider (`SyncPort`, `LocalFilesystemSyncProvider`, version-based conflict resolution, manifest synchronization, and atomic JSON store serialization).
- **M4.4 Delivered**: Web Compatibility Scenario Harness & Reftest Engine (`CompatibilityScenario`, `CompatibilityRunner`, deterministic scenario evaluations, header expectation checking, tracking-script blockage verification, and compatibility reports).

## Milestone M5: Privacy Profiles, Compatibility Mitigations, Adblock Parser & Network Lab Waterfall

- **M5.1 Delivered**: Ephemeral Profiles & Isolated Memory Vault (`ProfileManager`, `ProfileConfig`, persistent disk profiles vs strictly in-memory ephemeral/incognito profiles, zero-disk persistence guarantee, in-memory vault, and secure memory wipe on purge).
- **M5.2 Delivered**: Site-Scoped Compatibility Mitigations & Anti-Fingerprinting UA Engine (`CompatibilityMitigationRegistry`, origin-scoped transparent relaxation flags, and `UserAgentPolicyEngine` providing Standard Chromium, Anti-Fingerprint Uniform, and site-scoped override modes).
- **M5.3 Delivered**: Structured Network Lab Filter Query Parser & Waterfall Phase Timeline (`NetworkFilterQuery` supporting status comparisons/ranges, method, host, state, attribution, and free-text queries, and `WaterfallTimeline` computing timing phase segments and ASCII visualization).
- **M5.4 Delivered**: Native Adblock / EasyList Rule Parser & Rule List Compiler (`AdblockRuleParser` parsing EasyList/ABP syntax with domain anchors `||`, resource options `$script`, `$third-party`, allowlist rules `@@`, and high-speed compilation into `ContentFilter`).

## Milestone M6: Desktop Chrome Expansion & Integration

- **M6.1 Delivered**: Desktop Command Palette Overlay & Search Dialog (`CommandPaletteOverlay` UI connected to `CommandPalette` and `ActionRegistry`, live result scoring, category indicators, keyboard navigation, and action dispatch).
- **M6.2 Delivered**: Desktop Bookmarks Bar & Favorites Quick Launcher (`BookmarksBar` horizontal bar component below the navigation chrome, single-click active tab navigation, workspace-scoped bookmark list, and `[+ Bookmark]` quick addition).
- **M6.3 Delivered**: Downloads & Transfer Progress Drawer (`DownloadsPanel` drawer observing `TransferBroker` and `FileBroker`, tracking transfer state, progress percentages, download rates in KB/s, and pause/resume/cancel controls).
- **M6.4 Delivered**: Profiles & Incognito Session Launcher & M5 Subsystem Wiring (wired `ProfileManager`, `CompatibilityMitigationRegistry`, and `UserAgentPolicyEngine` in `DesktopApp`, interactive profile chip `[👤 Default]` / `[🕶 Private]`, and quick actions in `ActionRegistry`).

## Milestone M7: Developer Network Lab — Deep Diagnostics & Trace Export

- **M7.1 Delivered**: Connection & TLS Diagnostics Model (`ConnectionDiagnostics`, `TlsInfo`, `ConnectionRegistry` — Level 2 connection diagnostics with `ConnectionId`, remote endpoint, TLS version/cipher/ALPN, DNS/connect/TLS timing, protocol negotiation, and connection reuse tracking. `NetworkEvent` and `NetworkRequestSummary` extended with optional `connection_id` cross-reference. New event types: `DnsResolved`, `ConnectionEstablished`, `TlsHandshaked`).
- **M7.2 Delivered**: Privacy / Filter Decision Log & Explanation Engine (`FilterDecisionLog`, `FilterDecisionRecord`, `FilterDecisionSink` — per-request policy explanation with layer (`ContentFilter`, `CapabilityPolicy`, `WorkspacePolicy`, `UserAgentPolicy`), rule source, scope, and human explanation string. `ContentFilter::EvaluateWithId` emits decisions. `NetworkFilterQuery` extended with `blocked:true/false` and `decision:block/allow` structured filter clauses).
- **M7.3 Delivered**: Local `.obtrace` Trace Recorder & Versioned File Format (`ObtraceRecorder` subscribing to `NetworkTraceBuffer`, streaming versioned NDJSON events to disk with `trace_start` / `trace_end` sentinels; `NetworkTraceBuffer::ExportToObtrace` static batch export method; full sensitive header redaction preserved).
- **M7.4 Delivered**: Desktop Network Lab Enhanced Views (`NetworkLabPanel` extended with three-tab design — `[▶ Requests]` / `[▶ Connections]` / `[▶ Decisions]` — surfacing `ConnectionRegistry` and `FilterDecisionLog` data; two new ActionRegistry entries: `devtools.export_har` (`Ctrl+Shift+H`) and `devtools.export_obtrace` (`Ctrl+Shift+O`); `DesktopApp` wired with all M7 subsystems).

