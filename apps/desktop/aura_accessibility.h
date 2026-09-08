#pragma once

#include "include/views/cef_label_button.h"

#include <string>
#include <string_view>

namespace openbrowser::desktop::aura {

inline constexpr int kSidebarAccessibilityGroupId = 2801;
inline constexpr int kTabAccessibilityGroupId = 2802;

inline void ConfigureActionButton(
    const CefRefPtr<CefLabelButton>& button,
    const std::string_view accessible_name,
    const std::string_view tooltip,
    const int group_id = 0) {
    if (!button) {
        return;
    }

    button->SetAccessibleName(std::string(accessible_name));
    button->SetTooltipText(std::string(tooltip));
    button->SetFocusable(true);
    if (group_id != 0) {
        button->SetGroupID(group_id);
    }
}

inline void ConfigurePassiveStatus(
    const CefRefPtr<CefLabelButton>& button,
    const std::string_view accessible_name) {
    if (!button) {
        return;
    }

    button->SetAccessibleName(std::string(accessible_name));
    button->SetFocusable(false);
}

}  // namespace openbrowser::desktop::aura
