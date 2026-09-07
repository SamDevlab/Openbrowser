#include "core/history/history_manager.h"

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

}  // namespace openbrowser::core
