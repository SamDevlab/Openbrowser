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

- `Suspend` is currently a visibility/resource hint, not renderer discard;
- macOS core remains tested, but the CEF desktop target is intentionally blocked until the required helper-app bundle layout is implemented correctly;
- dynamic capability consent UI prompts ("Ask" decisions) will be expanded in M2;
- desktop runtime support is not considered validated until the dedicated CEF smoke build passes against the pinned distribution.

## Next steps (M2 roadmap)

1. Interactive capability permission prompts and security badge UI;
2. Focus session timer and attention metrics;
3. Advanced Network Lab inspection (body preview, header filtering);
4. Renderer discard and memory pressure lifecycle policies;
5. Workspace isolation for cookies and storage partitions.
