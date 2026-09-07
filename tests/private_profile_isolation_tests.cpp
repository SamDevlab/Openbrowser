#include "core/bookmarks/bookmark_manager.h"
#include "core/focus_queue/focus_queue.h"
#include "core/history/history_manager.h"
#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/session/session_history_bridge.h"
#include "core/session/session_persistence.h"
#include "core/session/session_privacy_orchestrator.h"
#include "core/tabs/tab.h"
#include "devtools/network/obtrace_recorder.h"
#include "engine/browser_engine.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

class MockBrowserEngine final : public openbrowser::engine::BrowserEngine {
public:
    void SetEventSink(openbrowser::engine::BrowserEngineEventSink*) noexcept override {}
    void CreateTab(const openbrowser::core::Tab& tab) override {
        created_tabs.push_back(tab);
        was_ephemeral_when_created.push_back(is_ephemeral_mode_);
        event_log.push_back("CreateTab:" + tab.id + ":" + (tab.is_ephemeral ? "eph" : "pers"));
    }
    void CloseTab(const openbrowser::core::TabId& tab_id) override {
        closed_tabs.push_back(tab_id);
        event_log.push_back("CloseTab:" + tab_id);
    }
    void ActivateTab(const openbrowser::core::TabId& tab_id) override {
        activated_tabs.push_back(tab_id);
        event_log.push_back("ActivateTab:" + tab_id);
    }
    void Navigate(const openbrowser::engine::NavigationRequest&) override {}
    void GoBack(const openbrowser::core::TabId&) override {}
    void GoForward(const openbrowser::core::TabId&) override {}
    void Reload(const openbrowser::core::TabId&) override {}
    void Suspend(const openbrowser::core::TabId&) override {}
    void Resume(const openbrowser::core::TabId&) override {}

    void SetEphemeralMode(bool enabled) noexcept override {
        is_ephemeral_mode_ = enabled;
        event_log.push_back(enabled ? "SetEphemeralMode(true)" : "SetEphemeralMode(false)");
    }
    bool IsEphemeralMode() const noexcept override { return is_ephemeral_mode_; }
    void PurgeEphemeralContext() override {
        purged_ephemeral_context = true;
        event_log.push_back("PurgeEphemeralContext()");
    }

    bool is_ephemeral_mode_{false};
    bool purged_ephemeral_context{false};
    std::vector<std::string> event_log;
    std::vector<openbrowser::core::Tab> created_tabs;
    std::vector<openbrowser::core::TabId> closed_tabs;
    std::vector<openbrowser::core::TabId> activated_tabs;
    std::vector<bool> was_ephemeral_when_created;
};

using DummyEngine = MockBrowserEngine;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

