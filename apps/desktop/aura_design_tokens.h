#pragma once

#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"

#include <cstdint>

namespace openbrowser::desktop::aura {

// Aura visual tokens are intentionally browser-owned and dependency-free. Keep
// product chrome restrained: deep neutral canvas, slightly elevated surfaces,
// quiet borders, one accent family, and predictable control geometry.
inline constexpr cef_color_t kCanvas = static_cast<cef_color_t>(0xFF0D1016u);
inline constexpr cef_color_t kChromeSurface = static_cast<cef_color_t>(0xFF131821u);
inline constexpr cef_color_t kSurface = static_cast<cef_color_t>(0xFF181E29u);
inline constexpr cef_color_t kSurfaceRaised = static_cast<cef_color_t>(0xFF202735u);
inline constexpr cef_color_t kSurfaceSelected = static_cast<cef_color_t>(0xFF292F43u);
inline constexpr cef_color_t kInputSurface = static_cast<cef_color_t>(0xFF10151Du);
inline constexpr cef_color_t kBorderSubtle = static_cast<cef_color_t>(0xFF303849u);
inline constexpr cef_color_t kTextPrimary = static_cast<cef_color_t>(0xFFF3F6FBu);
inline constexpr cef_color_t kTextSecondary = static_cast<cef_color_t>(0xFFB7C0CDu);
inline constexpr cef_color_t kTextMuted = static_cast<cef_color_t>(0xFF7F8A9Au);
inline constexpr cef_color_t kAccent = static_cast<cef_color_t>(0xFF8C82FFu);
inline constexpr cef_color_t kAccentSoft = static_cast<cef_color_t>(0xFF343053u);
inline constexpr cef_color_t kDanger = static_cast<cef_color_t>(0xFFFF7A8Au);

inline constexpr int kSpace2 = 2;
inline constexpr int kSpace4 = 4;
inline constexpr int kSpace6 = 6;
inline constexpr int kSpace8 = 8;
inline constexpr int kSpace10 = 10;
inline constexpr int kSpace12 = 12;
inline constexpr int kSpace14 = 14;
inline constexpr int kSpace16 = 16;

inline constexpr int kControlHeight = 36;
inline constexpr int kIconControlSize = 36;
inline constexpr int kCompactIconControlSize = 34;
inline constexpr int kTabHeight = 36;
inline constexpr int kSidebarRailWidth = 58;
inline constexpr int kSidebarExpandedWidth = 220;
inline constexpr int kPanelPadding = 14;
inline constexpr int kPanelGap = 8;

inline constexpr char kFontBody[] = "Segoe UI, Arial, 13px";
inline constexpr char kFontBodyStrong[] = "Segoe UI, Arial, Bold 13px";
inline constexpr char kFontCaption[] = "Segoe UI, Arial, 12px";
inline constexpr char kFontHeading[] = "Segoe UI, Arial, Bold 15px";

inline void StyleSurface(
    const CefRefPtr<CefPanel>& panel,
    const cef_color_t color = kSurface) {
    if (panel) {
        panel->SetBackgroundColor(color);
    }
}

inline void StyleButton(
    const CefRefPtr<CefLabelButton>& button,
    const bool selected = false,
    const bool quiet = false) {
    if (!button) {
        return;
    }

    button->SetFontList(kFontBody);
    button->SetTextColor(
        CEF_BUTTON_STATE_NORMAL,
        selected ? kTextPrimary : (quiet ? kTextMuted : kTextSecondary));
    button->SetTextColor(CEF_BUTTON_STATE_HOVERED, kTextPrimary);
    button->SetTextColor(CEF_BUTTON_STATE_PRESSED, kTextPrimary);
    button->SetTextColor(CEF_BUTTON_STATE_DISABLED, kTextMuted);
    button->SetBackgroundColor(selected ? kSurfaceSelected : kChromeSurface);
    button->SetInkDropEnabled(true);
}

inline void StyleIconButton(
    const CefRefPtr<CefLabelButton>& button,
    const bool selected = false) {
    StyleButton(button, selected, true);
    if (button) {
        button->SetMinimumSize(CefSize(kIconControlSize, kIconControlSize));
    }
}

inline void StyleTabButton(
    const CefRefPtr<CefLabelButton>& button,
    const bool active) {
    StyleButton(button, active, !active);
    if (button) {
        button->SetFontList(active ? kFontBodyStrong : kFontBody);
        button->SetMinimumSize(CefSize(112, kTabHeight));
        button->SetMaximumSize(CefSize(228, kTabHeight));
    }
}

inline void StyleTextField(
    const CefRefPtr<CefTextfield>& field,
    const bool prominent = false) {
    if (!field) {
        return;
    }

    // CEF Views 152 exposes typography and view background styling for
    // CefTextfield, but not the Chromium-internal text/placeholder/selection
    // color setters. Keep Aura on the supported public API instead of relying
    // on non-portable Views implementation details.
    field->SetFontList(prominent ? kFontBodyStrong : kFontBody);
    field->SetBackgroundColor(prominent ? kInputSurface : kChromeSurface);
}

inline void StylePassiveLabel(
    const CefRefPtr<CefLabelButton>& label,
    const bool heading = false,
    const bool muted = false) {
    if (!label) {
        return;
    }

    label->SetFontList(heading ? kFontHeading : kFontCaption);
    const cef_color_t color = muted ? kTextMuted : (heading ? kTextPrimary : kTextSecondary);
    label->SetTextColor(CEF_BUTTON_STATE_NORMAL, color);
    label->SetTextColor(CEF_BUTTON_STATE_DISABLED, color);
    label->SetBackgroundColor(kSurface);
}

}  // namespace openbrowser::desktop::aura
