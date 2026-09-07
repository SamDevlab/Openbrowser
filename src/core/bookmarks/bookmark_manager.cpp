#include "core/bookmarks/bookmark_manager.h"

#include "core/storage/atomic_file_store.h"
#include "core/storage/json_helper.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace openbrowser::core {

namespace {

std::int64_t NowEpochMs() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string ToLower(const std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

bool IsValidBookmarksDocument(const std::string_view content) {
    const auto root = storage::ParseJson(content);
    if (!root.has_value() || root->type != storage::JsonValue::Type::Object) {
        return false;
    }
    if (root->GetSizeT("schema_version", 0) != 1) {
        return false;
    }
    const auto* bookmarks = root->Find("bookmarks");
    return bookmarks != nullptr && bookmarks->type == storage::JsonValue::Type::Array;
}

}  // namespace

void BookmarkManager::SetAutoSavePath(std::filesystem::path path) {
    auto_save_path_ = std::move(path);
}

const std::filesystem::path& BookmarkManager::AutoSavePath() const noexcept {
    return auto_save_path_;
}

void BookmarkManager::TriggerAutoSave() const {
    if (!auto_save_path_.empty()) {
        static_cast<void>(SaveToFile(auto_save_path_));
    }
}

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
    TriggerAutoSave();
    return true;
}

bool BookmarkManager::RemoveBookmark(const std::string& id) {
    const auto it = std::remove_if(bookmarks_.begin(), bookmarks_.end(), [&](const BookmarkItem& item) {
        return item.id == id;
    });
    if (it != bookmarks_.end()) {
        bookmarks_.erase(it, bookmarks_.end());
        TriggerAutoSave();
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
        TriggerAutoSave();
        return true;
    }
    return false;
}

bool BookmarkManager::EditBookmark(
    const std::string& id,
    const std::string& new_title,
    const std::string& new_url,
    const std::optional<std::vector<std::string>>& new_tags) {
    if (new_url.empty()) {
        return false;
    }

    auto it = std::find_if(bookmarks_.begin(), bookmarks_.end(), [&](const BookmarkItem& item) {
        return item.id == id;
    });
    if (it == bookmarks_.end()) {
        return false;
    }

    if (it->url != new_url && IsBookmarked(new_url)) {
        return false;
    }

    it->title = new_title.empty() ? new_url : new_title;
    it->url = new_url;
    if (new_tags.has_value()) {
        it->tags = *new_tags;
    }
    TriggerAutoSave();
    return true;
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

std::string BookmarkManager::Serialize() const {
    std::string out;
    out.reserve(bookmarks_.size() * 160 + 32);
    out += "{\n  \"schema_version\": 1,\n  \"bookmarks\": [\n";
    for (std::size_t i = 0; i < bookmarks_.size(); ++i) {
        const auto& item = bookmarks_[i];
        out += "    {\n";
        out += "      \"id\": "; storage::EscapeJsonString(item.id, out); out += ",\n";
        out += "      \"url\": "; storage::EscapeJsonString(item.url, out); out += ",\n";
        out += "      \"title\": "; storage::EscapeJsonString(item.title, out); out += ",\n";
        out += "      \"created_at_ms\": " + std::to_string(item.created_at_ms) + ",\n";
        out += "      \"tags\": [";
        for (std::size_t t = 0; t < item.tags.size(); ++t) {
            storage::EscapeJsonString(item.tags[t], out);
            if (t + 1 < item.tags.size()) out += ", ";
        }
        out += "],\n";
        out += "      \"workspace_id\": ";
        if (item.workspace_id.has_value()) {
            storage::EscapeJsonString(*item.workspace_id, out);
        } else {
            out += "null";
        }
        out += "\n    }";
        if (i + 1 < bookmarks_.size()) {
            out += ",";
        }
        out += "\n";
    }
    out += "  ]\n}\n";
    return out;
}

bool BookmarkManager::Deserialize(const std::string_view json) {
    if (json.empty()) {
        return false;
    }

    const auto root = storage::ParseJson(json);
    if (!root.has_value() || root->type != storage::JsonValue::Type::Object ||
        root->GetSizeT("schema_version", 0) != 1) {
        return false;
    }

    const auto* bm_val = root->Find("bookmarks");
    if (!bm_val || bm_val->type != storage::JsonValue::Type::Array) {
        return false;
    }

    std::vector<BookmarkItem> loaded;
    std::size_t max_id = 0;

    for (const auto& obj : bm_val->arr_val) {
        if (obj.type != storage::JsonValue::Type::Object) continue;

        BookmarkItem item;
        item.id = obj.GetString("id");
        if (item.id.rfind("bm-", 0) == 0) {
            try {
                const auto num = std::stoull(item.id.substr(3));
                if (num > max_id) max_id = num;
            } catch (...) {}
        }
        item.url = obj.GetString("url");
        item.title = obj.GetString("title");
        item.created_at_ms = obj.GetInt64("created_at_ms", 0);
        item.workspace_id = obj.GetOptionalString("workspace_id");

        const auto* tags_val = obj.Find("tags");
        if (tags_val && tags_val->type == storage::JsonValue::Type::Array) {
            for (const auto& tag_item : tags_val->arr_val) {
                if (tag_item.type == storage::JsonValue::Type::String) {
                    item.tags.push_back(tag_item.str_val);
                }
            }
        }

        if (!item.url.empty()) {
            if (item.id.empty()) {
                item.id = "bm-" + std::to_string(++max_id);
            }
            loaded.push_back(std::move(item));
        }
    }

    bookmarks_ = std::move(loaded);
    next_id_ = max_id + 1;
    return true;
}

bool BookmarkManager::SaveToFile(const std::filesystem::path& path) const {
    if (path.empty()) return false;
    const auto json = Serialize();
    const auto result = storage::AtomicWriteFile(path, json, /*keep_backup=*/true);
    return result.success;
}

bool BookmarkManager::LoadFromFile(const std::filesystem::path& path) {
    if (path.empty()) return false;
    const auto result = storage::ReadFileWithBackupRecovery(path, IsValidBookmarksDocument);

    if (!result.success) {
        return false;
    }
    return Deserialize(result.content);
}

}  // namespace openbrowser::core
