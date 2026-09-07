#include "core/bookmarks/bookmark_manager.h"
#include "core/history/history_manager.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestHistoryRecordingAndDeduplication() {
    using namespace openbrowser::core;

    HistoryManager history;
    Require(history.TotalEntries() == 0, "Initial history should be empty");

    history.RecordVisit("https://github.com/SamDevlab/Openbrowser", "Openbrowser Repo");
    Require(history.TotalEntries() == 1, "Total entries should be 1");

    // Repeat visit to same URL
    history.RecordVisit("https://github.com/SamDevlab/Openbrowser", "Openbrowser - GitHub");
    Require(history.TotalEntries() == 1, "Deduplication should keep 1 entry");

    const auto results = history.Search("Openbrowser");
    Require(!results.empty(), "Search should find entry");
    Require(results[0].visit_count == 2, "Visit count should be 2");
    Require(results[0].title == "Openbrowser - GitHub", "Title should be updated to latest");
}

void TestHistorySearchAndRanking() {
    using namespace openbrowser::core;

    HistoryManager history;
    history.RecordVisit("https://duckduckgo.com", "DuckDuckGo Search");
    history.RecordVisit("https://news.ycombinator.com", "Hacker News");
    history.RecordVisit("https://news.ycombinator.com", "Hacker News");
    history.RecordVisit("https://news.ycombinator.com", "Hacker News");

    const auto hn_results = history.Search("news");
    Require(hn_results.size() == 1, "Search for news should yield 1 result");
    Require(hn_results[0].url == "https://news.ycombinator.com", "URL should match Hacker News");

    const auto all_results = history.Search("");
    Require(all_results.size() == 2, "Search all should yield 2 results");
    Require(all_results[0].url == "https://news.ycombinator.com", "Most visited should be ranked first");
}

void TestBookmarkOperations() {
    using namespace openbrowser::core;

    BookmarkManager bookmarks;
    Require(bookmarks.TotalBookmarks() == 0, "Initial bookmarks should be 0");

    BookmarkItem bm;
    bm.id = "bm-1";
    bm.url = "https://example.com";
    bm.title = "Example Domain";
    bm.tags = {"sample", "test"};
    bm.workspace_id = "work";

    Require(bookmarks.AddBookmark(bm), "AddBookmark should succeed");
    Require(bookmarks.IsBookmarked("https://example.com"), "IsBookmarked should be true");

    // Duplicate addition should be prevented
    Require(!bookmarks.AddBookmark(bm), "Duplicate bookmark URL should fail");

    const auto work_bms = bookmarks.ListBookmarks("work");
    Require(work_bms.size() == 1, "Work workspace bookmarks should have 1 item");

    const auto personal_bms = bookmarks.ListBookmarks("personal");
    Require(personal_bms.empty(), "Personal workspace bookmarks should be empty");

    Require(bookmarks.RemoveBookmark("bm-1"), "RemoveBookmark should succeed");
    Require(!bookmarks.IsBookmarked("https://example.com"), "IsBookmarked should now be false");
    Require(bookmarks.TotalBookmarks() == 0, "Total bookmarks should be 0 after removal");
}

}  // namespace

int main() {
    TestHistoryRecordingAndDeduplication();
    TestHistorySearchAndRanking();
    TestBookmarkOperations();

    if (failures != 0) {
        std::cerr << "history_bookmark_tests failed with " << failures << " failures.\n";
        return 1;
    }

    std::cout << "All history and bookmark tests passed!\n";
    return 0;
}
