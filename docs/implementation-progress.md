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
- an explicit CEF smoke workflow that downloads only the exact pinned stable distribution and verifies the upstream SHA-1 before compiling.

## Deliberate limitations

The current M1 shell is not yet a usable daily browser:

- the initial page is selected with `--url=<url>`; an address bar is next;
- tab chrome is not implemented yet;
- `Suspend` is currently a visibility/resource hint, not renderer discard;
- Network Lab has a trace core and CEF request bridge but no developer UI yet;
- macOS core remains tested, but the CEF desktop target is intentionally blocked until the required helper-app bundle layout is implemented correctly;
- desktop runtime support is not considered validated until the dedicated CEF smoke build passes against the pinned distribution.

## Next after runtime compile validation

1. address bar and navigation commands;
2. horizontal tab projection;
3. vertical tab projection over the same `BrowserSession` state;
4. first Network Lab request table;
5. session persistence/crash recovery contract;
6. capability enforcement at the engine/network boundary.
