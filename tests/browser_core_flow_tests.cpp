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

void TestPrivacyAwareCloseTab() {
    MockBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);
    openbrowser::core::ProfileManager profile_mgr;
    openbrowser::core::SessionPrivacyOrchestrator orchestrator(session, profile_mgr, &engine);

    // 1. Persistent tab A
    openbrowser::core::Tab pers_a;
    pers_a.id = "pers_a";
    pers_a.url = "https://persistent.org/a";
    pers_a.title = "Persistent A";
    pers_a.is_ephemeral = false;
    session.OpenTab(pers_a);
    Require(session.ActiveTabId() == "pers_a", "active is pers_a");

    // 2. Enter private mode -> creates private-tab-1, active
    Require(orchestrator.EnterPrivateMode(), "enter private mode");
    Require(orchestrator.IsPrivateModeActive(), "private mode is active");
    const auto priv_1_id = session.ActiveTabId();
    Require(priv_1_id.has_value(), "private tab is active");
    const auto* priv_1 = session.FindTab(*priv_1_id);
    Require(priv_1 != nullptr && priv_1->is_ephemeral, "active tab is ephemeral");

    engine.activated_tabs.clear();

    // 3. Close the active private tab using orchestrator.CloseActiveTab()
    Require(orchestrator.CloseActiveTab(), "CloseActiveTab in private mode succeeds");

    // Persistent A must NEVER have been activated!
    for (const auto& activated : engine.activated_tabs) {
        Require(activated != "pers_a", "engine never activated persistent tab in private mode");
    }
    Require(session.ActiveTabId() != "pers_a", "persistent A is not active in session");

    // A new private tab must have been spawned and made active
    const auto new_priv_id = session.ActiveTabId();
    Require(new_priv_id.has_value(), "new private tab active after closing lone private tab");
    const auto* new_priv = session.FindTab(*new_priv_id);
    Require(new_priv != nullptr && new_priv->is_ephemeral, "new active tab is ephemeral");

    // 4. Test multiple private tabs: persistent A, private B, private C active
    openbrowser::core::Tab priv_c;
    priv_c.id = "priv_c";
    priv_c.url = "https://secret.org/c";
    priv_c.title = "Secret C";
    priv_c.is_ephemeral = true;
    session.OpenTab(priv_c, true);
    Require(session.ActiveTabId() == "priv_c", "priv_c is active");

    engine.activated_tabs.clear();

    // Close priv_c -> previous ephemeral tab must become active, persistent A must never be activated
    Require(orchestrator.CloseActiveTab(), "close priv_c");
    for (const auto& activated : engine.activated_tabs) {
        Require(activated != "pers_a", "engine never activated persistent tab when closing C");
    }
    Require(session.ActiveTabId() != "pers_a", "persistent A not active");
    const auto* active_after_c = session.FindTab(*session.ActiveTabId());
    Require(active_after_c != nullptr && active_after_c->is_ephemeral, "ephemeral tab active after C closed");

    // Clean up
    orchestrator.ExitPrivateMode();
    Require(session.ActiveTabId() == "pers_a", "persistent A restored on exit private mode");
}

