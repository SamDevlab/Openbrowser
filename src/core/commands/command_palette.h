#pragma once

#include "core/commands/action_registry.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openbrowser::core {

struct CommandPaletteMatch {
    ActionDefinition action;
    int score{0};
};

class CommandPalette {
public:
    explicit CommandPalette(const ActionRegistry& registry);

    [[nodiscard]] std::vector<CommandPaletteMatch> Search(
        std::string_view query,
        std::optional<ActionCategory> category_filter = std::nullopt) const;

private:
    [[nodiscard]] static int CalculateScore(
        std::string_view query,
        const ActionDefinition& action);

    const ActionRegistry& registry_;
};

}  // namespace openbrowser::core