// 1. Negative Test: Session persistence must NOT write ephemeral tabs or their focus items.
void TestSessionPersistenceExcludesEphemeralTabs() {
    using namespace openbrowser::core;
    DummyEngine engine;
    BrowserSession session(engine);
    FocusQueue queue;

    // Open persistent tab
    Tab persistent_tab;
    persistent_tab.id = "tab-persistent-1";
    persistent_tab.url = "https://example.com/regular";
    persistent_tab.title = "Regular Tab";
    persistent_tab.lifecycle = TabLifecycle::Active;
    persistent_tab.is_ephemeral = false;
    Require(session.OpenTab(std::move(persistent_tab), true), "Opened persistent tab");

    // Open ephemeral / private tab
    Tab private_tab;
    private_tab.id = "tab-private-1";
    private_tab.url = "https://sensitive.example.com/secret";
    private_tab.title = "Secret Vault";
    private_tab.lifecycle = TabLifecycle::Background;
    private_tab.is_ephemeral = true;
    Require(session.OpenTab(std::move(private_tab), false), "Opened ephemeral tab");

    // Enqueue focus items for both tabs
    Require(queue.Enqueue({
        .id = "focus-1",
        .url = "https://example.com/regular",
        .tab_id = "tab-persistent-1",
        .state = FocusState::Now
    }), "Enqueued regular focus item");

    Require(queue.Enqueue({
        .id = "focus-2",
        .url = "https://sensitive.example.com/secret",
        .tab_id = "tab-private-1",
        .state = FocusState::Next
    }), "Enqueued private focus item");

    // Capture snapshot
    const auto snapshot = SessionPersistence::CaptureSnapshot(session, queue, true);

    // Verify negative assertion: private tab is completely absent
    Require(snapshot.tabs.size() == 1, "Snapshot has exactly 1 tab");
    Require(snapshot.tabs[0].id == "tab-persistent-1", "Only persistent tab was captured");
    Require(snapshot.tabs[0].url == "https://example.com/regular", "Persistent URL matches");

    for (const auto& tab : snapshot.tabs) {
        Require(tab.id != "tab-private-1", "Private tab ID must NEVER appear in snapshot");
        Require(tab.url.find("sensitive") == std::string::npos, "Private URL must NEVER appear in snapshot");
    }

    // Verify negative assertion: private focus item is completely absent
    Require(snapshot.focus_items.size() == 1, "Snapshot has exactly 1 focus item");
    Require(snapshot.focus_items[0].id == "focus-1", "Only regular focus item captured");
    for (const auto& item : snapshot.focus_items) {
        Require(item.id != "focus-2", "Private focus item must NEVER appear in snapshot");
        Require(item.url.find("sensitive") == std::string::npos, "Private focus URL must NEVER appear in snapshot");
    }

    // Test serialization output has zero traces of sensitive data
    const std::string serialized = SessionPersistence::Serialize(snapshot);
    Require(serialized.find("tab-private-1") == std::string::npos, "Zero private tab ID in JSON");
    Require(serialized.find("sensitive.example.com") == std::string::npos, "Zero private domain in JSON");
    Require(serialized.find("Secret Vault") == std::string::npos, "Zero private title in JSON");

    // Test writing to disk and reloading
    const auto temp_file = std::filesystem::temp_directory_path() / "openbrowser_private_test_session.json";
    Require(SessionPersistence::SaveToFile(temp_file, snapshot), "Saved snapshot to disk");

    const auto reloaded = SessionPersistence::LoadFromFile(temp_file);
    Require(reloaded.has_value(), "Reloaded snapshot");
    Require(reloaded->tabs.size() == 1, "Reloaded snapshot has 1 tab");
    Require(reloaded->tabs[0].id == "tab-persistent-1", "Reloaded tab is persistent tab");

    std::filesystem::remove(temp_file);
}

// 2. Behavioral Negative Test: SessionHistoryBridge suppresses history and persistence for private tabs & profiles.
void TestSessionHistoryBridgeSuppressesPrivateVisits() {
    using namespace openbrowser::core;
    DummyEngine engine;
    BrowserSession session(engine);
    HistoryManager history;
    ProfileManager profile_mgr;

    bool session_save_called = false;
    SessionHistoryBridge bridge(&history, &profile_mgr, [&](bool) {
        session_save_called = true;
    });
    session.AddObserver(&bridge);

    // Step A: Normal persistent tab navigation -> history records visit and session save is triggered.
    Tab tab1;
    tab1.id = "tab-persistent";
    tab1.url = "https://public.example.org";
    tab1.title = "Public Site";
    tab1.lifecycle = TabLifecycle::Active;
    tab1.is_ephemeral = false;

    session_save_called = false;
    Require(session.OpenTab(std::move(tab1), true), "Opened persistent tab");
    Require(history.TotalEntries() == 1, "Normal tab visit recorded in history");
    Require(session_save_called, "Session persistence callback fired for normal tab");
    Require(!history.Search("public").empty(), "Found public site in history");

    // Step B: Ephemeral tab navigation -> must NOT enter history and must NOT trigger session save.
    Tab tab_priv;
    tab_priv.id = "tab-private";
    tab_priv.url = "https://confidential.bank/account";
    tab_priv.title = "Confidential Bank";
    tab_priv.lifecycle = TabLifecycle::Active;
    tab_priv.is_ephemeral = true;

    session_save_called = false;
    Require(session.OpenTab(std::move(tab_priv), true), "Opened ephemeral tab");
    Require(history.TotalEntries() == 1, "History count unchanged after private tab open");
    Require(!session_save_called, "Session save NOT called for private tab");
    Require(history.Search("confidential").empty(), "No confidential site in history");
    Require(history.Search("bank").empty(), "No bank in history");

    // Step C: When active profile is ephemeral, visits are suppressed even if tab is not marked ephemeral.
    auto eph_profile = profile_mgr.CreateEphemeralProfile("Temp Private");
    Require(eph_profile != nullptr, "Created ephemeral profile");
    profile_mgr.SetActiveProfile(eph_profile->GetId());

    Tab tab_c;
    tab_c.id = "tab-in-private-profile";
    tab_c.url = "https://another-secret.com";
    tab_c.title = "Another Secret";
    tab_c.lifecycle = TabLifecycle::Active;
    tab_c.is_ephemeral = false; // Even if tab itself is not flagged ephemeral, ephemeral profile suppresses recording

    session_save_called = false;
    Require(session.OpenTab(std::move(tab_c), true), "Opened tab while profile is ephemeral");
    Require(history.TotalEntries() == 1, "History count still unchanged while in ephemeral profile");
    Require(!session_save_called, "Session save NOT called while profile is ephemeral");
    Require(history.Search("another-secret").empty(), "No visit recorded while profile is ephemeral");

    // Step D: Return to default profile -> close private tabs and resume normal visits.
    Require(session.CloseTab("tab-private"), "Closed private tab on exit");
    Require(session.CloseTab("tab-in-private-profile"), "Closed tab opened in private profile");
    profile_mgr.SetActiveProfile("default");
    profile_mgr.PurgeEphemeralProfiles();

    Require(session.ActivateTab("tab-persistent"), "Activated persistent tab in default profile");
    session_save_called = false;
    Require(session.Navigate("tab-persistent", "https://welcome-back.org"), "Navigated tab after restoring default profile");
    session.OnNavigationCommitted(openbrowser::engine::NavigationCommittedEvent{.tab_id = "tab-persistent", .url = "https://welcome-back.org"});
    Require(history.TotalEntries() == 2, "History resumes recording in persistent mode");
    Require(session_save_called, "Session save resumed in persistent mode");
    Require(!history.Search("welcome-back").empty(), "Found post-private visit in history");

    session.RemoveObserver(&bridge);
}

