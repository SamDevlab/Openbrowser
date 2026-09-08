#include "cef_browser_engine.h"

#include "include/wrapper/cef_helpers.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace openbrowser::desktop {
namespace {

constexpr std::array<int, 17> kZoomPercentages{
    25, 33, 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400, 500};

constexpr double kZoomRatio = 1.2;

[[nodiscard]] double ZoomLevelForPercent(const int percent) {
    return std::log(static_cast<double>(percent) / 100.0) / std::log(kZoomRatio);
}

[[nodiscard]] int ClosestZoomPercent(const double zoom_level) {
    const int raw_percent = static_cast<int>(
        std::lround(std::pow(kZoomRatio, zoom_level) * 100.0));

    const auto closest = std::min_element(
        kZoomPercentages.begin(),
        kZoomPercentages.end(),
        [raw_percent](const int lhs, const int rhs) {
            return std::abs(lhs - raw_percent) < std::abs(rhs - raw_percent);
        });
    return closest != kZoomPercentages.end() ? *closest : 100;
}

}  // namespace

int CefBrowserEngine::ZoomPercent(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();
    CefRefPtr<CefBrowser> browser = BrowserForCommand(tab_id);
    if (!browser || !browser->GetHost()) {
        return 100;
    }
    return ClosestZoomPercent(browser->GetHost()->GetZoomLevel());
}

int CefBrowserEngine::ZoomIn(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();
    CefRefPtr<CefBrowser> browser = BrowserForCommand(tab_id);
    if (!browser || !browser->GetHost()) {
        return 100;
    }

    const int current = ClosestZoomPercent(browser->GetHost()->GetZoomLevel());
    const auto next = std::upper_bound(kZoomPercentages.begin(), kZoomPercentages.end(), current);
    const int target = next != kZoomPercentages.end() ? *next : kZoomPercentages.back();
    browser->GetHost()->SetZoomLevel(ZoomLevelForPercent(target));
    return target;
}

int CefBrowserEngine::ZoomOut(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();
    CefRefPtr<CefBrowser> browser = BrowserForCommand(tab_id);
    if (!browser || !browser->GetHost()) {
        return 100;
    }

    const int current = ClosestZoomPercent(browser->GetHost()->GetZoomLevel());
    auto previous = std::lower_bound(kZoomPercentages.begin(), kZoomPercentages.end(), current);
    if (previous == kZoomPercentages.begin()) {
        browser->GetHost()->SetZoomLevel(ZoomLevelForPercent(kZoomPercentages.front()));
        return kZoomPercentages.front();
    }
    if (previous == kZoomPercentages.end() || *previous >= current) {
        --previous;
    }

    const int target = *previous;
    browser->GetHost()->SetZoomLevel(ZoomLevelForPercent(target));
    return target;
}

int CefBrowserEngine::ResetZoom(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();
    CefRefPtr<CefBrowser> browser = BrowserForCommand(tab_id);
    if (!browser || !browser->GetHost()) {
        return 100;
    }

    const double default_level = browser->GetHost()->GetDefaultZoomLevel();
    browser->GetHost()->SetZoomLevel(default_level);
    return ClosestZoomPercent(default_level);
}

}  // namespace openbrowser::desktop
