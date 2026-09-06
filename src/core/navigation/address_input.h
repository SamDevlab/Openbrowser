#pragma once

#include <optional>
#include <string>

namespace openbrowser::core::navigation {

// Normalizes user-entered address-bar text for the M1 browser chrome.
// Only HTTP(S) navigation is admitted here; additional schemes require
// explicit capability/policy work instead of being enabled accidentally.
[[nodiscard]] std::optional<std::string> NormalizeAddressInput(std::string input);

}  // namespace openbrowser::core::navigation