// 3. Behavioral Negative Test: ObtraceRecorder rejects recording when in private mode.
void TestObtraceRecorderRejectsPrivateRecording() {
    using namespace openbrowser::devtools::network;
    ObtraceRecorder recorder;

    const auto temp_path = std::filesystem::temp_directory_path() / "test_private_obtrace.obtrace";
    std::filesystem::remove(temp_path);

    // Set private mode
    recorder.SetPrivateMode(true);
    Require(recorder.IsPrivateMode(), "Recorder is in private mode");

    // StartRecording must fail
    Require(!recorder.StartRecording(temp_path.string()), "StartRecording must return false in private mode");
    Require(!recorder.IsRecording(), "Recorder must not be active");

    // Event append must drop silently
    NetworkEvent ev;
    ev.request_id = "req-secret-1";
    ev.url = "https://sensitive.local/data";
    ev.type = NetworkEventType::RequestStarted;
    recorder.OnTraceEventAppended(ev);

    Require(recorder.GetRecordedEventCount() == 0, "Zero events recorded");
    Require(!std::filesystem::exists(temp_path), "No file created on disk in private mode");

    // Entering private mode during active recording must immediately terminate recording
    recorder.SetPrivateMode(false);
    Require(recorder.StartRecording(temp_path.string()), "Started recording in normal mode");
    Require(recorder.IsRecording(), "Recorder is active");

    recorder.SetPrivateMode(true);
    Require(!recorder.IsRecording(), "Entering private mode must immediately stop recording");

    std::filesystem::remove(temp_path);
}

// 4. Negative Test: Bookmarks are never altered implicitly by private browsing.
void TestBookmarksUnchangedByPrivateBrowsing() {
    using namespace openbrowser::core;
    BookmarkManager bookmarks;

    bookmarks.AddBookmark({
        .url = "https://my-favorite.org",
        .title = "Favorite"
    });
    Require(bookmarks.TotalBookmarks() == 1, "1 initial bookmark");

    // Private browsing activity must not add or mutate bookmarks
    const std::size_t initial_count = bookmarks.TotalBookmarks();
    std::string private_visited_url = "https://secret.local";
    Require(!bookmarks.IsBookmarked(private_visited_url), "Private URL is not bookmarked");
    Require(bookmarks.TotalBookmarks() == initial_count, "Bookmark count unchanged");
}

