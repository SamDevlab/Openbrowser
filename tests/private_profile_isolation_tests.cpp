#include "core/bookmarks/bookmark_manager.h"
#include "core/focus_queue/focus_queue.h"
#include "core/history/history_manager.h"
#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/session/session_history_bridge.h"
#include "core/session/session_persistence.h"
#include "core/tabs/tab.h"
#include "devtools/network/obtrace_recorder.h"
#include "engine/browser_engine.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

class DummyEngine final : public openbrowser::engine::BrowserEngine {
public:
    void SetEventSink(openbrowser::engine::BrowserEngineEventSink*) noexcept override {}
    void CreateTab(const openbrowser::core::Tab&) override {}
    void CloseTab(const openbrowser::core::TabId&) override {}
    void ActivateTab(const openbrowser::core::TabId&) override {}
    void Navigate(const openbrowser::engine::NavigationRequest&) override {}
    void GoBack(const openbrowser::core::TabId&) override {}
    void GoForward(const openbrowser::core::TabId&) override {}
    void Reload(const openbrowser::core::TabId&) override {}
    void Suspend(const openbrowser::core::TabId&) override {}
    void Resume(const openbrowser::core::TabId&) override {}
};

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

} // namespace

int main() {
    std::cout << "Running Private Profile Isolation Negative Tests..." << std::endl;

    TestSessionPersistenceExcludesEphemeralTabs();
    TestSessionHistoryBridgeSuppressesPrivateVisits();
    TestObtraceRecorderRejectsPrivateRecording();
    TestBookmarksUnchangedByPrivateBrowsing();
    TestEphemeralProfilePurge();

    std::cout << "All Private Profile Isolation Negative Tests passed successfully." << std::endl;
    return 0;
}
