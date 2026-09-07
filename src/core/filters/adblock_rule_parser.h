#pragma once

#include "core/filters/content_filter.h"

#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openbrowser::core {

enum class AdblockResourceType : uint32_t {
    Any = 0,
    Script = 1 << 0,
    Image = 1 << 1,
    Stylesheet = 1 << 2,
    XmlHttpRequest = 1 << 3,
    Subdocument = 1 << 4
};

inline AdblockResourceType operator|(AdblockResourceType a, AdblockResourceType b) {
    return static_cast<AdblockResourceType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool HasResourceType(AdblockResourceType mask, AdblockResourceType type) {
    if (mask == AdblockResourceType::Any || type == AdblockResourceType::Any) {
        return true;
    }
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(type)) != 0;
}

struct ParsedAdblockRule {
    std::string raw_rule;
    std::string pattern;
    bool is_exception = false;     // Started with @@
    bool is_domain_anchor = false; // Started with ||
    bool is_start_anchor = false;  // Started with |
    bool is_end_anchor = false;    // Ended with |
    bool third_party_only = false;
    AdblockResourceType resource_types = AdblockResourceType::Any;
    TrackerCategory category = TrackerCategory::Advertising;

    [[nodiscard]] bool Matches(
        std::string_view url,
        bool is_third_party = false,
        AdblockResourceType type = AdblockResourceType::Any) const;
};

class AdblockRuleParser {
public:
    static std::optional<ParsedAdblockRule> ParseLine(std::string_view line);
    static std::vector<ParsedAdblockRule> ParseStream(std::istream& input);
    static std::vector<ParsedAdblockRule> ParseText(std::string_view text);

    static std::size_t CompileInto(
        ContentFilter& target_filter,
        const std::vector<ParsedAdblockRule>& rules);
};

} // namespace openbrowser::core
