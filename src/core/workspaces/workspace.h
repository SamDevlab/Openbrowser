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
    // Short browser-owned glyph used by Aura's compact workspace rail. Keep it
    // textual so visual identity stays portable and does not require assets.
    std::string icon{"◇"};
};

}  // namespace openbrowser::core
