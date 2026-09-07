#include "core/bookmarks/bookmark_manager.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
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

void EscapeString(const std::string_view str, std::string& out) {
    out += '"';
    for (const char c : str) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    out += '"';
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

std::string BookmarkManager::Serialize() const {
    std::string out;
    out.reserve(bookmarks_.size() * 160 + 32);
    out += "{\n  \"schema_version\": 1,\n  \"bookmarks\": [\n";
    for (std::size_t i = 0; i < bookmarks_.size(); ++i) {
        const auto& item = bookmarks_[i];
        out += "    {\n";
        out += "      \"id\": "; EscapeString(item.id, out); out += ",\n";
        out += "      \"url\": "; EscapeString(item.url, out); out += ",\n";
        out += "      \"title\": "; EscapeString(item.title, out); out += ",\n";
        out += "      \"created_at_ms\": " + std::to_string(item.created_at_ms) + ",\n";
        out += "      \"tags\": [";
        for (std::size_t t = 0; t < item.tags.size(); ++t) {
            EscapeString(item.tags[t], out);
            if (t + 1 < item.tags.size()) out += ", ";
        }
        out += "],\n";
        out += "      \"workspace_id\": ";
        if (item.workspace_id.has_value()) {
            EscapeString(*item.workspace_id, out);
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
    std::size_t pos = 0;
    std::vector<BookmarkItem> loaded;
    std::size_t max_id = 0;

    while ((pos = json.find("\"url\":", pos)) != std::string_view::npos) {
        const auto block_start = json.rfind('{', pos);
        const auto block_end = json.find('}', pos);
        if (block_start == std::string_view::npos || block_end == std::string_view::npos) {
            break;
        }
        const std::string_view block = json.substr(block_start, block_end - block_start + 1);

        auto extract_str = [&](std::string_view key) -> std::optional<std::string> {
            const auto kp = block.find(key);
            if (kp == std::string_view::npos) return std::nullopt;
            const auto s = block.find('"', kp + key.size());
            if (s == std::string_view::npos || s >= block_end) return std::nullopt;
            const auto e = block.find('"', s + 1);
            if (e == std::string_view::npos) return std::nullopt;
            return std::string(block.substr(s + 1, e - s - 1));
        };

        BookmarkItem item;
        if (const auto id_opt = extract_str("\"id\":")) {
            item.id = *id_opt;
            if (item.id.rfind("bm-", 0) == 0) {
                try {
                    const auto num = std::stoull(item.id.substr(3));
                    if (num > max_id) max_id = num;
                } catch (...) {}
            }
        }
        if (const auto url_opt = extract_str("\"url\":")) {
            item.url = *url_opt;
        }
        if (const auto title_opt = extract_str("\"title\":")) {
            item.title = *title_opt;
        }
        if (const auto ws_opt = extract_str("\"workspace_id\":")) {
            if (*ws_opt != "null") {
                item.workspace_id = *ws_opt;
            }
        }

        const auto time_p = block.find("\"created_at_ms\":");
        if (time_p != std::string_view::npos) {
            try {
                item.created_at_ms = std::stoll(std::string(block.substr(time_p + 16)));
            } catch (...) {}
        }

        const auto tags_p = block.find("\"tags\":");
        if (tags_p != std::string_view::npos) {
            const auto ts = block.find('[', tags_p);
            const auto te = block.find(']', tags_p);
            if (ts != std::string_view::npos && te != std::string_view::npos && te > ts) {
                const auto tag_str = block.substr(ts + 1, te - ts - 1);
                std::size_t tp = 0;
                while ((tp = tag_str.find('"', tp)) != std::string_view::npos) {
                    const auto t_end = tag_str.find('"', tp + 1);
                    if (t_end == std::string_view::npos) break;
                    item.tags.push_back(std::string(tag_str.substr(tp + 1, t_end - tp - 1)));
                    tp = t_end + 1;
                }
            }
        }

        if (!item.url.empty()) {
            if (item.id.empty()) {
                item.id = "bm-" + std::to_string(++max_id);
            }
            loaded.push_back(std::move(item));
        }
        pos = block_end + 1;
    }

    bookmarks_ = std::move(loaded);
    next_id_ = max_id + 1;
    return true;
}

bool BookmarkManager::SaveToFile(const std::filesystem::path& path) const {
    if (path.empty()) return false;
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    const auto json = Serialize();
    const auto temp_path = path.string() + ".tmp";
    {
        std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
    }
    std::filesystem::rename(temp_path, path, ec);
    return !ec;
}

bool BookmarkManager::LoadFromFile(const std::filesystem::path& path) {
    if (path.empty() || !std::filesystem::exists(path)) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    return Deserialize(content);
}

}  // namespace openbrowser::core
