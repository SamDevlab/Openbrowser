#include "core/commands/action_registry.h"

#include <utility>

namespace openbrowser::core {

std::string ActionCategoryToString(const ActionCategory category) {
    switch (category) {
        case ActionCategory::Navigation:
            return "Navigation";
        case ActionCategory::Workspaces:
            return "Workspaces";
        case ActionCategory::Focus:
            return "Focus";
        case ActionCategory::NetworkLab:
            return "Network Lab";
        case ActionCategory::Privacy:
            return "Privacy";
        case ActionCategory::Settings:
            return "Settings";
    }
    return "Navigation";
}

bool ActionRegistry::RegisterAction(ActionDefinition action) {
    if (action.id.empty()) {
        return false;
    }
    const auto [it, inserted] = actions_.emplace(action.id, std::move(action));
    return inserted;
}

bool ActionRegistry::UnregisterAction(const std::string& action_id) {
    return actions_.erase(action_id) > 0;
}

bool ActionRegistry::HasAction(const std::string& action_id) const noexcept {
    return actions_.find(action_id) != actions_.end();
}

const ActionDefinition* ActionRegistry::FindAction(const std::string& action_id) const noexcept {
    const auto it = actions_.find(action_id);
    if (it != actions_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ActionRegistry::ExecuteAction(const std::string& action_id) {
    const auto it = actions_.find(action_id);
    if (it == actions_.end() || !it->second.handler) {
        return false;
    }
    return it->second.handler();
}

std::vector<ActionDefinition> ActionRegistry::ListActions() const {
    std::vector<ActionDefinition> list;
    list.reserve(actions_.size());
    for (const auto& [id, action] : actions_) {
        list.push_back(action);
    }
    return list;
}

std::size_t ActionRegistry::TotalActions() const noexcept {
    return actions_.size();
}

}  // namespace openbrowser::core
