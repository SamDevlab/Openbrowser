#pragma once

#include "include/views/cef_label_button.h"

#include <cstdint>

namespace openbrowser::desktop::aura {

// Aura intentionally keeps motion short and native. CEF's ink-drop state
// transition gives buttons a consistent hover/press response without adding a
// JavaScript animation layer to browser chrome.
inline void EnableButtonMotion(CefRefPtr<CefLabelButton> button) {
    if (!button) {
        return;
    }
    button->SetInkDropEnabled(true);
}

inline void ApplySelectedAccent(
    CefRefPtr<CefLabelButton> button,
    const cef_color_t accent) {
    if (!button) {
        return;
    }
    EnableButtonMotion(button);
    button->SetTextColor(CEF_BUTTON_STATE_NORMAL, accent);
    button->SetTextColor(CEF_BUTTON_STATE_HOVERED, accent);
    button->SetTextColor(CEF_BUTTON_STATE_PRESSED, accent);
}

inline void ApplyHoverAccent(
    CefRefPtr<CefLabelButton> button,
    const cef_color_t accent) {
    if (!button) {
        return;
    }
    EnableButtonMotion(button);
    button->SetTextColor(CEF_BUTTON_STATE_HOVERED, accent);
    button->SetTextColor(CEF_BUTTON_STATE_PRESSED, accent);
}

constexpr std::int64_t kTransientFeedbackDurationMs = 1600;

}  // namespace openbrowser::desktop::aura
