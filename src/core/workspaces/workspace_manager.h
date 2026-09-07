#pragma once

#include "core/workspaces/workspace.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
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

private:
    std::map<WorkspaceId, Workspace> workspaces_;
    WorkspaceId active_workspace_id_{"default"};
};

}  // namespace openbrowser::core
