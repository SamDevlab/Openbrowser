#include "core/bookmarks/bookmark_manager.h"
#include "core/history/history_manager.h"
#include "core/navigation/address_input.h"
#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/session/session_history_bridge.h"
#include "core/session/session_persistence.h"
#include "core/settings/settings_manager.h"
#include "core/storage/atomic_file_store.h"
#include "core/storage/json_helper.h"
#include "core/workspaces/workspace_manager.h"
#include "fakes/fake_browser_engine.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
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

std::filesystem::path TempTestDir() {
    const auto dir = std::filesystem::temp_directory_path() / "ob_persistence_tests";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

// 1. JSON Round-Trip tests
void TestJsonRoundTrip() {
    using namespace openbrowser::core::storage;

    const std::vector<std::string> test_strings = {
        "plain text",
        "\"quoted title\"",
        "back\\slash and \\\\double",
        "line\nbreak\r\nand return",
        "tab\tvalue\tand\ttabs",
        "Unicode: 日本語 / português / 🚀 emoji / São Paulo",
        "https://example.com/search?q=hello%20world&filter=true#anchor",
        "",
        "tags with \"quotes\" and \\backslashes\\",
        "Control \b and \f characters",
    };

    for (const auto& original : test_strings) {
        std::string json;
        EscapeJsonString(original, json);

        const auto parsed = ParseJson(json);
        Require(parsed.has_value(), "ParseJson succeeded for: " + original);
        Require(parsed->type == JsonValue::Type::String, "Parsed type is String");
        Require(parsed->str_val == original, "Round-trip exact match for: " + original);
    }
}

// 2. Storage primitive: AtomicWriteFile and repeated overwrites
void TestAtomicOverwriteAndBackup() {
    using namespace openbrowser::core::storage;

    const auto target = TempTestDir() / "repeated_overwrite.json";
    const auto bak = TempTestDir() / "repeated_overwrite.json.bak";
    std::error_code ec;
    std::filesystem::remove(target, ec);
    std::filesystem::remove(bak, ec);

    // State A
    const std::string state_a = "{\"version\": 1, \"state\": \"A\"}";
    auto res_a = AtomicWriteFile(target, state_a, /*keep_backup=*/true);
    Require(res_a.success, "State A written");

    auto read_a = ReadFileWithBackupRecovery(target);
    Require(read_a.success, "Read State A");
    Require(read_a.source == ReadRecoverySource::Primary, "Read A from Primary");
    Require(read_a.content == state_a, "Content A matches");

    // State B
    const std::string state_b = "{\"version\": 2, \"state\": \"B\"}";
    auto res_b = AtomicWriteFile(target, state_b, /*keep_backup=*/true);
    Require(res_b.success, "State B written");

    auto read_b = ReadFileWithBackupRecovery(target);
    Require(read_b.success, "Read State B");
    Require(read_b.source == ReadRecoverySource::Primary, "Read B from Primary");
    Require(read_b.content == state_b, "Content B matches");

    // Backup should now hold State A
    Require(std::filesystem::exists(bak), "Backup exists after state B");
    auto read_bak_a = ReadFileWithBackupRecovery(bak);
    Require(read_bak_a.success && read_bak_a.content == state_a, "Backup contains State A");

    // State C
    const std::string state_c = "{\"version\": 3, \"state\": \"C\"}";
    auto res_c = AtomicWriteFile(target, state_c, /*keep_backup=*/true);
    Require(res_c.success, "State C written");

    auto read_c = ReadFileWithBackupRecovery(target);
    Require(read_c.success, "Read State C");
    Require(read_c.source == ReadRecoverySource::Primary, "Read C from Primary");
    Require(read_c.content == state_c, "Content C matches");

    // Backup should now hold State B
    auto read_bak_b = ReadFileWithBackupRecovery(bak);
    Require(read_bak_b.success && read_bak_b.content == state_b, "Backup contains State B");

    std::filesystem::remove(target, ec);
    std::filesystem::remove(bak, ec);
}

// 3. Corrupted-file behavior and Backup recovery
void TestCorruptionRecovery() {
    using namespace openbrowser::core::storage;

    const auto target = TempTestDir() / "corrupt_test.json";
    const auto bak = TempTestDir() / "corrupt_test.json.bak";
    const auto tmp = TempTestDir() / "corrupt_test.json.tmp";
    std::error_code ec;
    std::filesystem::remove(target, ec);
    std::filesystem::remove(bak, ec);
    std::filesystem::remove(tmp, ec);

    auto validator = [](std::string_view s) {
        const auto root = ParseJson(s);
        return root.has_value() && root->type == JsonValue::Type::Object;
    };

    // Case 1: Both absent
    auto res_absent = ReadFileWithBackupRecovery(target, validator);
    Require(!res_absent.success, "Both absent returns failure");
    Require(res_absent.source == ReadRecoverySource::None, "Recovery source None");

    // Setup valid file and valid backup
    const std::string valid_v1 = "{\"version\": 1, \"status\": \"valid_v1\"}";
    const std::string valid_v2 = "{\"version\": 2, \"status\": \"valid_v2\"}";
    AtomicWriteFile(target, valid_v1, true);
    AtomicWriteFile(target, valid_v2, true);

    // Case 2: Primary corrupt (truncated/invalid JSON) + backup valid
    {
        std::ofstream corrupt_out(target, std::ios::binary | std::ios::trunc);
        corrupt_out << "{\"version\": 2, \"status\": \"incompl"; // Truncated JSON
    }

    auto res_recovered = ReadFileWithBackupRecovery(target, validator);
    Require(res_recovered.success, "Recovered from corrupted primary");
    Require(res_recovered.source == ReadRecoverySource::Backup, "Recovery source is Backup");
    Require(res_recovered.content == valid_v1, "Recovered valid_v1 from backup");

    // Case 3: Primary missing + backup valid
    std::filesystem::remove(target, ec);
    auto res_missing_primary = ReadFileWithBackupRecovery(target, validator);
    Require(res_missing_primary.success, "Recovered from missing primary");
    Require(res_missing_primary.source == ReadRecoverySource::Backup, "Recovery source is Backup");
    Require(res_missing_primary.content == valid_v1, "Recovered valid_v1 from backup");

    // Case 4: Primary corrupt + backup corrupt
    {
        std::ofstream corrupt_target(target, std::ios::binary | std::ios::trunc);
        corrupt_target << "{{invalid";
        std::ofstream corrupt_bak(bak, std::ios::binary | std::ios::trunc);
        corrupt_bak << "not json";
    }
    auto res_both_corrupt = ReadFileWithBackupRecovery(target, validator);
    Require(!res_both_corrupt.success, "Both corrupt returns failure");
    Require(res_both_corrupt.source == ReadRecoverySource::None, "Recovery source None when both corrupt");

    // Case 5: Interrupted temporary write (.tmp exists, primary and bak valid)
    AtomicWriteFile(target, valid_v1, true);
    AtomicWriteFile(target, valid_v2, true);
    {
        std::ofstream broken_tmp(tmp, std::ios::binary | std::ios::trunc);
        broken_tmp << "half written data";
    }

    auto res_with_tmp = ReadFileWithBackupRecovery(target, validator);
    Require(res_with_tmp.success, "Read successful despite lingering .tmp");
    Require(res_with_tmp.source == ReadRecoverySource::Primary, "Primary untouched by .tmp");
    Require(res_with_tmp.content == valid_v2, "Primary has correct valid_v2");

    std::filesystem::remove(target, ec);
    std::filesystem::remove(bak, ec);
    std::filesystem::remove(tmp, ec);
}

// 4. History Correctness & CRUD tests
void TestHistoryCorrectnessAndCrud() {
    using namespace openbrowser::core;

    HistoryManager history;
    const auto hist_path = TempTestDir() / "history_test.json";
    std::error_code ec;
    std::filesystem::remove(hist_path, ec);

    history.SetAutoSavePath(hist_path);

    // Initial visit
    history.RecordVisit("https://example.com/page1", "Page One", "work");
    Require(history.TotalEntries() == 1, "Recorded first visit");
    Require(std::filesystem::exists(hist_path), "Auto-save wrote history to disk");

    // Repeat visit to same URL updates count, does not duplicate entry
    history.RecordVisit("https://example.com/page1", "Page One Updated", "work");
    Require(history.TotalEntries() == 1, "Still 1 entry after repeat visit");

    const auto entries = history.ListHistory();
    Require(entries[0].visit_count == 2, "Visit count is 2");
    Require(entries[0].title == "Page One Updated", "Title updated");

    // Second URL
    history.RecordVisit("https://example.com/page2", "Page Two", "work");
    Require(history.TotalEntries() == 2, "Recorded second visit");

    // Remove entry
    const auto entry_to_remove = history.Search("Page Two")[0].id;
    Require(history.RemoveEntry(entry_to_remove), "RemoveEntry succeeded for existing");
    Require(!history.RemoveEntry("non-existent-id"), "RemoveEntry returned false for missing");
    Require(history.TotalEntries() == 1, "1 entry remains after removal");

    // Verify disk persistence after mutation
    HistoryManager reloaded;
    Require(reloaded.LoadFromFile(hist_path), "Reloaded history from disk");
    Require(reloaded.TotalEntries() == 1, "Reloaded history matches memory state");
    Require(reloaded.Search("Page Two").empty(), "Removed entry stays deleted on disk");

    std::filesystem::remove(hist_path, ec);
}

// 5. SessionHistoryBridge semantics
void TestSessionHistoryBridgeSemantics() {
    using namespace openbrowser::core;

    openbrowser::tests::FakeBrowserEngine engine;
    BrowserSession session(engine);
    HistoryManager history;
    ProfileManager profiles;

    SessionHistoryBridge bridge(&history, &profiles);
    session.AddObserver(&bridge);

    // Open persistent tab A
    Tab tab_a;
    tab_a.id = "tab-a";
    tab_a.url = "https://site-a.com";
    tab_a.title = "Site A";
    tab_a.is_ephemeral = false;
    static_cast<void>(session.OpenTab(std::move(tab_a), true));
    Require(history.TotalEntries() == 1, "Initial tab open records 1 visit");
    Require(history.ListHistory()[0].visit_count == 1, "Visit count is 1");

    // Commit for A with same URL does NOT duplicate visit
    session.OnNavigationCommitted({.tab_id = "tab-a", .url = "https://site-a.com"});
    Require(history.TotalEntries() == 1, "Same URL commit does not increment visit count");
    Require(history.ListHistory()[0].visit_count == 1, "Visit count is still 1");

    // Title change for A
    session.OnTitleChanged({.tab_id = "tab-a", .title = "Site A - New Title"});
    Require(history.ListHistory()[0].visit_count == 1, "Title change does not increment visit count");

    // Open and activate tab B
    Tab tab_b;
    tab_b.id = "tab-b";
    tab_b.url = "https://site-b.com";
    tab_b.is_ephemeral = false;
    tab_b.navigation_state = NavigationState::Idle;
    static_cast<void>(session.OpenTab(std::move(tab_b), true));
    session.OnNavigationCommitted({.tab_id = "tab-b", .url = "https://site-b.com"});
    Require(history.TotalEntries() == 2, "Site B committed records second visit");

    // Switch back to tab A
    static_cast<void>(session.ActivateTab("tab-a"));
    Require(history.TotalEntries() == 2, "Activating tab A does not increment visit count");
    Require(history.Search("site-a.com")[0].visit_count == 1, "Tab A visit count still 1");

    // Navigate A -> B on tab A
    session.OnNavigationCommitted({.tab_id = "tab-a", .url = "https://site-b.com"});
    Require(history.Search("site-b.com")[0].visit_count == 2, "Navigating tab A to B records visit");

    // Navigate B -> A on tab A
    session.OnNavigationCommitted({.tab_id = "tab-a", .url = "https://site-a.com"});
    Require(history.Search("site-a.com")[0].visit_count == 2, "Navigating back to A records new visit");

    // Failed navigation does not record visit
    session.OnNavigationFailed({.tab_id = "tab-a", .url = "https://site-c.com", .error_text = "error"});
    Require(history.Search("site-c.com").empty(), "Failed navigation does not record visit");

    // Ephemeral / Private navigation
    profiles.SetActiveProfile("incognito"); // Ephemeral
    Tab tab_priv;
    tab_priv.id = "tab-priv";
    tab_priv.url = "https://secret.com";
    tab_priv.is_ephemeral = true;
    static_cast<void>(session.OpenTab(std::move(tab_priv), true));
    session.OnNavigationCommitted({.tab_id = "tab-priv", .url = "https://secret.com"});
    Require(history.Search("secret.com").empty(), "Private navigation records 0 visits");
}

// 6. Bookmark Editing & Uniqueness
void TestBookmarkEditing() {
    using namespace openbrowser::core;

    BookmarkManager bm;
    const auto bm_path = TempTestDir() / "bookmarks_edit_test.json";
    std::error_code ec;
    std::filesystem::remove(bm_path, ec);

    bm.SetAutoSavePath(bm_path);

    bm.AddBookmark({
        .id = "bm-1",
        .url = "https://alpha.org",
        .title = "Alpha",
        .tags = {"tag1"},
        .created_at_ms = 1000,
        .workspace_id = "work",
    });

    bm.AddBookmark({
        .id = "bm-2",
        .url = "https://beta.org",
        .title = "Beta",
        .tags = {"tag2"},
        .created_at_ms = 2000,
        .workspace_id = "work",
    });

    // Edit bm-1 title and tags
    Require(bm.EditBookmark("bm-1", "Alpha Updated", "https://alpha.org", std::vector<std::string>{"tag1", "new_tag"}),
            "Edit title and tags succeeded");
    const auto* item1 = bm.FindBookmark("bm-1");
    Require(item1->title == "Alpha Updated", "Title updated");
    Require(item1->tags.size() == 2, "Tags updated");
    Require(item1->created_at_ms == 1000, "Created_at preserved");
    Require(item1->workspace_id == "work", "Workspace preserved");

    // Edit bm-1 URL to conflict with bm-2
    Require(!bm.EditBookmark("bm-1", "Alpha", "https://beta.org"),
            "Cannot edit URL to duplicate existing bookmark");

    // Edit bm-1 URL to unique new URL
    Require(bm.EditBookmark("bm-1", "Alpha New", "https://alpha-new.org"),
            "Edit to unique URL succeeded");

    // Reload from disk
    BookmarkManager reloaded;
    Require(reloaded.LoadFromFile(bm_path), "Loaded bookmarks from disk");
    const auto* rel1 = reloaded.FindBookmark("bm-1");
    Require(rel1 != nullptr && rel1->url == "https://alpha-new.org", "Edited bookmark persisted on disk");

    std::filesystem::remove(bm_path, ec);
}

// 7. Workspace Persistence & Active Workspace
void TestWorkspacePersistence() {
    using namespace openbrowser::core;

    WorkspaceManager wm;
    const auto ws_path = TempTestDir() / "workspaces_test.json";
    std::error_code ec;
    std::filesystem::remove(ws_path, ec);

    wm.SetAutoSavePath(ws_path);

    wm.CreateWorkspace(Workspace{
        .id = "research",
        .name = "Research Lab",
        .badge_color = "#FF5722",
        .is_ephemeral = false,
    });

    Require(wm.SetActiveWorkspace("research"), "Set active workspace to research");
    Require(wm.ActiveWorkspaceId() == "research", "Active workspace is research");

    // Reload into fresh manager
    WorkspaceManager reloaded;
    Require(reloaded.LoadFromFile(ws_path), "Loaded workspaces from disk");
    Require(reloaded.HasWorkspace("research"), "Research workspace persisted");
    Require(reloaded.ActiveWorkspaceId() == "research", "Active workspace restored as research");

    // Fallback if saved active workspace is missing
    const std::string invalid_active_json = "{\"schema_version\": 1, \"active_workspace_id\": \"non-existent\", \"workspaces\": [{\"id\": \"default\", \"name\": \"Default\", \"badge_color\": \"#3B82F6\", \"is_ephemeral\": false}]}";
    WorkspaceManager fallback_wm;
    Require(fallback_wm.Deserialize(invalid_active_json), "Deserialized workspace json");
    Require(fallback_wm.ActiveWorkspaceId() == "default", "Invalid active workspace falls back to default");

    std::filesystem::remove(ws_path, ec);
}

// 8. Settings Persistence & Active Consumption
void TestSettingsPersistenceAndConsumption() {
    using namespace openbrowser::core;

    SettingsManager sm;
    const auto set_path = TempTestDir() / "settings_test.json";
    std::error_code ec;
    std::filesystem::remove(set_path, ec);

    sm.SetAutoSavePath(set_path);

    sm.SetSearchProvider("Google", "https://www.google.com/search?q=%s");
    sm.SetHomePageUrl("https://news.ycombinator.com");
    sm.SetRestoreSessionOnStartup(false);
    sm.SetDownloadsDirectory("C:/Downloads/Custom");

    // Verify active consumption in navigation::ResolveAddressInput
    const auto resolved = navigation::ResolveAddressInput("search query", {
        .name = sm.Settings().search_provider_name,
        .search_url_template = sm.Settings().search_url_template,
    });
    Require(resolved.has_value() && resolved->starts_with("https://www.google.com/search?q="),
            "Settings search provider actively used by ResolveAddressInput");

    // Reload from disk
    SettingsManager reloaded;
    Require(reloaded.LoadFromFile(set_path), "Reloaded settings from disk");
    Require(reloaded.Settings().search_provider_name == "Google", "Search provider persisted");
    Require(reloaded.Settings().home_page_url == "https://news.ycombinator.com", "Home page persisted");
    Require(!reloaded.Settings().restore_session_on_startup, "Restore session setting persisted");
    Require(reloaded.Settings().downloads_directory == "C:/Downloads/Custom", "Downloads dir persisted");

    std::filesystem::remove(set_path, ec);
}

// 9. Crash / Unclean exit and Private Mode isolation test
void TestCrashAndPrivateModeIsolation() {
    using namespace openbrowser::core;

    const auto storage_dir = TempTestDir() / "crash_test_storage";
    std::error_code ec;
    std::filesystem::create_directories(storage_dir, ec);

    const auto session_path = storage_dir / "session.json";
    const auto history_path = storage_dir / "history.json";
    const auto bookmark_path = storage_dir / "bookmarks.json";

    std::filesystem::remove(session_path, ec);
    std::filesystem::remove(history_path, ec);
    std::filesystem::remove(bookmark_path, ec);

    openbrowser::tests::FakeBrowserEngine engine;
    BrowserSession session(engine);
    FocusQueue focus_queue;
    ProfileManager profiles;
    HistoryManager history;
    BookmarkManager bookmarks;

    history.SetAutoSavePath(history_path);
    bookmarks.SetAutoSavePath(bookmark_path);

    SessionHistoryBridge bridge(&history, &profiles, [&](bool is_clean) {
        const auto snap = SessionPersistence::CaptureSnapshot(session, focus_queue, is_clean);
        SessionPersistence::SaveToFile(session_path, snap);
    });
    session.AddObserver(&bridge);

    // Normal browsing: visit Site A and bookmark Site B
    Tab tab_a;
    tab_a.id = "tab-1";
    tab_a.url = "https://public-a.com";
    tab_a.title = "Public A";
    tab_a.is_ephemeral = false;
    static_cast<void>(session.OpenTab(std::move(tab_a), true));
    session.OnNavigationCommitted({.tab_id = "tab-1", .url = "https://public-a.com"});

    bookmarks.AddBookmark({
        .id = "bm-b",
        .url = "https://bookmark-b.com",
        .title = "Bookmark B",
    });

    // Enter Private Mode
    profiles.SetActiveProfile("incognito");
    Tab tab_priv;
    tab_priv.id = "tab-secret";
    tab_priv.url = "https://secret-vault.com";
    tab_priv.title = "Secret";
    tab_priv.is_ephemeral = true;
    static_cast<void>(session.OpenTab(std::move(tab_priv), true));
    session.OnNavigationCommitted({.tab_id = "tab-secret", .url = "https://secret-vault.com"});

    // Simulate exit / crash while still in Private Mode (NO clean shutdown executed)
    // Reload everything fresh from disk
    HistoryManager reloaded_history;
    BookmarkManager reloaded_bookmarks;
    reloaded_history.LoadFromFile(history_path);
    reloaded_bookmarks.LoadFromFile(bookmark_path);
    const auto reloaded_session = SessionPersistence::LoadFromFile(session_path);

    // Assertions:
    // 1. Public activity survived
    Require(reloaded_history.Search("public-a.com").size() == 1, "Public visit survived crash");
    Require(reloaded_bookmarks.IsBookmarked("https://bookmark-b.com"), "Bookmark B survived crash");
    Require(reloaded_session.has_value(), "Session snapshot exists");
    Require(reloaded_session->tabs.size() == 1, "Session has 1 persistent tab");
    Require(reloaded_session->tabs[0].url == "https://public-a.com", "Session tab is Public A");

    // 2. Private activity never touched disk
    Require(reloaded_history.Search("secret-vault.com").empty(), "Private visit NEVER reached history disk");
    for (const auto& tab : reloaded_session->tabs) {
        Require(tab.url != "https://secret-vault.com", "Private tab NEVER reached session disk");
    }

    std::filesystem::remove_all(storage_dir, ec);
}

}  // namespace

int main() {
    std::cout << "Running Local Persistence Reliability Tests (MVP-2)...\n";

    TestJsonRoundTrip();
    TestAtomicOverwriteAndBackup();
    TestCorruptionRecovery();
    TestHistoryCorrectnessAndCrud();
    TestSessionHistoryBridgeSemantics();
    TestBookmarkEditing();
    TestWorkspacePersistence();
    TestSettingsPersistenceAndConsumption();
    TestCrashAndPrivateModeIsolation();

    if (failures != 0) {
        std::cerr << "Local Persistence Tests FAILED with " << failures << " failures.\n";
        return 1;
    }

    std::cout << "All Local Persistence Reliability Tests PASSED!\n";
    return 0;
}
