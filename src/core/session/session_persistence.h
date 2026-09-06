#pragma once

#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session.h"
#include "core/tabs/tab.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openbrowser::core {

struct SessionTabRecord {
    TabId id;
    std::string url;
    std::string title;
    std::optional<WorkspaceId> workspace_id;
    TabLifecycle lifecycle{TabLifecycle::Background};
};

struct FocusItemRecord {
    std::string id;
    std::string url;
    std::optional<TabId> tab_id;
    std::optional<WorkspaceId> workspace_id;
    FocusState state{FocusState::Next};
};

struct SessionSnapshot {
    std::size_t schema_version{1};
    bool clean_shutdown{false};
    std::optional<TabId> active_tab_id;
    std::vector<SessionTabRecord> tabs;
    std::vector<FocusItemRecord> focus_items;
};

class SessionPersistence {
public:
    [[nodiscard]] static SessionSnapshot CaptureSnapshot(
        const BrowserSession& session,
        const FocusQueue& queue,
        bool clean_shutdown = false);

    [[nodiscard]] static bool RestoreSession(
        BrowserSession& session,
        FocusQueue& queue,
        const SessionSnapshot& snapshot);

    [[nodiscard]] static std::string Serialize(const SessionSnapshot& snapshot);
    [[nodiscard]] static std::optional<SessionSnapshot> Deserialize(std::string_view json);

    [[nodiscard]] static bool SaveToFile(
        const std::filesystem::path& file_path,
        const SessionSnapshot& snapshot);

    [[nodiscard]] static std::optional<SessionSnapshot> LoadFromFile(
        const std::filesystem::path& file_path);

    [[nodiscard]] static bool WasLastShutdownClean(
        const std::filesystem::path& file_path);
};

}  // namespace openbrowser::core
