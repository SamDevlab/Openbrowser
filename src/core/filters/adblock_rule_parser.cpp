#include "core/filters/adblock_rule_parser.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace openbrowser::core {

namespace {
std::string_view Trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

std::string ToLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

TrackerCategory GuessCategory(std::string_view pattern) {
    const std::string lower = ToLower(pattern);
    if (lower.find("analytics") != std::string::npos ||
        lower.find("telemetry") != std::string::npos ||
        lower.find("metrics") != std::string::npos ||
        lower.find("stats") != std::string::npos ||
        lower.find("tracker") != std::string::npos) {
        return TrackerCategory::Analytics;
    }
    if (lower.find("fingerprint") != std::string::npos) {
        return TrackerCategory::Fingerprinting;
    }
    if (lower.find("coin") != std::string::npos ||
        lower.find("miner") != std::string::npos) {
        return TrackerCategory::Cryptomining;
    }
    if (lower.find("facebook") != std::string::npos ||
        lower.find("twitter") != std::string::npos ||
        lower.find("social") != std::string::npos) {
        return TrackerCategory::Social;
    }
    return TrackerCategory::Advertising;
}
} // namespace

bool ParsedAdblockRule::Matches(
    std::string_view url,
    bool is_third_party,
    AdblockResourceType type) const {

    if (third_party_only && !is_third_party) {
        return false;
    }

    if (!HasResourceType(resource_types, type)) {
        return false;
    }

    if (pattern.empty()) {
        return false;
    }

    if (is_domain_anchor) {
        // Domain anchor: check if pattern matches hostname in URL
        const auto host_pos = url.find("://");
        std::string_view search_area = url;
        if (host_pos != std::string_view::npos) {
            search_area = url.substr(host_pos + 3);
        }
        return search_area.find(pattern) != std::string_view::npos;
    }

    if (is_start_anchor && is_end_anchor) {
        return url == pattern;
    }

    if (is_start_anchor) {
        return url.rfind(pattern, 0) == 0;
    }

    if (is_end_anchor) {
        if (url.length() < pattern.length()) {
            return false;
        }
        return url.substr(url.length() - pattern.length()) == pattern;
    }

    return url.find(pattern) != std::string_view::npos;
}

std::optional<ParsedAdblockRule> AdblockRuleParser::ParseLine(std::string_view line) {
    line = Trim(line);
    if (line.empty() || line.front() == '!' || line.front() == '[') {
        return std::nullopt; // Comment or header
    }

    // Cosmetic/element hiding rules (e.g. ##.ad or #@#) are ignored for network filtering
    if (line.find("##") != std::string_view::npos ||
        line.find("#?#") != std::string_view::npos ||
        line.find("#@#") != std::string_view::npos) {
        return std::nullopt;
    }

    ParsedAdblockRule rule;
    rule.raw_rule = std::string(line);

    if (line.rfind("@@", 0) == 0) {
        rule.is_exception = true;
        line.remove_prefix(2);
    }

    // Parse options after $
    const auto dollar = line.find('$');
    if (dollar != std::string_view::npos) {
        std::string_view options = line.substr(dollar + 1);
        line = line.substr(0, dollar);

        std::size_t start = 0;
        while (start < options.length()) {
            auto comma = options.find(',', start);
            if (comma == std::string_view::npos) {
                comma = options.length();
            }
            std::string_view opt = Trim(options.substr(start, comma - start));
            if (opt == "third-party") {
                rule.third_party_only = true;
            } else if (opt == "script") {
                rule.resource_types = rule.resource_types | AdblockResourceType::Script;
            } else if (opt == "image") {
                rule.resource_types = rule.resource_types | AdblockResourceType::Image;
            } else if (opt == "stylesheet") {
                rule.resource_types = rule.resource_types | AdblockResourceType::Stylesheet;
            } else if (opt == "xmlhttprequest") {
                rule.resource_types = rule.resource_types | AdblockResourceType::XmlHttpRequest;
            } else if (opt == "subdocument") {
                rule.resource_types = rule.resource_types | AdblockResourceType::Subdocument;
            }
            start = comma + 1;
        }
    }

    // Anchors
    if (line.rfind("||", 0) == 0) {
        rule.is_domain_anchor = true;
        line.remove_prefix(2);
    } else if (line.rfind('|', 0) == 0) {
        rule.is_start_anchor = true;
        line.remove_prefix(1);
    }

    if (!line.empty() && line.back() == '|') {
        rule.is_end_anchor = true;
        line.remove_suffix(1);
    }

    // Strip separator caret ^ at end
    if (!line.empty() && line.back() == '^') {
        line.remove_suffix(1);
    }

    if (line.empty()) {
        return std::nullopt;
    }

    rule.pattern = std::string(line);
    rule.category = GuessCategory(rule.pattern);
    return rule;
}

std::vector<ParsedAdblockRule> AdblockRuleParser::ParseStream(std::istream& input) {
    std::vector<ParsedAdblockRule> rules;
    std::string line;
    while (std::getline(input, line)) {
        auto parsed = ParseLine(line);
        if (parsed.has_value()) {
            rules.push_back(std::move(*parsed));
        }
    }
    return rules;
}

std::vector<ParsedAdblockRule> AdblockRuleParser::ParseText(std::string_view text) {
    std::string str(text);
    std::istringstream iss(str);
    return ParseStream(iss);
}

std::size_t AdblockRuleParser::CompileInto(
    ContentFilter& target_filter,
    const std::vector<ParsedAdblockRule>& rules) {

    std::size_t added = 0;
    for (const auto& r : rules) {
        if (r.is_exception) {
            target_filter.AddAllowDomain(r.pattern);
            ++added;
        } else {
            target_filter.AddBlockRule(r.pattern, r.category);
            ++added;
        }
    }
    return added;
}

} // namespace openbrowser::core
