#include "core/bookmarks/bookmark_manager.h"

#include <iostream>
#include <string>

namespace {
void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

void TestBookmarksBarPopulationAndWorkspaceIsolation() {
    using namespace openbrowser::core;

    BookmarkManager manager;

    // Default workspace bookmarks
    Require(manager.AddBookmark("https://github.com", "GitHub", "default"), "Added GitHub");
    Require(manager.AddBookmark("https://news.ycombinator.com", "Hacker News", "default"), "Added HN");

    // Work workspace bookmarks
    Require(manager.AddBookmark("https://jira.corp.internal", "Jira", "work"), "Added Jira to work");

    // Query default workspace
    auto default_bms = manager.ListBookmarks("default");
    Require(default_bms.size() == 2, "Default workspace has 2 bookmarks");
    Require(default_bms[0].title == "GitHub", "First bookmark is GitHub");

    // Query work workspace
    auto work_bms = manager.ListBookmarks("work");
    Require(work_bms.size() == 1, "Work workspace has 1 bookmark");
    Require(work_bms[0].title == "Jira", "Work bookmark is Jira");

    // Remove bookmark
    Require(manager.RemoveBookmark(default_bms[0].id), "Removed GitHub");
    Require(manager.ListBookmarks("default").size() == 1, "Default workspace now has 1 bookmark");
}

} // namespace

int main() {
    TestBookmarksBarPopulationAndWorkspaceIsolation();

    std::cout << "All BookmarksBar tests passed successfully." << std::endl;
    return 0;
}
