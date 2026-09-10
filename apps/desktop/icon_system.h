#pragma once

#include "icon_metrics.h"

#include "include/cef_base.h"
#include "include/cef_image.h"
#include "include/views/cef_button.h"

#include <string_view>

class CefLabelButton;

namespace openbrowser::desktop {

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
