#pragma once

#include "core/workspaces/workspace.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openbrowser::core {

class WorkspaceManager {
public:
    WorkspaceManager();

    [[nodiscard]] bool HasWorkspace(const WorkspaceId& id) const noexcept;
    [[nodiscard]] const Workspace* FindWorkspace(const WorkspaceId& id) const noexcept;
    [[nodiscard]] const Workspace& DefaultWorkspace() const noexcept;
    [[nodiscard]] std::vector<Workspace> ListWorkspaces() const;

    bool CreateWorkspace(Workspace workspace);
    bool RemoveWorkspace(const WorkspaceId& id);

    [[nodiscard]] const WorkspaceId& ActiveWorkspaceId() const noexcept;
    bool SetActiveWorkspace(const WorkspaceId& id);
    std::string CycleNextWorkspace();

    void SetAutoSavePath(std::filesystem::path path);
    [[nodiscard]] const std::filesystem::path& AutoSavePath() const noexcept;

    [[nodiscard]] std::string Serialize() const;
    bool Deserialize(std::string_view json);

    [[nodiscard]] bool SaveToFile(const std::filesystem::path& path) const;
    bool LoadFromFile(const std::filesystem::path& path);

private:
    void TriggerAutoSave() const;

    std::map<WorkspaceId, Workspace> workspaces_;
    WorkspaceId active_workspace_id_{"default"};
    std::filesystem::path auto_save_path_;
};

}  // namespace openbrowser::core