void TestPrivateModeClosedTabIsolation() {
    MockBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);
    openbrowser::core::ProfileManager profile_mgr;
    openbrowser::core::SessionPrivacyOrchestrator orchestrator(session, profile_mgr, &engine);

    // 1. Open and close persistent tab A
    openbrowser::core::Tab pers_tab;
    pers_tab.id = "pers_1";
    pers_tab.url = "https://persistent.org/";
    pers_tab.title = "Persistent";
    pers_tab.is_ephemeral = false;
    session.OpenTab(pers_tab);
    session.CloseTab("pers_1");
    Require(session.ClosedTabs().size() == 1, "closed tabs has persistent tab");

    // 2. Enter private mode
    Require(orchestrator.EnterPrivateMode(), "entered private mode");

    // 3. Reopen closed tab while in private mode:
    // orchestrator.ReopenClosedTab() requests EphemeralOnly; persistent tab must NOT be reopened!
    Require(!orchestrator.ReopenClosedTab().has_value(), "persistent tab is NOT reopened in private mode");
    Require(!session.ReopenLastClosedTab(openbrowser::core::ClosedTabMode::EphemeralOnly).has_value(),
            "EphemeralOnly rejects persistent closed tab");

    // 4. Open ephemeral tab in private mode and close it
    openbrowser::core::Tab eph_tab;
    eph_tab.id = "eph_1";
    eph_tab.url = "https://secret.org/dashboard";
    eph_tab.title = "Secret Dashboard";
    eph_tab.is_ephemeral = true;
    session.OpenTab(eph_tab);
    orchestrator.CloseTab("eph_1");

    // 5. Reopen in private mode -> ephemeral tab B MUST be reopened!
    const auto reopened_eph = orchestrator.ReopenClosedTab();
    Require(reopened_eph.has_value(), "ephemeral tab reopened in private mode");
    const auto* eph_restored = session.FindTab(*reopened_eph);
    Require(eph_restored != nullptr && eph_restored->is_ephemeral, "restored tab is ephemeral");
    Require(eph_restored != nullptr && eph_restored->url == "https://secret.org/dashboard", "restored tab url matches");

    // Re-close ephemeral tab
    orchestrator.CloseTab(*reopened_eph);

    // 6. Exit private mode
    orchestrator.ExitPrivateMode();
    Require(!orchestrator.IsPrivateModeActive(), "exited private mode");

    // 7. Verify all ephemeral closed tabs were purged
    for (const auto& closed : session.ClosedTabs()) {
        Require(!closed.is_ephemeral, "no ephemeral closed tab records after exit");
    }

    // 8. Normal mode reopen restores the persistent tab A
    const auto reopened_pers = orchestrator.ReopenClosedTab();
    Require(reopened_pers.has_value(), "persistent tab reopened in normal mode");
    const auto* pers_restored = session.FindTab(*reopened_pers);
    Require(pers_restored != nullptr && !pers_restored->is_ephemeral, "restored tab is persistent");
    Require(pers_restored != nullptr && pers_restored->url == "https://persistent.org/", "persistent url matches");
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

void TestTabCyclingWithWorkspaceFilter() {
    MockBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    openbrowser::core::Tab t1;
    t1.id = "t1";
    t1.url = "https://def1.com";
    t1.workspace_id = "default";
    session.OpenTab(t1);

    openbrowser::core::Tab t2;
    t2.id = "t2";
    t2.url = "https://work1.com";
    t2.workspace_id = "work";
    session.OpenTab(t2);

    openbrowser::core::Tab t3;
    t3.id = "t3";
    t3.url = "https://def2.com";
    t3.workspace_id = "default";
    session.OpenTab(t3);

    const std::string active_ws = "default";
    auto ws_filter = [&](const openbrowser::core::Tab& t) {
        return t.workspace_id.has_value() && *t.workspace_id == active_ws;
    };

    session.ActivateTab("t1");
    Require(session.ActiveTabId() == "t1", "initial active is t1");

    Require(session.CycleTab(true, ws_filter), "cycle forward in default ws");
    Require(session.ActiveTabId() == "t3", "cycled to t3, skipped t2");

    Require(session.CycleTab(true, ws_filter), "cycle forward wrap");
    Require(session.ActiveTabId() == "t1", "wrapped back to t1");

    Require(session.CycleTab(false, ws_filter), "cycle backward wrap");
    Require(session.ActiveTabId() == "t3", "cycled backward to t3");
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
    TestPrivacyAwareCloseTab();
    TestPrivateModeClosedTabIsolation();
    TestTabCycling();
    TestTabCyclingWithWorkspaceFilter();
    TestActionRegistryShortcutBindings();

    if (failures != 0) {
        std::cerr << failures << " browser core flow assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser browser core flow: PASS\n";
    return 0;
}
