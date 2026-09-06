#pragma once

#include <optional>
#include <string>

namespace openbrowser::core {

using TabId = std::string;
using WorkspaceId = std::string;

enum class TabLifecycle {
    Active,
    Background,
    Suspended,
    Discarded,
};

struct Tab {
    TabId id;
    std::string url;
    std::string title;
    TabLifecycle lifecycle{TabLifecycle::Background};
    std::optional<WorkspaceId> workspace_id;
};

}  // namespace openbrowser::core
