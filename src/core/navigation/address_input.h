#pragma once

#include <optional>
#include <string>

namespace openbrowser::core::navigation {

struct SearchProvider {
    std::string name{"DuckDuckGo"};
    std::string search_url_template{"https://duckduckgo.com/?q=%s"};
};

// Normalizes user-entered address-bar text for direct URL/domain navigation.
// Returns std::nullopt if the input cannot be parsed as a URL or naked domain.
[[nodiscard]] std::optional<std::string> NormalizeAddressInput(std::string input);

// Resolves user-entered address-bar text:
// - If valid URL (http, https, about:), returns it directly.
// - If loopback address, normalizes to http://.
// - If naked domain (e.g. github.com, sub.example.com/path), normalizes to https://.
// - Otherwise, formats the query using the given SearchProvider template.
// - Returns std::nullopt only if input is empty or contains only whitespace.
[[nodiscard]] std::optional<std::string> ResolveAddressInput(
    std::string input,
    const SearchProvider& provider = SearchProvider{});

// URL-encodes a string (spaces to +, reserved characters to %XX).
[[nodiscard]] std::string UrlEncode(std::string_view value);

}  // namespace openbrowser::core::navigation
