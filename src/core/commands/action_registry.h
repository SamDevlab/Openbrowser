#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace openbrowser::core {

enum class ActionCategory {
    Navigation,
    Workspaces,
    Focus,
    NetworkLab,
    Privacy,
    Settings,
};

[[nodiscard]] std::string ActionCategoryToString(ActionCategory category);

struct ActionDefinition {
    std::string id;
    std::string title;
    std::string description;
    ActionCategory category{ActionCategory::Navigation};
    std::string shortcut_hint;
    std::function<bool()> handler;
};

class ActionRegistry {
public:
    ActionRegistry() = default;

    bool RegisterAction(ActionDefinition action);
    bool UnregisterAction(const std::string& action_id);
    [[nodiscard]] bool HasAction(const std::string& action_id) const noexcept;
    [[nodiscard]] const ActionDefinition* FindAction(const std::string& action_id) const noexcept;

    bool ExecuteAction(const std::string& action_id);

    [[nodiscard]] std::vector<ActionDefinition> ListActions() const;
    [[nodiscard]] std::size_t TotalActions() const noexcept;

private:
    std::unordered_map<std::string, ActionDefinition> actions_;
};

}  // namespace openbrowser::core
