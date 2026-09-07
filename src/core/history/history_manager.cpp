#include "core/history/history_manager.h"

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

void HistoryManager::RecordVisit(
    std::string url,
    std::string title,
    std::optional<WorkspaceId> workspace_id) {
    if (url.empty()) {
        return;
    }

    const auto now = NowEpochMs();

    // Check if URL was already recorded
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const HistoryEntry& entry) {
        return entry.url == url && entry.workspace_id == workspace_id;
    });

    if (it != entries_.end()) {
        ++it->visit_count;
        it->last_visit_time_ms = now;
        if (!title.empty()) {
            it->title = std::move(title);
        }
        return;
    }

    HistoryEntry entry;
    entry.id = "hist-" + std::to_string(next_id_++);
    entry.url = std::move(url);
    entry.title = title.empty() ? entry.url : std::move(title);
    entry.last_visit_time_ms = now;
    entry.visit_count = 1;
    entry.workspace_id = std::move(workspace_id);

    entries_.push_back(std::move(entry));
}

std::vector<HistoryEntry> HistoryManager::Search(
    const std::string& query,
    const std::size_t max_results) const {
    if (entries_.empty()) {
        return {};
    }

    const auto lower_query = ToLower(query);
    std::vector<HistoryEntry> matches;

    for (const auto& entry : entries_) {
        if (query.empty() ||
            ToLower(entry.title).find(lower_query) != std::string::npos ||
            ToLower(entry.url).find(lower_query) != std::string::npos) {
            matches.push_back(entry);
        }
    }

    // Sort by visit count descending, then by last visit time descending
    std::sort(matches.begin(), matches.end(), [](const HistoryEntry& a, const HistoryEntry& b) {
        if (a.visit_count != b.visit_count) {
            return a.visit_count > b.visit_count;
        }
        return a.last_visit_time_ms > b.last_visit_time_ms;
    });

    if (matches.size() > max_results) {
        matches.resize(max_results);
    }

    return matches;
}

std::vector<HistoryEntry> HistoryManager::ListHistory(const std::size_t limit) const {
    auto sorted = entries_;
    std::sort(sorted.begin(), sorted.end(), [](const HistoryEntry& a, const HistoryEntry& b) {
        return a.last_visit_time_ms > b.last_visit_time_ms;
    });
    if (sorted.size() > limit) {
        sorted.resize(limit);
    }
    return sorted;
}

void HistoryManager::ClearHistory() noexcept {
    entries_.clear();
}

void HistoryManager::ClearWorkspaceHistory(const WorkspaceId& workspace_id) {
    const auto it = std::remove_if(entries_.begin(), entries_.end(), [&](const HistoryEntry& entry) {
        return entry.workspace_id.has_value() && *entry.workspace_id == workspace_id;
    });
    entries_.erase(it, entries_.end());
}

std::size_t HistoryManager::TotalEntries() const noexcept {
    return entries_.size();
}

std::string HistoryManager::Serialize() const {
    std::string out;
    out.reserve(entries_.size() * 128 + 32);
    out += "{\n  \"schema_version\": 1,\n  \"entries\": [\n";
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const auto& entry = entries_[i];
        out += "    {\n";
        out += "      \"id\": "; EscapeString(entry.id, out); out += ",\n";
        out += "      \"url\": "; EscapeString(entry.url, out); out += ",\n";
        out += "      \"title\": "; EscapeString(entry.title, out); out += ",\n";
        out += "      \"last_visit_time_ms\": " + std::to_string(entry.last_visit_time_ms) + ",\n";
        out += "      \"visit_count\": " + std::to_string(entry.visit_count) + ",\n";
        out += "      \"workspace_id\": ";
        if (entry.workspace_id.has_value()) {
            EscapeString(*entry.workspace_id, out);
        } else {
            out += "null";
        }
        out += "\n    }";
        if (i + 1 < entries_.size()) {
            out += ",";
        }
        out += "\n";
    }
    out += "  ]\n}\n";
    return out;
}

bool HistoryManager::Deserialize(const std::string_view json) {
    if (json.empty()) {
        return false;
    }
    std::size_t pos = 0;
    std::vector<HistoryEntry> loaded;
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
            if (s == std::string_view::npos) return std::nullopt;
            const auto e = block.find('"', s + 1);
            if (e == std::string_view::npos) return std::nullopt;
            return std::string(block.substr(s + 1, e - s - 1));
        };

        HistoryEntry entry;
        if (const auto id_opt = extract_str("\"id\":")) {
            entry.id = *id_opt;
            if (entry.id.rfind("hist-", 0) == 0) {
                try {
                    const auto num = std::stoull(entry.id.substr(5));
                    if (num > max_id) max_id = num;
                } catch (...) {}
            }
        }
        if (const auto url_opt = extract_str("\"url\":")) {
            entry.url = *url_opt;
        }
        if (const auto title_opt = extract_str("\"title\":")) {
            entry.title = *title_opt;
        }
        if (const auto ws_opt = extract_str("\"workspace_id\":")) {
            if (*ws_opt != "null") {
                entry.workspace_id = *ws_opt;
            }
        }

        const auto time_p = block.find("\"last_visit_time_ms\":");
        if (time_p != std::string_view::npos) {
            try {
                entry.last_visit_time_ms = std::stoll(std::string(block.substr(time_p + 21)));
            } catch (...) {}
        }
        const auto count_p = block.find("\"visit_count\":");
        if (count_p != std::string_view::npos) {
            try {
                entry.visit_count = std::stoul(std::string(block.substr(count_p + 14)));
            } catch (...) {}
        }

        if (!entry.url.empty()) {
            if (entry.id.empty()) {
                entry.id = "hist-" + std::to_string(++max_id);
            }
            loaded.push_back(std::move(entry));
        }
        pos = block_end + 1;
    }

    entries_ = std::move(loaded);
    next_id_ = max_id + 1;
    return true;
}

bool HistoryManager::SaveToFile(const std::filesystem::path& path) const {
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

bool HistoryManager::LoadFromFile(const std::filesystem::path& path) {
    if (path.empty() || !std::filesystem::exists(path)) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    return Deserialize(content);
}

}  // namespace openbrowser::core
