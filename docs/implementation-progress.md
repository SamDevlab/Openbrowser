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



