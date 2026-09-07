#include "core/filters/adblock_rule_parser.h"

#include <iostream>
#include <string>

namespace {
void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

void TestAdblockLineParsing() {
    using namespace openbrowser::core;

    // Comment lines
    Require(!AdblockRuleParser::ParseLine("! This is a comment").has_value(), "Comment line ignored");
    Require(!AdblockRuleParser::ParseLine("[Adblock Plus 2.0]").has_value(), "Header line ignored");
    Require(!AdblockRuleParser::ParseLine("   ").has_value(), "Empty line ignored");

    // Element hiding
    Require(!AdblockRuleParser::ParseLine("example.com##.ad-banner").has_value(), "Element hiding rule ignored");

    // Domain anchor
    auto r1 = AdblockRuleParser::ParseLine("||doubleclick.net^$third-party,script");
    Require(r1.has_value(), "Parsed domain anchor rule");
    Require(r1->is_domain_anchor, "Domain anchor flag set");
    Require(r1->pattern == "doubleclick.net", "Pattern matches domain");
    Require(r1->third_party_only, "Third party flag set");
    Require(HasResourceType(r1->resource_types, AdblockResourceType::Script), "Script type set");
    Require(!r1->is_exception, "Not an exception");

    // Exception rule
    auto r2 = AdblockRuleParser::ParseLine("@@||safe.doubleclick.net^");
    Require(r2.has_value(), "Parsed exception rule");
    Require(r2->is_exception, "Exception flag set");
    Require(r2->pattern == "safe.doubleclick.net", "Exception pattern matches");

    // Start/End anchors
    auto r3 = AdblockRuleParser::ParseLine("|https://adservice.google.com/banner.gif|");
    Require(r3.has_value(), "Parsed exact URL rule");
    Require(r3->is_start_anchor, "Start anchor set");
    Require(r3->is_end_anchor, "End anchor set");
    Require(r3->pattern == "https://adservice.google.com/banner.gif", "Pattern matches");
}

void TestRuleMatching() {
    using namespace openbrowser::core;

    auto r_domain = AdblockRuleParser::ParseLine("||analytics.tracker.com^$third-party");
    Require(r_domain.has_value(), "Parsed rule");

    Require(r_domain->Matches("https://analytics.tracker.com/collect", true), "Matches third-party tracker");
    Require(!r_domain->Matches("https://analytics.tracker.com/collect", false), "Rejects first-party when third-party only");

    auto r_script = AdblockRuleParser::ParseLine("/ad_logger.js$script");
    Require(r_script.has_value(), "Parsed script rule");
    Require(r_script->Matches("https://example.com/static/ad_logger.js", false, AdblockResourceType::Script), "Matches script resource");
    Require(!r_script->Matches("https://example.com/static/ad_logger.js", false, AdblockResourceType::Image), "Rejects image resource");
}

void TestCompileIntoContentFilter() {
    using namespace openbrowser::core;

    const std::string list_text =
        "! Sample Filter List\n"
        "||adserver.bad^\n"
        "||tracker.telemetry.com^\n"
        "@@||allowlisted.adserver.bad^\n";

    auto parsed_rules = AdblockRuleParser::ParseText(list_text);
    Require(parsed_rules.size() == 3, "Parsed 3 rules");

    ContentFilter filter;
    const auto compiled_count = AdblockRuleParser::CompileInto(filter, parsed_rules);
    Require(compiled_count == 3, "Compiled 3 rules into ContentFilter");

    // Evaluation
    Require(filter.Evaluate("https://adserver.bad/banner.png") == FilterDecision::Block, "Blocks adserver.bad");
    Require(filter.Evaluate("https://tracker.telemetry.com/event") == FilterDecision::Block, "Blocks telemetry tracker");
    Require(filter.Evaluate("https://allowlisted.adserver.bad/banner.png") == FilterDecision::Allow, "Allows allowlisted domain");
    Require(filter.Evaluate("https://clean-site.org/index.html") == FilterDecision::Allow, "Allows clean site");
}

} // namespace

int main() {
    TestAdblockLineParsing();
    TestRuleMatching();
    TestCompileIntoContentFilter();

    std::cout << "All AdblockRuleParser tests passed successfully." << std::endl;
    return 0;
}
