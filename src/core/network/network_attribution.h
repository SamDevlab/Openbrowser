#pragma once

#include <string>
#include <string_view>

namespace openbrowser::core {

enum class NetworkAttribution {
    Page,
    UpdateCheck,
    FilterListSync,
    Telemetry,
    Transfer,
};

[[nodiscard]] std::string AttributionToString(NetworkAttribution attr);
[[nodiscard]] NetworkAttribution StringToAttribution(std::string_view str);

}  // namespace openbrowser::core
