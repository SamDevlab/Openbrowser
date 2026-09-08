#pragma once

#include <string_view>

namespace openbrowser::core::navigation {

inline constexpr std::string_view kNewTabUrl = "https://newtab.openbrowser.invalid/";
inline constexpr std::string_view kPrivateNewTabUrl = "about:blank";
inline constexpr std::string_view kLegacyNewTabPlaceholderUrl = "https://example.com/";

[[nodiscard]] inline bool IsNewTabUrl(const std::string_view url) noexcept {
    return url == kNewTabUrl;
}

[[nodiscard]] inline bool IsBlankOrNewTabUrl(const std::string_view url) noexcept {
    return url.empty() || url == kPrivateNewTabUrl || IsNewTabUrl(url);
}

[[nodiscard]] inline bool IsLegacySyntheticNewTab(
    const std::string_view url,
    const std::string_view title) noexcept {
    if (url != kLegacyNewTabPlaceholderUrl) {
        return false;
    }

    return title == "New Tab" || title == "New tab" || title == "Private Tab";
}

}  // namespace openbrowser::core::navigation
