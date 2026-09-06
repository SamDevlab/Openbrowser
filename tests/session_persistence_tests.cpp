#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session.h"
#include "core/session/session_persistence.h"
#include "fakes/fake_browser_engine.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestSerializationRoundtrip() {
    openbrowser::core::SessionSnapshot original;
    original.schema_version = 1;
    original.clean_shutdown = false;
    original.active_tab_id = "tab-active";

    original.tabs.push_back({
        .id = "tab-1",
        .url = "https://example.com/one",
        .title = "One",
        .workspace_id = "work",
        .lifecycle = openbrowser::core::TabLifecycle::Background,
    });
    original.tabs.push_back({
        .id = "tab-active",
        .url = "https://example.com/active",
        .title = "Active Tab",
        .workspace_id = std::nullopt,
        .lifecycle = openbrowser::core::TabLifecycle::Active,
    });

    original.focus_items.push_back({
        .id = "focus-1",
        .url = "https://example.com/task",
        .tab_id = "tab-1",
        .workspace_id = "work",
        .state = openbrowser::core::FocusState::Now,
    });
    original.focus_items.push_back({
        .id = "focus-2",
        .url = "https://example.com/later",
        .tab_id = std::nullopt,
        .workspace_id = std::nullopt,
        .state = openbrowser::core::FocusState::Later,
    });

    const auto serialized = openbrowser::core::SessionPersistence::Serialize(original);
    Require(!serialized.empty(), "serialized string is not empty");

    const auto restored = openbrowser::core::SessionPersistence::Deserialize(serialized);
    Require(restored.has_value(), "deserialized snapshot is valid");
    if (restored.has_value()) {
        Require(restored->schema_version == 1, "schema version preserved");
        Require(!restored->clean_shutdown, "clean_shutdown false preserved");
        Require(restored->active_tab_id == std::optional<std::string>{"tab-active"}, "active_tab_id preserved");
        Require(restored->tabs.size() == 2, "2 tabs restored");
        Require(restored->tabs[0].id == "tab-1", "tab 1 id preserved");
        Require(restored->tabs[0].url == "https://example.com/one", "tab 1 url preserved");
        Require(restored->tabs[0].workspace_id == std::optional<std::string>{"work"}, "tab 1 workspace preserved");
        Require(restored->tabs[1].id == "tab-active", "tab active id preserved");
        Require(restored->focus_items.size() == 2, "2 focus items restored");
        Require(restored->focus_items[0].state == openbrowser::core::FocusState::Now, "focus item 1 Now state preserved");
        Require(restored->focus_items[1].state == openbrowser::core::FocusState::Later, "focus item 2 Later state preserved");
    }
}

void TestCorruptedInputHandling() {
    Require(!openbrowser::core::SessionPersistence::Deserialize("").has_value(), "empty input fails gracefully");
    Require(!openbrowser::core::SessionPersistence::Deserialize("{ not valid json").has_value(), "invalid json fails gracefully");
    Require(!openbrowser::core::SessionPersistence::Deserialize("[1, 2, 3]").has_value(), "non-object root fails gracefully");
}

void TestFilePersistenceAndCrashDetection() {
    const auto temp_dir = std::filesystem::temp_directory_path() / "openbrowser_test_session";
    std::filesystem::create_directories(temp_dir);
    const auto session_file = temp_dir / "session.json";

    openbrowser::core::SessionSnapshot snapshot;
    snapshot.schema_version = 1;
    snapshot.clean_shutdown = false;
    snapshot.active_tab_id = "t1";
    snapshot.tabs.push_back({
        .id = "t1",
        .url = "https://example.com",
        .title = "Example",
        .workspace_id = std::nullopt,
        .lifecycle = openbrowser::core::TabLifecycle::Active,
    });

    Require(openbrowser::core::SessionPersistence::SaveToFile(session_file, snapshot), "save to file succeeds");
    Require(std::filesystem::exists(session_file), "session file exists on disk");
    Require(!std::filesystem::exists(session_file.string() + ".tmp"), "temporary file is cleaned up");

    Require(!openbrowser::core::SessionPersistence::WasLastShutdownClean(session_file), "detects unclean shutdown / crash");

    snapshot.clean_shutdown = true;
    Require(openbrowser::core::SessionPersistence::SaveToFile(session_file, snapshot), "resave with clean shutdown succeeds");
    Require(openbrowser::core::SessionPersistence::WasLastShutdownClean(session_file), "detects clean shutdown");

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

void TestSessionRestoreIntegration() {
    openbrowser::tests::FakeBrowserEngine engine1;
    openbrowser::core::BrowserSession session1(engine1);
    openbrowser::core::FocusQueue queue1;

    Require(session1.OpenTab({
        .id = "tab1",
        .url = "https://tab1.test",
        .title = "Tab One",
        .lifecycle = openbrowser::core::TabLifecycle::Active,
        .workspace_id = "ws-work",
    }), "open tab 1");

    Require(session1.OpenTab({
        .id = "tab2",
        .url = "https://tab2.test",
        .title = "Tab Two",
        .lifecycle = openbrowser::core::TabLifecycle::Background,
        .workspace_id = std::nullopt,
    }, false), "open tab 2 in background");

    Require(queue1.Enqueue({
        .id = "f1",
        .url = "https://tab1.test",
        .tab_id = "tab1",
        .workspace_id = "ws-work",
        .state = openbrowser::core::FocusState::Now,
    }), "enqueue f1");

    const auto snapshot = openbrowser::core::SessionPersistence::CaptureSnapshot(session1, queue1, false);
    Require(!snapshot.clean_shutdown, "captured snapshot has clean_shutdown = false");
    Require(snapshot.tabs.size() == 2, "captured snapshot has 2 tabs");
    Require(snapshot.focus_items.size() == 1, "captured snapshot has 1 focus item");

    // Restore into fresh session
    openbrowser::tests::FakeBrowserEngine engine2;
    openbrowser::core::BrowserSession session2(engine2);
    openbrowser::core::FocusQueue queue2;

    Require(openbrowser::core::SessionPersistence::RestoreSession(session2, queue2, snapshot), "restore session succeeds");

    Require(session2.Tabs().size() == 2, "restored session has 2 tabs");
    Require(session2.ActiveTabId() == std::optional<std::string>{"tab1"}, "restored session preserves active tab");
    const auto* t2 = session2.FindTab("tab2");
    Require(t2 != nullptr && t2->lifecycle == openbrowser::core::TabLifecycle::Background, "tab 2 restored as background tab (lazy)");
    Require(queue2.Items().size() == 1, "restored queue has 1 item");
    Require(queue2.Find("f1")->tab_id == std::optional<std::string>{"tab1"}, "queue item preserved bound tab id");
}

}  // namespace

int main() {
    TestSerializationRoundtrip();
    TestCorruptedInputHandling();
    TestFilePersistenceAndCrashDetection();
    TestSessionRestoreIntegration();

    if (failures != 0) {
        std::cerr << failures << " session persistence assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser session persistence: PASS\n";
    return 0;
}
