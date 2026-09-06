# Desktop shell

This directory will host the desktop application shell and rendering-engine integration.

The shell may depend on the concrete Chromium-family adapter. `src/core/` may not.

First bring-up target:

1. create a native window;
2. instantiate an engine adapter;
3. create one Openbrowser `Tab` and map it to one renderer instance;
4. normalize navigation/lifecycle events back into application commands;
5. add horizontal/vertical tab presentations over the same tab model.

Focus Queue UI is intentionally a later presentation over the independent core queue model, not a replacement for vertical tabs.
