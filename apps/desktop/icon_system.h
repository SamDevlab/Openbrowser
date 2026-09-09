#pragma once

#include "include/cef_base.h"
#include "include/cef_image.h"
#include "include/views/cef_button.h"

#include <string_view>

class CefLabelButton;

namespace openbrowser::desktop {

// Openbrowser's native icon vocabulary. Icons are drawn from the small set of
// primitives in icon_system.cpp and cached as CEF images for the lifetime of
// the UI process. No font, emoji or third-party icon pack is involved.
enum class IconId {
    AppMark,
    Back,
    Forward,
    Reload,
    Security,
    SecurityWarning,
    InternalPage,
    Bookmark,
    Download,
    Profile,
    PrivateProfile,
    More,
    Sidebar,
    SidebarCollapse,
    Focus,
    Library,
    Settings,
    Commands,
    Network,
    WorkspaceHome,
    WorkspaceWork,
    WorkspacePersonal,
    WorkspaceGeneric,
    Close,
    Add,
    Previous,
    Next,
    Pause,
    Play,
    Clear,
    Search,
    Count,
};

[[nodiscard]] CefRefPtr<CefImage> GetIcon(IconId id);

void SetIcon(const CefRefPtr<CefLabelButton>& button, IconId id);

void ConfigureIconButton(
    const CefRefPtr<CefLabelButton>& button,
    IconId id,
    std::string_view text,
    std::string_view accessible_name,
    std::string_view tooltip,
    int group_id = 0);

}  // namespace openbrowser::desktop
