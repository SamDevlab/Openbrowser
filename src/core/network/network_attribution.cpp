#include "core/network/network_attribution.h"

namespace openbrowser::core {

std::string AttributionToString(const NetworkAttribution attr) {
    switch (attr) {
        case NetworkAttribution::Page:
            return "Page";
        case NetworkAttribution::UpdateCheck:
            return "UpdateCheck";
        case NetworkAttribution::FilterListSync:
            return "FilterListSync";
        case NetworkAttribution::Telemetry:
            return "Telemetry";
        case NetworkAttribution::Transfer:
            return "Transfer";
    }
    return "Page";
}

NetworkAttribution StringToAttribution(const std::string_view str) {
    if (str == "UpdateCheck") {
        return NetworkAttribution::UpdateCheck;
    }
    if (str == "FilterListSync") {
        return NetworkAttribution::FilterListSync;
    }
    if (str == "Telemetry") {
        return NetworkAttribution::Telemetry;
    }
    if (str == "Transfer") {
        return NetworkAttribution::Transfer;
    }
    return NetworkAttribution::Page;
}

}  // namespace openbrowser::core