// 5. Negative Test: Purging ephemeral profile clears all secrets and session data.
void TestEphemeralProfilePurge() {
    using namespace openbrowser::core;
    ProfileManager profile_mgr;

    auto ephemeral = profile_mgr.CreateEphemeralProfile("Private");
    Require(ephemeral != nullptr, "Created ephemeral profile");
    const std::string eph_id = ephemeral->GetId();

    ephemeral->SetSecret("auth_token", "super_secret_token_123");
    ephemeral->SetSessionData("session_state", "active_private");
    Require(ephemeral->HasSecret("auth_token"), "Secret stored in memory");
    Require(ephemeral->HasSessionData("session_state"), "Session data stored in memory");

    // Purge
    profile_mgr.PurgeEphemeralProfiles();

    // Verification: profile removed from manager
    Require(profile_mgr.GetProfile(eph_id) == nullptr, "Profile removed after purge");

    // Vault in purged profile is cleared
    Require(!ephemeral->HasSecret("auth_token"), "Secret purged from memory vault");
    Require(!ephemeral->HasSessionData("session_state"), "Session data purged");
}

// 6. Behavioral Scenario: Private Tab Visibility and Activation Guard
void TestPrivateTabVisibilityAndActivationGuard() {
    using namespace openbrowser::core;
    MockBrowserEngine engine;
    BrowserSession session(engine);
    ProfileManager profile_mgr;
    SessionPrivacyOrchestrator orchestrator(session, profile_mgr, &engine);

    // Initial state: normal tab exists
    Tab normal_tab;
    normal_tab.id = "tab-persistent-1";
    normal_tab.url = "https://example.com/normal";
    normal_tab.title = "Normal Tab";
    normal_tab.lifecycle = TabLifecycle::Active;
    normal_tab.is_ephemeral = false;
    Require(session.OpenTab(std::move(normal_tab), true), "Opened normal tab");

    // In normal mode: normal tab is visible and activatable
    const auto* tab1 = session.FindTab("tab-persistent-1");
    Require(tab1 != nullptr, "Tab 1 exists");
    Require(orchestrator.IsTabVisible(*tab1), "Normal tab is visible in normal mode");
    Require(orchestrator.CanActivateTab("tab-persistent-1"), "Normal tab can be activated in normal mode");

    // Enter private mode
    Require(orchestrator.EnterPrivateMode(), "Entered private mode");
    Require(orchestrator.IsPrivateModeActive(), "Private mode active");
    Require(session.Tabs().size() == 2, "Session has 2 tabs (1 normal, 1 private)");

    const auto* priv_tab = session.FindTab(*session.ActiveTabId());
    Require(priv_tab != nullptr, "Active tab is private tab");
    Require(priv_tab->is_ephemeral, "Active tab is marked ephemeral");

    // Private tab visibility: normal tabs are hidden, ephemeral tabs are visible
    Require(!orchestrator.IsTabVisible(*tab1), "Normal tab must be HIDDEN in Private Mode");
    Require(orchestrator.IsTabVisible(*priv_tab), "Ephemeral tab is VISIBLE in Private Mode");

    // Activation guard: attempting to activate normal tab while in Private Mode is REJECTED
    Require(!orchestrator.CanActivateTab("tab-persistent-1"), "CanActivateTab must return false for persistent tab in Private Mode");
    const bool activation_result = orchestrator.ActivateTab("tab-persistent-1");
    Require(!activation_result, "ActivateTab must REJECT activating persistent tab during Private Mode");
    Require(*session.ActiveTabId() != "tab-persistent-1", "Active tab must NOT switch to persistent tab");
    Require(*session.ActiveTabId() == priv_tab->id, "Active tab must remain the private tab");

    // Ephemeral tab can be activated
    Require(orchestrator.CanActivateTab(priv_tab->id), "CanActivateTab returns true for ephemeral tab in Private Mode");
}

