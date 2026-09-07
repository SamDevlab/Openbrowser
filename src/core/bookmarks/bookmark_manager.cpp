#include "core/bookmarks/bookmark_manager.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace openbrowser::core {

namespace {

std::int64_t NowEpochMs() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string ToLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

}  // namespace

bool BookmarkManager::AddBookmark(BookmarkItem item) {
    if (item.url.empty()) {
        return false;
    }

    if (IsBookmarked(item.url)) {
        return false;
    }

    if (item.id.empty()) {
        item.id = "bm-" + std::to_string(next_id_++);
    }
    if (item.title.empty()) {
        item.title = item.url;
    }
    if (item.created_at_ms <= 0) {
        item.created_at_ms = NowEpochMs();
    }

    bookmarks_.push_back(std::move(item));
    return true;
}

bool BookmarkManager::RemoveBookmark(const std::string& id) {
    const auto it = std::remove_if(bookmarks_.begin(), bookmarks_.end(), [&](const BookmarkItem& item) {
        return item.id == id;
    });
    if (it != bookmarks_.end()) {
        bookmarks_.erase(it, bookmarks_.end());
        return true;
    }
    return false;
}

bool BookmarkManager::RemoveBookmarkByUrl(const std::string& url) {
    const auto it = std::remove_if(bookmarks_.begin(), bookmarks_.end(), [&](const BookmarkItem& item) {
        return item.url == url;
    });
    if (it != bookmarks_.end()) {
        bookmarks_.erase(it, bookmarks_.end());
        return true;
    }
    return false;
}

bool BookmarkManager::IsBookmarked(const std::string& url) const noexcept {
    return std::any_of(bookmarks_.begin(), bookmarks_.end(), [&](const BookmarkItem& item) {
        return item.url == url;
    });
}

const BookmarkItem* BookmarkManager::FindBookmark(const std::string& id) const noexcept {
    const auto it = std::find_if(bookmarks_.begin(), bookmarks_.end(), [&](const BookmarkItem& item) {
        return item.id == id;
    });
    if (it != bookmarks_.end()) {
        return &(*it);
    }
    return nullptr;
}

std::vector<BookmarkItem> BookmarkManager::ListBookmarks(
    const std::optional<WorkspaceId>& workspace_id) const {
    if (!workspace_id.has_value()) {
        return bookmarks_;
    }

    std::vector<BookmarkItem> filtered;
    for (const auto& item : bookmarks_) {
        if (item.workspace_id == workspace_id) {
            filtered.push_back(item);
        }
    }
    return filtered;
}

std::vector<BookmarkItem> BookmarkManager::Search(const std::string& query) const {
    if (query.empty()) {
        return bookmarks_;
    }

    const auto lower_query = ToLower(query);
    std::vector<BookmarkItem> matches;

    for (const auto& item : bookmarks_) {
        if (ToLower(item.title).find(lower_query) != std::string::npos ||
            ToLower(item.url).find(lower_query) != std::string::npos) {
            matches.push_back(item);
        }
    }
    return matches;
}

std::size_t BookmarkManager::TotalBookmarks() const noexcept {
    return bookmarks_.size();
}

}  // namespace openbrowser::core
