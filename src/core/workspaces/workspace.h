#pragma once

#include <optional>
#include <string>

namespace openbrowser::core {

using WorkspaceId = std::string;

struct Workspace {
    WorkspaceId id;
    std::string name;
    std::string badge_color{"#3B82F6"};
    bool is_ephemeral{false};
};

}  // namespace openbrowser::core
