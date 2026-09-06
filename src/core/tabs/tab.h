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

enum class NavigationState {
    Idle,
    Requested,
    Loading,
    Failed,
};

struct Tab {
    TabId id;
    std::string url;
    std::string title;
    TabLifecycle lifecycle{TabLifecycle::Background};
    std::optional<WorkspaceId> workspace_id;
    std::optional<std::string> pending_url;
    NavigationState navigation_state{NavigationState::Idle};
    std::optional<std::string> last_error;
    bool renderer_crashed{false};
};

}  // namespace openbrowser::core
