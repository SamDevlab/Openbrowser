#pragma once

#include "core/tabs/tab.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openbrowser::core {

struct HistoryEntry {
    std::string id;
    std::string url;
    std::string title;
    std::int64_t last_visit_time_ms{0};
    std::size_t visit_count{1};
    std::optional<WorkspaceId> workspace_id;
};

class HistoryManager {
public:
    HistoryManager() = default;

    void RecordVisit(
        std::string url,
        std::string title,
        std::optional<WorkspaceId> workspace_id = std::nullopt);

    bool RemoveEntry(const std::string& entry_id);

    [[nodiscard]] std::vector<HistoryEntry> Search(
        const std::string& query,
        std::size_t max_results = 10) const;

    [[nodiscard]] std::vector<HistoryEntry> ListHistory(std::size_t limit = 100) const;
    void ClearHistory() noexcept;
    void ClearWorkspaceHistory(const WorkspaceId& workspace_id);

    [[nodiscard]] std::size_t TotalEntries() const noexcept;

    void SetAutoSavePath(std::filesystem::path path);
    [[nodiscard]] const std::filesystem::path& AutoSavePath() const noexcept;

    [[nodiscard]] std::string Serialize() const;
    bool Deserialize(std::string_view json);

    [[nodiscard]] bool SaveToFile(const std::filesystem::path& path) const;
    bool LoadFromFile(const std::filesystem::path& path);

private:
    void TriggerAutoSave() const;

    std::vector<HistoryEntry> entries_;
    std::size_t next_id_{1};
    std::filesystem::path auto_save_path_;
};

}  // namespace openbrowser::core
