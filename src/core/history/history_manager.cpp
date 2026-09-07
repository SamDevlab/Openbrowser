#include "core/history/history_manager.h"

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

}  // namespace

void HistoryManager::SetAutoSavePath(std::filesystem::path path) {
    auto_save_path_ = std::move(path);
}

const std::filesystem::path& HistoryManager::AutoSavePath() const noexcept {
    return auto_save_path_;
}

void HistoryManager::TriggerAutoSave() const {
    if (!auto_save_path_.empty()) {
        static_cast<void>(SaveToFile(auto_save_path_));
    }
}

void HistoryManager::RecordVisit(
    std::string url,
    std::string title,
    std::optional<WorkspaceId> workspace_id) {
    if (url.empty()) {
        return;
    }

    const auto now = NowEpochMs();

    // Check if URL was already recorded in this workspace
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const HistoryEntry& entry) {
        return entry.url == url && entry.workspace_id == workspace_id;
    });

    if (it != entries_.end()) {
        ++it->visit_count;
        it->last_visit_time_ms = now;
        if (!title.empty()) {
            it->title = std::move(title);
        }
        TriggerAutoSave();
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
    TriggerAutoSave();
}

bool HistoryManager::RemoveEntry(const std::string& entry_id) {
    const auto it = std::remove_if(entries_.begin(), entries_.end(), [&](const HistoryEntry& entry) {
        return entry.id == entry_id;
    });
    if (it != entries_.end()) {
        entries_.erase(it, entries_.end());
        TriggerAutoSave();
        return true;
    }
    return false;
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
    TriggerAutoSave();
}

void HistoryManager::ClearWorkspaceHistory(const WorkspaceId& workspace_id) {
    const auto it = std::remove_if(entries_.begin(), entries_.end(), [&](const HistoryEntry& entry) {
        return entry.workspace_id.has_value() && *entry.workspace_id == workspace_id;
    });
    entries_.erase(it, entries_.end());
    TriggerAutoSave();
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
        out += "      \"id\": "; storage::EscapeJsonString(entry.id, out); out += ",\n";
        out += "      \"url\": "; storage::EscapeJsonString(entry.url, out); out += ",\n";
        out += "      \"title\": "; storage::EscapeJsonString(entry.title, out); out += ",\n";
        out += "      \"last_visit_time_ms\": " + std::to_string(entry.last_visit_time_ms) + ",\n";
        out += "      \"visit_count\": " + std::to_string(entry.visit_count) + ",\n";
        out += "      \"workspace_id\": ";
        if (entry.workspace_id.has_value()) {
            storage::EscapeJsonString(*entry.workspace_id, out);
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

    const auto root = storage::ParseJson(json);
    if (!root.has_value() || root->type != storage::JsonValue::Type::Object) {
        return false;
    }

    const auto* entries_val = root->Find("entries");
    if (!entries_val || entries_val->type != storage::JsonValue::Type::Array) {
        return false;
    }

    std::vector<HistoryEntry> loaded;
    std::size_t max_id = 0;

    for (const auto& item : entries_val->arr_val) {
        if (item.type != storage::JsonValue::Type::Object) continue;

        HistoryEntry entry;
        entry.id = item.GetString("id");
        if (entry.id.rfind("hist-", 0) == 0) {
            try {
                const auto num = std::stoull(entry.id.substr(5));
                if (num > max_id) max_id = num;
            } catch (...) {}
        }
        entry.url = item.GetString("url");
        entry.title = item.GetString("title");
        entry.last_visit_time_ms = item.GetInt64("last_visit_time_ms", 0);
        entry.visit_count = item.GetSizeT("visit_count", 1);
        entry.workspace_id = item.GetOptionalString("workspace_id");

        if (!entry.url.empty()) {
            if (entry.id.empty()) {
                entry.id = "hist-" + std::to_string(++max_id);
            }
            loaded.push_back(std::move(entry));
        }
    }

    entries_ = std::move(loaded);
    next_id_ = max_id + 1;
    return true;
}

bool HistoryManager::SaveToFile(const std::filesystem::path& path) const {
    if (path.empty()) return false;
    const auto json = Serialize();
    const auto result = storage::AtomicWriteFile(path, json, /*keep_backup=*/true);
    return result.success;
}

bool HistoryManager::LoadFromFile(const std::filesystem::path& path) {
    if (path.empty()) return false;
    const auto result = storage::ReadFileWithBackupRecovery(path, [](std::string_view content) {
        const auto root = storage::ParseJson(content);
        return root.has_value() && root->type == storage::JsonValue::Type::Object;
    });

    if (!result.success) {
        return false;
    }
    return Deserialize(result.content);
}

}  // namespace openbrowser::core