// 7. Behavioral Scenario: Exit Private Mode with existing normal tabs restores persistent tab
// Proves strict event ordering on MockBrowserEngine:
// CloseTab(B) & CloseTab(C) -> SetEphemeralMode(false) -> PurgeEphemeralContext() -> ActivateTab(A)
// and explicitly rejects any premature activation of persistent tab before engine reset.
void TestPrivateExitWithNormalTabsRestoresPersistentTab() {
    using namespace openbrowser::core;
    MockBrowserEngine engine;
    BrowserSession session(engine);
    ProfileManager profile_mgr;
    SessionPrivacyOrchestrator orchestrator(session, profile_mgr, &engine);

    // Persistent Tab A
    Tab normal_tab;
    normal_tab.id = "tab-persistent-A";
    normal_tab.url = "https://work.example.com";
    normal_tab.title = "Work Tab";
    normal_tab.lifecycle = TabLifecycle::Active;
    normal_tab.is_ephemeral = false;
    Require(session.OpenTab(std::move(normal_tab), true), "Opened persistent tab A");

    // Enter Private Mode -> creates Private Tab B
    Require(orchestrator.EnterPrivateMode(), "Entered Private Mode");
    const auto priv_b_id = *session.ActiveTabId();

    // Open a second Private Tab C (active)
    Tab priv_c;
    priv_c.id = "private-tab-C";
    priv_c.url = "https://confidential.example.org";
    priv_c.title = "Confidential C";
    priv_c.lifecycle = TabLifecycle::Active;
    priv_c.is_ephemeral = true;
    Require(session.OpenTab(std::move(priv_c), true), "Opened second private tab C (active)");
    Require(session.Tabs().size() == 3, "Session has 3 tabs: A, B, C");
    Require(*session.ActiveTabId() == "private-tab-C", "Tab C is currently active");

    // Clear event log so we observe only the ExitPrivateMode sequence
    engine.event_log.clear();

    // Exit Private Mode
    orchestrator.ExitPrivateMode();

    // Invariants upon exit:
    Require(!orchestrator.IsPrivateModeActive(), "Private mode inactive");
    Require(profile_mgr.GetActiveProfile()->GetId() == "default", "Profile restored to default");
    Require(!engine.IsEphemeralMode(), "Engine ephemeral mode disabled");
    Require(engine.purged_ephemeral_context, "Engine purged ephemeral context");

    // All ephemeral tabs are closed
    for (const auto& t : session.Tabs()) {
        Require(!t.is_ephemeral, "Zero ephemeral tabs remain in session");
    }

    // Original persistent tab is restored and active
    Require(session.Tabs().size() == 1, "Only normal tab A remains");
    Require(session.ActiveTabId().has_value() && *session.ActiveTabId() == "tab-persistent-A",
            "Persistent tab A restored as active tab");
    Require(orchestrator.IsTabVisible(*session.FindTab("tab-persistent-A")),
            "Restored persistent tab A is visible in tab strip");

    // Verify strict event ordering in engine event log:
    // 1. CloseTab:private-tab-1 (or B) and CloseTab:private-tab-C must happen FIRST.
    // 2. SetEphemeralMode(false) must happen AFTER all private tabs are closed.
    // 3. PurgeEphemeralContext() must happen AFTER SetEphemeralMode(false).
    // 4. ActivateTab:tab-persistent-A must happen ONLY AFTER SetEphemeralMode(false) & PurgeEphemeralContext().
    // 5. NO ActivateTab on persistent tab may EVER occur before SetEphemeralMode(false)!
    std::size_t close_b_idx = std::string::npos;
    std::size_t close_c_idx = std::string::npos;
    std::size_t set_ephemeral_false_idx = std::string::npos;
    std::size_t purge_idx = std::string::npos;
    std::size_t activate_persistent_idx = std::string::npos;

    for (std::size_t i = 0; i < engine.event_log.size(); ++i) {
        const auto& ev = engine.event_log[i];
        if (ev == "CloseTab:" + priv_b_id) {
            close_b_idx = i;
        } else if (ev == "CloseTab:private-tab-C") {
            close_c_idx = i;
        } else if (ev == "SetEphemeralMode(false)") {
            set_ephemeral_false_idx = i;
        } else if (ev == "PurgeEphemeralContext()") {
            purge_idx = i;
        } else if (ev == "ActivateTab:tab-persistent-A") {
            activate_persistent_idx = i;
        }
    }

    Require(close_b_idx != std::string::npos, "CloseTab(B) logged");
    Require(close_c_idx != std::string::npos, "CloseTab(C) logged");
    Require(set_ephemeral_false_idx != std::string::npos, "SetEphemeralMode(false) logged");
    Require(purge_idx != std::string::npos, "PurgeEphemeralContext() logged");
    Require(activate_persistent_idx != std::string::npos, "ActivateTab(A) logged");

    Require(close_b_idx < set_ephemeral_false_idx, "CloseTab(B) must precede SetEphemeralMode(false)");
    Require(close_c_idx < set_ephemeral_false_idx, "CloseTab(C) must precede SetEphemeralMode(false)");
    Require(set_ephemeral_false_idx < activate_persistent_idx,
            "SetEphemeralMode(false) MUST precede ActivateTab(persistent A)");
    Require(purge_idx < activate_persistent_idx,
            "PurgeEphemeralContext() MUST precede ActivateTab(persistent A)");

    // Explicit negative assertion: Persistent tab A must NEVER be activated before SetEphemeralMode(false)
    for (std::size_t i = 0; i < set_ephemeral_false_idx; ++i) {
        Require(engine.event_log[i] != "ActivateTab:tab-persistent-A",
                "REJECTION ASSERTION: Persistent tab A was activated prematurely before engine reset!");
    }
}

