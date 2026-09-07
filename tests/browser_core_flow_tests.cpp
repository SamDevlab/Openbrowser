#include "core/commands/action_registry.h"
#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/session/session_privacy_orchestrator.h"
#include "core/tabs/tab.h"
#include "engine/browser_engine.h"

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

class MockBrowserEngine final : public openbrowser::engine::BrowserEngine {
public:
    void SetEventSink(openbrowser::engine::BrowserEngineEventSink*) noexcept override {}
    void CreateTab(const openbrowser::core::Tab& tab) override {
        created_tabs.push_back(tab);
    }
    void CloseTab(const openbrowser::core::TabId& tab_id) override {
        closed_tabs.push_back(tab_id);
    }
    void ActivateTab(const openbrowser::core::TabId& tab_id) override {
        activated_tabs.push_back(tab_id);
    }
    void Navigate(const openbrowser::engine::NavigationRequest& req) override {
        navigated_urls.push_back(req.url);
    }
    void GoBack(const openbrowser::core::TabId& tab_id) override {
        back_calls.push_back(tab_id);
    }
    void GoForward(const openbrowser::core::TabId& tab_id) override {
        forward_calls.push_back(tab_id);
    }
    void Reload(const openbrowser::core::TabId& tab_id) override {
        reload_calls.push_back(tab_id);
    }
    void Suspend(const openbrowser::core::TabId&) override {}
    void Resume(const openbrowser::core::TabId&) override {}

    void SetEphemeralMode(bool enabled) noexcept override {
        is_ephemeral_mode_ = enabled;
    }
    bool IsEphemeralMode() const noexcept override { return is_ephemeral_mode_; }
    void PurgeEphemeralContext() override {
        purged_ephemeral_context = true;
    }

    bool is_ephemeral_mode_{false};
    bool purged_ephemeral_context{false};
    std::vector<openbrowser::core::Tab> created_tabs;
    std::vector<openbrowser::core::TabId> closed_tabs;
    std::vector<openbrowser::core::TabId> activated_tabs;
    std::vector<std::string> navigated_urls;
    std::vector<openbrowser::core::TabId> back_calls;
    std::vector<openbrowser::core::TabId> forward_calls;
    std::vector<openbrowser::core::TabId> reload_calls;
};

void TestTabReopeningLifecycle() {
    MockBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    openbrowser::core::Tab tab_a;
    tab_a.id = "tab_a";
    tab_a.url = "https://github.com/openai";
    tab_a.title = "GitHub OpenAI";
    tab_a.lifecycle = openbrowser::core::TabLifecycle::Active;
    session.OpenTab(tab_a);

    Require(session.Tabs().size() == 1, "session has 1 tab");
    Require(session.ActiveTabId() == "tab_a", "active tab is tab_a");

    // Close tab_a
    session.CloseTab("tab_a");
    Require(session.Tabs().empty(), "tabs empty after close");
    Require(session.ClosedTabs().size() == 1, "closed_tabs has 1 record");
    Require(session.ClosedTabs()[0].url == "https://github.com/openai", "closed tab record URL matches");

    // Reopen last closed tab
    const auto reopened_id = session.ReopenLastClosedTab();
    Require(reopened_id.has_value(), "ReopenLastClosedTab returned ID");
    Require(session.Tabs().size() == 1, "session has 1 tab again");
    Require(session.Tabs()[0].url == "https://github.com/openai", "reopened tab url matches");
    Require(session.Tabs()[0].title == "GitHub OpenAI", "reopened tab title matches");
    Require(session.ClosedTabs().empty(), "closed_tabs empty after reopen");

    // LIFO ordering test
    openbrowser::core::Tab tab_b;
    tab_b.id = "tab_b";
    tab_b.url = "https://example.com/b";
    tab_b.title = "Tab B";
    session.OpenTab(tab_b);

    openbrowser::core::Tab tab_c;
    tab_c.id = "tab_c";
    tab_c.url = "https://example.com/c";
    tab_c.title = "Tab C";
    session.OpenTab(tab_c);

    // Close B then C
    session.CloseTab("tab_b");
    session.CloseTab("tab_c");
    Require(session.ClosedTabs().size() == 2, "2 closed tab records");

    // First reopen should be C (LIFO)
    const auto reopened_c = session.ReopenLastClosedTab();
    Require(reopened_c.has_value(), "reopened first LIFO");
    const auto* tab_c_restored = session.FindTab(*reopened_c);
    Require(tab_c_restored != nullptr && tab_c_restored->url == "https://example.com/c", "first reopened is C");

    // Second reopen should be B
    const auto reopened_b = session.ReopenLastClosedTab();
    Require(reopened_b.has_value(), "reopened second LIFO");
    const auto* tab_b_restored = session.FindTab(*reopened_b);
    Require(tab_b_restored != nullptr && tab_b_restored->url == "https://example.com/b", "second reopened is B");

    // Stack should now be empty
    Require(!session.ReopenLastClosedTab().has_value(), "reopen on empty stack returns nullopt");
}

