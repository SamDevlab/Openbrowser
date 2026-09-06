#include "core/session/browser_session.h"
#include "fakes/fake_browser_engine.h"

#include <iostream>
#include <optional>
#include <string>

int main() {
    openbrowser::tests::FakeBrowserEngine engine;
    engine.emit_initial_navigation_during_create = true;

    openbrowser::core::BrowserSession session(engine);
    const bool opened = session.OpenTab({
        .id = "reentrant",
        .url = "https://example.test/",
        .title = "Reentrant",
        .lifecycle = openbrowser::core::TabLifecycle::Background,
        .workspace_id = std::nullopt,
    });

    if (!opened) {
        std::cerr << "FAIL: synchronous-callback tab did not open\n";
        return 1;
    }

    const auto* tab = session.FindTab("reentrant");
    if (tab == nullptr) {
        std::cerr << "FAIL: tab was not registered before engine callback\n";
        return 1;
    }

    if (tab->url != "https://example.test/" ||
        tab->navigation_state != openbrowser::core::NavigationState::Idle ||
        tab->pending_url.has_value()) {
        std::cerr << "FAIL: synchronous engine commit was lost during CreateTab\n";
        return 1;
    }

    std::cout << "Openbrowser reentrant engine callback invariant: PASS\n";
    return 0;
}