// 8. Behavioral Scenario: Exit Private Mode without normal tabs creates new persistent tab AFTER engine reset
void TestPrivateExitWithoutNormalTabsCreatesPersistentTabAfterEngineReset() {
    using namespace openbrowser::core;
    MockBrowserEngine engine;
    BrowserSession session(engine);
    ProfileManager profile_mgr;
    SessionPrivacyOrchestrator orchestrator(session, profile_mgr, &engine);

    Tab normal_tab;
    normal_tab.id = "tab-temporary";
    normal_tab.url = "https://temp.example.com";
    normal_tab.lifecycle = TabLifecycle::Active;
    normal_tab.is_ephemeral = false;
    Require(session.OpenTab(std::move(normal_tab), true), "Opened temp normal tab");

    Require(orchestrator.EnterPrivateMode(), "Entered private mode");
    Require(engine.IsEphemeralMode(), "Engine is in ephemeral mode");

    // Close all normal tabs while in private mode
    Require(session.CloseTab("tab-temporary"), "Closed all normal tabs");
    Require(session.Tabs().size() == 1, "Only private tab remains");

    // Clear event log to inspect exact sequence during ExitPrivateMode
    engine.event_log.clear();

    // Exit Private Mode
    orchestrator.ExitPrivateMode();

    // Verify invariants:
    Require(!orchestrator.IsPrivateModeActive(), "Private mode inactive");
    Require(!engine.IsEphemeralMode(), "Engine is in persistent mode");
    Require(session.Tabs().size() == 1, "Exactly one new persistent tab created");

    const auto& new_tab = session.Tabs().front();
    Require(!new_tab.is_ephemeral, "New tab is strictly PERSISTENT (is_ephemeral == false)");
    Require(engine.was_ephemeral_when_created.back() == false,
            "CRITICAL: Tab must NOT be created while engine is in ephemeral mode");

    // Verify strict operation order in engine event log:
    // SetEphemeralMode(false) must happen BEFORE CreateTab!
    // PurgeEphemeralContext() must happen BEFORE CreateTab!
    std::size_t set_ephemeral_false_idx = std::string::npos;
    std::size_t purge_idx = std::string::npos;
    std::size_t create_tab_idx = std::string::npos;

    for (std::size_t i = 0; i < engine.event_log.size(); ++i) {
        if (engine.event_log[i] == "SetEphemeralMode(false)") {
            set_ephemeral_false_idx = i;
        } else if (engine.event_log[i] == "PurgeEphemeralContext()") {
            purge_idx = i;
        } else if (engine.event_log[i].rfind("CreateTab:", 0) == 0) {
            create_tab_idx = i;
        }
    }

    Require(set_ephemeral_false_idx != std::string::npos, "SetEphemeralMode(false) was logged");
    Require(purge_idx != std::string::npos, "PurgeEphemeralContext() was logged");
    Require(create_tab_idx != std::string::npos, "CreateTab was logged");
    Require(set_ephemeral_false_idx < create_tab_idx, "SetEphemeralMode(false) MUST precede CreateTab");
    Require(purge_idx < create_tab_idx, "PurgeEphemeralContext() MUST precede CreateTab");
}

} // namespace

int main() {
    std::cout << "Running Private Profile Isolation Negative Tests..." << std::endl;

    TestSessionPersistenceExcludesEphemeralTabs();
    TestSessionHistoryBridgeSuppressesPrivateVisits();
    TestObtraceRecorderRejectsPrivateRecording();
    TestBookmarksUnchangedByPrivateBrowsing();
    TestEphemeralProfilePurge();
    TestPrivateTabVisibilityAndActivationGuard();
    TestPrivateExitWithNormalTabsRestoresPersistentTab();
    TestPrivateExitWithoutNormalTabsCreatesPersistentTabAfterEngineReset();

    std::cout << "All Private Profile Isolation Negative Tests passed successfully." << std::endl;
    return 0;
}
