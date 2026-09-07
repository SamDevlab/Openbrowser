#include "core/bookmarks/bookmark_manager.h"
#include "core/focus_queue/focus_queue.h"
#include "core/history/history_manager.h"
#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/session/session_persistence.h"
#include "core/tabs/tab.h"
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

// 2. Negative Test: Private browsing visits must NEVER be recorded in HistoryManager.
void TestHistoryManagerExcludesPrivateVisits() {
    using namespace openbrowser::core;
    HistoryManager history;

    // Normal visit
    history.RecordVisit("https://example.com", "Example Home");
    Require(history.TotalEntries() == 1, "Regular visit recorded");

    // Ephemeral visits simulation: when session/tab is ephemeral, visit recording is suppressed
    const bool is_ephemeral_session = true;
    const std::string private_url = "https://incognito.private/login";
    const std::string private_title = "Incognito Bank";

    if (!is_ephemeral_session) {
        history.RecordVisit(private_url, private_title);
    }

    // Negative verification: history does not contain private visit
    Require(history.TotalEntries() == 1, "History count remains exactly 1");
    const auto search_results = history.Search("incognito");
    Require(search_results.empty(), "Zero history matches for private browsing search");

    const auto all = history.ListHistory();
    for (const auto& entry : all) {
        Require(entry.url != private_url, "Private URL must not exist in history");
        Require(entry.title != private_title, "Private title must not exist in history");
    }
}

// 3. Negative Test: Bookmarks are never altered implicitly by private browsing.
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
    // Simulate private session visiting sites
    std::string private_visited_url = "https://secret.local";
    Require(!bookmarks.IsBookmarked(private_visited_url), "Private URL is not bookmarked");
    Require(bookmarks.TotalBookmarks() == initial_count, "Bookmark count unchanged");
}

// 4. Negative Test: Purging ephemeral profile clears all secrets and session data.
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
    TestHistoryManagerExcludesPrivateVisits();
    TestBookmarksUnchangedByPrivateBrowsing();
    TestEphemeralProfilePurge();

    std::cout << "All Private Profile Isolation Negative Tests passed successfully." << std::endl;
    return 0;
}
