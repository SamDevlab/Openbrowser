#include "core/filters/adblock_rule_parser.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

using openbrowser::core::AdblockResourceType;
using openbrowser::core::AdblockRuleParser;
using openbrowser::core::ContentFilter;

namespace {

AdblockResourceType ResourceTypeFromByte(const std::uint8_t value) {
    switch (value % 6U) {
        case 1U: return AdblockResourceType::Script;
        case 2U: return AdblockResourceType::Image;
        case 3U: return AdblockResourceType::Stylesheet;
        case 4U: return AdblockResourceType::XmlHttpRequest;
        case 5U: return AdblockResourceType::Subdocument;
        default: return AdblockResourceType::Any;
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
    if (data == nullptr) {
        return 0;
    }

    const std::string input(reinterpret_cast<const char*>(data), size);
    const std::string_view input_view(input);

    const auto parsed_line = AdblockRuleParser::ParseLine(input_view);
    if (parsed_line.has_value()) {
        const bool third_party = !input.empty() && ((data[0] & 1U) != 0U);
        const auto type = input.empty()
            ? AdblockResourceType::Any
            : ResourceTypeFromByte(data[size / 2U]);

        static_cast<void>(parsed_line->Matches(input_view, third_party, type));
        static_cast<void>(parsed_line->Matches("https://example.test/path/script.js", third_party, type));
        static_cast<void>(parsed_line->Matches("https://sub.example.test/image.png", !third_party, type));
    }

    const auto rules = AdblockRuleParser::ParseText(input_view);
    ContentFilter filter;
    static_cast<void>(AdblockRuleParser::CompileInto(filter, rules));

    static_cast<void>(filter.Evaluate(input_view));
    static_cast<void>(filter.Evaluate("https://example.test/resource"));

    return 0;
}
