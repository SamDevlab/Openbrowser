#include "core/workspaces/workspace_manager.h"

#include "core/storage/atomic_file_store.h"
#include "core/storage/json_helper.h"

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

bool IsValidWorkspaceDocument(const std::string_view content) {
    const auto root = storage::ParseJson(content);
    if (!root.has_value() || root->type != storage::JsonValue::Type::Object) {
        return false;
    }
    if (root->GetSizeT("schema_version", 0) != 1) {
        return false;
    }
    const auto* active = root->Find("active_workspace_id");
    const auto* workspaces = root->Find("workspaces");
    return active != nullptr && active->type == storage::JsonValue::Type::String &&
           workspaces != nullptr && workspaces->type == storage::JsonValue::Type::Array;
}

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

void WorkspaceManager::SetAutoSavePath(std::filesystem::path path) {
    auto_save_path_ = std::move(path);
}

const std::filesystem::path& WorkspaceManager::AutoSavePath() const noexcept {
    return auto_save_path_;
}

void WorkspaceManager::TriggerAutoSave() const {
    if (!auto_save_path_.empty()) {
        static_cast<void>(SaveToFile(auto_save_path_));
    }
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
    TriggerAutoSave();
    return true;
}

bool WorkspaceManager::RemoveWorkspace(const WorkspaceId& id) {
    if (id == "default" || id == active_workspace_id_) {
        return false;
    }
    const bool removed = workspaces_.erase(id) > 0;
    if (removed) {
        TriggerAutoSave();
    }
    return removed;
}

const WorkspaceId& WorkspaceManager::ActiveWorkspaceId() const noexcept {
    return active_workspace_id_;
}

bool WorkspaceManager::SetActiveWorkspace(const WorkspaceId& id) {
    if (!HasWorkspace(id)) {
        return false;
    }
    active_workspace_id_ = id;
    TriggerAutoSave();
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

    TriggerAutoSave();
    return active_workspace_id_;
}

std::string WorkspaceManager::Serialize() const {
    std::string out;
    out.reserve(512);
    out += "{\n  \"schema_version\": 1,\n";
    out += "  \"active_workspace_id\": ";
    storage::EscapeJsonString(active_workspace_id_, out);
    out += ",\n  \"workspaces\": [\n";

    std::vector<const Workspace*> to_serialize;
    for (const auto& [_, ws] : workspaces_) {
        if (!ws.is_ephemeral) {
            to_serialize.push_back(&ws);
        }
    }

    for (std::size_t i = 0; i < to_serialize.size(); ++i) {
        const auto* ws = to_serialize[i];
        out += "    {\n";
        out += "      \"id\": "; storage::EscapeJsonString(ws->id, out); out += ",\n";
        out += "      \"name\": "; storage::EscapeJsonString(ws->name, out); out += ",\n";
        out += "      \"badge_color\": "; storage::EscapeJsonString(ws->badge_color, out); out += ",\n";
        out += "      \"is_ephemeral\": false\n";
        out += "    }";
        if (i + 1 < to_serialize.size()) {
            out += ",";
        }
        out += "\n";
    }
    out += "  ]\n}\n";
    return out;
}

bool WorkspaceManager::Deserialize(const std::string_view json) {
    if (json.empty()) {
        return false;
    }

    const auto root = storage::ParseJson(json);
    if (!root.has_value() || root->type != storage::JsonValue::Type::Object ||
        root->GetSizeT("schema_version", 0) != 1) {
        return false;
    }

    const auto* ws_arr = root->Find("workspaces");
    if (!ws_arr || ws_arr->type != storage::JsonValue::Type::Array) {
        return false;
    }

    std::map<WorkspaceId, Workspace> loaded;
    for (const auto& item : ws_arr->arr_val) {
        if (item.type != storage::JsonValue::Type::Object) continue;

        Workspace ws;
        ws.id = item.GetString("id");
        ws.name = item.GetString("name");
        ws.badge_color = item.GetString("badge_color", "#3B82F6");
        ws.is_ephemeral = item.GetBool("is_ephemeral", false);

        if (!ws.id.empty() && !ws.is_ephemeral) {
            loaded.emplace(ws.id, std::move(ws));
        }
    }

    if (loaded.find("default") == loaded.end()) {
        loaded.emplace("default", kDefaultWorkspace);
    }

    workspaces_ = std::move(loaded);

    const auto active_id = root->GetString("active_workspace_id", "default");
    if (HasWorkspace(active_id)) {
        active_workspace_id_ = active_id;
    } else {
        active_workspace_id_ = "default";
    }

    return true;
}

bool WorkspaceManager::SaveToFile(const std::filesystem::path& path) const {
    if (path.empty()) return false;
    const auto json = Serialize();
    const auto result = storage::AtomicWriteFile(path, json, /*keep_backup=*/true);
    return result.success;
}

bool WorkspaceManager::LoadFromFile(const std::filesystem::path& path) {
    if (path.empty()) return false;
    const auto result = storage::ReadFileWithBackupRecovery(path, IsValidWorkspaceDocument);

    if (!result.success) {
        return false;
    }
    return Deserialize(result.content);
}

}  // namespace openbrowser::core
