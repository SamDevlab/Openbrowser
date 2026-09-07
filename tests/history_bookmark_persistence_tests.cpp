#include "core/bookmarks/bookmark_manager.h"
#include "core/history/history_manager.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

void TestHistoryPersistence() {
    using namespace openbrowser::core;
    HistoryManager manager;

    manager.RecordVisit("https://example.com", "Example Domain", "ws-work");
    manager.RecordVisit("https://github.com/SamDevlab/Openbrowser", "Openbrowser Repo");
    manager.RecordVisit("https://example.com", "Example Domain - Visited Again", "ws-work");

    Require(manager.TotalEntries() == 2, "2 unique URL+workspace entries");

    // Serialize
    const std::string json = manager.Serialize();
    Require(json.find("\"schema_version\": 1") != std::string::npos, "Has schema version");
    Require(json.find("https://example.com") != std::string::npos, "Has example.com");
    Require(json.find("ws-work") != std::string::npos, "Has workspace_id");

    // Deserialize into fresh manager
    HistoryManager restored;
    Require(restored.Deserialize(json), "Deserialized history JSON");
    Require(restored.TotalEntries() == 2, "Restored 2 entries");

    const auto search = restored.Search("Openbrowser");
    Require(search.size() == 1, "Found Openbrowser repo in restored history");
    Require(search[0].url == "https://github.com/SamDevlab/Openbrowser", "URL matches");

    // File roundtrip test
    const auto temp_path = std::filesystem::temp_directory_path() / "test_history_store.json";
    Require(manager.SaveToFile(temp_path), "Saved history to file");

    HistoryManager from_file;
    Require(from_file.LoadFromFile(temp_path), "Loaded history from file");
    Require(from_file.TotalEntries() == 2, "Total entries from file match");

    std::filesystem::remove(temp_path);
}

void TestBookmarkPersistence() {
    using namespace openbrowser::core;
    BookmarkManager manager;

    manager.AddBookmark({
        .id = "bm-1",
        .url = "https://developer.mozilla.org",
        .title = "MDN Web Docs",
        .tags = {"docs", "web", "js"},
        .workspace_id = "dev"
    });
    manager.AddBookmark({
        .id = "bm-2",
        .url = "https://news.ycombinator.com",
        .title = "Hacker News",
        .tags = {"news", "tech"},
        .workspace_id = std::nullopt
    });

    Require(manager.TotalBookmarks() == 2, "2 bookmarks added");

    // Serialize
    const std::string json = manager.Serialize();
    Require(json.find("MDN Web Docs") != std::string::npos, "Has MDN title");
    Require(json.find("\"tags\": [\"docs\", \"web\", \"js\"]") != std::string::npos, "Has tags array");
    Require(json.find("news.ycombinator.com") != std::string::npos, "Has Hacker News URL");

    // Deserialize into fresh manager
    BookmarkManager restored;
    Require(restored.Deserialize(json), "Deserialized bookmark JSON");
    Require(restored.TotalBookmarks() == 2, "Restored 2 bookmarks");
    Require(restored.IsBookmarked("https://developer.mozilla.org"), "MDN is bookmarked");
    Require(restored.IsBookmarked("https://news.ycombinator.com"), "HN is bookmarked");

    const auto* bm = restored.FindBookmark("bm-1");
    Require(bm != nullptr, "Found bookmark bm-1");
    Require(bm->tags.size() == 3, "Bookmark bm-1 has 3 tags");
    Require(bm->workspace_id.has_value() && *bm->workspace_id == "dev", "Workspace is dev");

    // File roundtrip test
    const auto temp_path = std::filesystem::temp_directory_path() / "test_bookmarks_store.json";
    Require(manager.SaveToFile(temp_path), "Saved bookmarks to file");

    BookmarkManager from_file;
    Require(from_file.LoadFromFile(temp_path), "Loaded bookmarks from file");
    Require(from_file.TotalBookmarks() == 2, "Total bookmarks from file match");

    std::filesystem::remove(temp_path);
}

} // namespace

int main() {
    std::cout << "Running History & Bookmark Persistence Tests..." << std::endl;

    TestHistoryPersistence();
    TestBookmarkPersistence();

    std::cout << "All History & Bookmark Persistence Tests passed successfully." << std::endl;
    return 0;
}
