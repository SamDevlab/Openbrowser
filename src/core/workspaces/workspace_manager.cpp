#include "core/workspaces/workspace_manager.h"

#include <algorithm>
#include <utility>

namespace openbrowser::core {

namespace {

const Workspace kDefaultWorkspace{
    .id = "default",
    .name = "Default",
    .badge_color = "#3B82F6",
    .is_ephemeral = false,
};

}  // namespace

WorkspaceManager::WorkspaceManager() {
    workspaces_.emplace("default", kDefaultWorkspace);
    workspaces_.emplace("work", Workspace{
        .id = "work",
        .name = "Work",
        .badge_color = "#10B981",
        .is_ephemeral = false,
    });
    workspaces_.emplace("personal", Workspace{
        .id = "personal",
        .name = "Personal",
        .badge_color = "#8B5CF6",
        .is_ephemeral = false,
    });
}

bool WorkspaceManager::HasWorkspace(const WorkspaceId& id) const noexcept {
    return workspaces_.find(id) != workspaces_.end();
}

const Workspace* WorkspaceManager::FindWorkspace(const WorkspaceId& id) const noexcept {
    const auto it = workspaces_.find(id);
    if (it == workspaces_.end()) {
        return nullptr;
    }
    return &it->second;
}

const Workspace& WorkspaceManager::DefaultWorkspace() const noexcept {
    const auto it = workspaces_.find("default");
    if (it != workspaces_.end()) {
        return it->second;
    }
    return kDefaultWorkspace;
}

std::vector<Workspace> WorkspaceManager::ListWorkspaces() const {
    std::vector<Workspace> list;
    list.reserve(workspaces_.size());
    for (const auto& [_, ws] : workspaces_) {
        list.push_back(ws);
    }
    return list;
}

bool WorkspaceManager::CreateWorkspace(Workspace workspace) {
    if (workspace.id.empty() || HasWorkspace(workspace.id)) {
        return false;
    }
    const auto id = workspace.id;
    workspaces_.emplace(id, std::move(workspace));
    return true;
}

bool WorkspaceManager::RemoveWorkspace(const WorkspaceId& id) {
    if (id == "default" || id == active_workspace_id_) {
        return false;
    }
    return workspaces_.erase(id) > 0;
}

const WorkspaceId& WorkspaceManager::ActiveWorkspaceId() const noexcept {
    return active_workspace_id_;
}

bool WorkspaceManager::SetActiveWorkspace(const WorkspaceId& id) {
    if (!HasWorkspace(id)) {
        return false;
    }
    active_workspace_id_ = id;
    return true;
}

std::string WorkspaceManager::CycleNextWorkspace() {
    if (workspaces_.empty()) {
        return active_workspace_id_;
    }

    std::vector<std::string> ids;
    ids.reserve(workspaces_.size());
    for (const auto& [id, _] : workspaces_) {
        ids.push_back(id);
    }

    const auto it = std::find(ids.begin(), ids.end(), active_workspace_id_);
    if (it == ids.end() || std::next(it) == ids.end()) {
        active_workspace_id_ = ids.front();
    } else {
        active_workspace_id_ = *std::next(it);
    }

    return active_workspace_id_;
}

}  // namespace openbrowser::core