void TestPrivateModeClosedTabIsolation() {
    MockBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);
    openbrowser::core::ProfileManager profile_mgr;
    openbrowser::core::SessionPrivacyOrchestrator orchestrator(session, profile_mgr, &engine);

    // 1. Open persistent tab
    openbrowser::core::Tab pers_tab;
    pers_tab.id = "pers_1";
    pers_tab.url = "https://persistent.org/";
    pers_tab.title = "Persistent";
    pers_tab.is_ephemeral = false;
    session.OpenTab(pers_tab);

    // 2. Enter private mode
    Require(orchestrator.EnterPrivateMode(), "entered private mode");

    // 3. Open ephemeral tab in private mode
    openbrowser::core::Tab eph_tab;
    eph_tab.id = "eph_1";
    eph_tab.url = "https://secret.org/dashboard";
    eph_tab.title = "Secret Dashboard";
    eph_tab.is_ephemeral = true;
    session.OpenTab(eph_tab);

    // 4. Close ephemeral tab
    session.CloseTab("eph_1");
    Require(session.ClosedTabs().size() == 1, "closed tabs has ephemeral tab");
    Require(session.ClosedTabs()[0].is_ephemeral == true, "closed tab is marked ephemeral");

    // 5. Normal-mode reopen (allow_ephemeral = false) must NOT reopen ephemeral tab
    Require(!session.ReopenLastClosedTab(false).has_value(), "reopen(allow_ephemeral=false) ignores private tabs");

    // 6. Exit private mode
    orchestrator.ExitPrivateMode();
    Require(!orchestrator.IsPrivateModeActive(), "exited private mode");

    // 7. Verify all ephemeral closed tabs were purged
    Require(session.ClosedTabs().empty(), "closed tabs purged upon private exit");
    Require(!session.ReopenLastClosedTab(true).has_value(), "cannot reopen private tab after exit");
}

void TestTabCycling() {
    MockBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    openbrowser::core::Tab t1; t1.id = "t1"; t1.url = "https://site1.com"; session.OpenTab(t1);
    openbrowser::core::Tab t2; t2.id = "t2"; t2.url = "https://site2.com"; session.OpenTab(t2);
    openbrowser::core::Tab t3; t3.id = "t3"; t3.url = "https://site3.com"; session.OpenTab(t3);

    // Make t1 active
    session.ActivateTab("t1");
    Require(session.ActiveTabId() == "t1", "initial active is t1");

    // Cycle forward: t1 -> t2 -> t3 -> t1
    Require(session.CycleTab(true), "cycle forward 1");
    Require(session.ActiveTabId() == "t2", "cycled to t2");

    Require(session.CycleTab(true), "cycle forward 2");
    Require(session.ActiveTabId() == "t3", "cycled to t3");

    Require(session.CycleTab(true), "cycle forward 3 (wrap)");
    Require(session.ActiveTabId() == "t1", "wrapped to t1");

    // Cycle backward: t1 -> t3 -> t2 -> t1
    Require(session.CycleTab(false), "cycle backward 1 (wrap)");
    Require(session.ActiveTabId() == "t3", "cycled backward to t3");

    Require(session.CycleTab(false), "cycle backward 2");
    Require(session.ActiveTabId() == "t2", "cycled backward to t2");

    // Tab cycling with visibility filter: hide t2
    session.ActivateTab("t1");
    auto filter_hide_t2 = [](const openbrowser::core::Tab& t) { return t.id != "t2"; };

    Require(session.CycleTab(true, filter_hide_t2), "cycle forward with filter");
    Require(session.ActiveTabId() == "t3", "skipped hidden t2, went straight to t3");

    Require(session.CycleTab(false, filter_hide_t2), "cycle backward with filter");
    Require(session.ActiveTabId() == "t1", "skipped hidden t2 backward, back to t1");
}

void TestActionRegistryShortcutBindings() {
    openbrowser::core::ActionRegistry registry;
    bool new_tab_called = false;
    bool close_tab_called = false;
    bool reload_called = false;

    registry.RegisterAction({
        .id = "navigation.new_tab",
        .title = "New Tab",
        .description = "Open a new browser tab",
        .category = openbrowser::core::ActionCategory::Navigation,
        .shortcut_hint = "Ctrl+T",
        .handler = [&]() { new_tab_called = true; return true; },
    });

    registry.RegisterAction({
        .id = "navigation.close_tab",
        .title = "Close Tab",
        .description = "Close the current active tab",
        .category = openbrowser::core::ActionCategory::Navigation,
        .shortcut_hint = "Ctrl+W",
        .handler = [&]() { close_tab_called = true; return true; },
    });

    registry.RegisterAction({
        .id = "navigation.reload",
        .title = "Reload Page",
        .description = "Reload the current page",
        .category = openbrowser::core::ActionCategory::Navigation,
        .shortcut_hint = "Ctrl+R",
        .handler = [&]() { reload_called = true; return true; },
    });

    Require(registry.HasAction("navigation.new_tab"), "registry has new_tab");
    Require(registry.HasAction("navigation.close_tab"), "registry has close_tab");
    Require(registry.HasAction("navigation.reload"), "registry has reload");

    const auto* act = registry.FindAction("navigation.new_tab");
    Require(act != nullptr && act->shortcut_hint == "Ctrl+T", "shortcut hint is Ctrl+T");

    Require(registry.ExecuteAction("navigation.new_tab"), "exec new_tab");
    Require(new_tab_called, "handler was called");

    Require(registry.ExecuteAction("navigation.close_tab"), "exec close_tab");
    Require(close_tab_called, "handler was called");

    Require(registry.ExecuteAction("navigation.reload"), "exec reload");
    Require(reload_called, "handler was called");
}

}  // namespace

int main() {
    TestTabReopeningLifecycle();
    TestPrivateModeClosedTabIsolation();
    TestTabCycling();
    TestActionRegistryShortcutBindings();

    if (failures != 0) {
        std::cerr << failures << " browser core flow assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser browser core flow: PASS\n";
    return 0;
}
