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

## Remaining M2 roadmap

1. Renderer discard and memory pressure lifecycle policies;
2. Workspace isolation for cookies and storage partitions.

