#pragma once

#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"

#include <cstdint>

namespace openbrowser::desktop::aura {

// Aura visual tokens are intentionally browser-owned and dependency-free. The
// permanent chrome follows the same quiet-dark family as New Tab while giving
// tabs, omnibox and selected controls enough contrast to read as real product
// surfaces instead of diagnostic labels floating over a canvas.
inline constexpr cef_color_t kCanvas = static_cast<cef_color_t>(0xFF101115u);
inline constexpr cef_color_t kChromeSurface = static_cast<cef_color_t>(0xFF1B1C20u);
inline constexpr cef_color_t kPrivateChromeSurface = static_cast<cef_color_t>(0xFF211B2Bu);
inline constexpr cef_color_t kSurface = static_cast<cef_color_t>(0xFF202126u);
inline constexpr cef_color_t kSurfaceRaised = static_cast<cef_color_t>(0xFF292A30u);
inline constexpr cef_color_t kSurfaceSelected = static_cast<cef_color_t>(0xFF3A3B44u);
inline constexpr cef_color_t kInputSurface = static_cast<cef_color_t>(0xFF292A2Fu);
inline constexpr cef_color_t kBorderSubtle = static_cast<cef_color_t>(0xFF45464Fu);
inline constexpr cef_color_t kTextPrimary = static_cast<cef_color_t>(0xFFF4F4F6u);
inline constexpr cef_color_t kTextSecondary = static_cast<cef_color_t>(0xFFB7B8C0u);
inline constexpr cef_color_t kTextMuted = static_cast<cef_color_t>(0xFF858792u);
inline constexpr cef_color_t kAccent = static_cast<cef_color_t>(0xFF806EFFu);
inline constexpr cef_color_t kAccentSoft = static_cast<cef_color_t>(0xFF39325Du);
inline constexpr cef_color_t kDanger = static_cast<cef_color_t>(0xFFFF7A8Au);

inline constexpr int kSpace2 = 2;
inline constexpr int kSpace4 = 4;
inline constexpr int kSpace6 = 6;
inline constexpr int kSpace8 = 8;
inline constexpr int kSpace10 = 10;
inline constexpr int kSpace12 = 12;
inline constexpr int kSpace14 = 14;
inline constexpr int kSpace16 = 16;

// Permanent browser chrome is deliberately denser than drawers/settings. The
// target is the visual rhythm of a mature desktop browser without copying any
// third-party assets or geometry verbatim.
inline constexpr int kControlHeight = 34;
inline constexpr int kIconControlSize = 32;
inline constexpr int kCompactIconControlSize = 28;
inline constexpr int kTabHeight = 34;
inline constexpr int kOmniboxHeight = 36;
inline constexpr int kSidebarRailWidth = 52;
inline constexpr int kSidebarExpandedWidth = 216;
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
        button->SetMaximumSize(CefSize(kIconControlSize, kIconControlSize));
    }
}

inline void StyleTabButton(
    const CefRefPtr<CefLabelButton>& button,
    const bool active) {
    StyleButton(button, active, !active);
    if (button) {
        button->SetFontList(active ? kFontBodyStrong : kFontBody);
        button->SetHorizontalAlignment(CEF_HORIZONTAL_ALIGNMENT_LEFT);
        button->SetMinimumSize(CefSize(116, kTabHeight));
        button->SetMaximumSize(CefSize(216, kTabHeight));
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
