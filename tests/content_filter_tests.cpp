#include "core/filters/content_filter.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestDefaultBlockRules() {
    using namespace openbrowser::core;

    ContentFilter filter;
    Require(filter.TotalRules() > 0, "Default filter should be seeded with rules");
    Require(filter.BlockedCount() == 0, "Initial blocked count should be 0");

    Require(filter.Evaluate("https://google-analytics.com/analytics.js") == FilterDecision::Block,
            "Google Analytics should be blocked");
    Require(filter.BlockedCount() == 1, "Blocked count should be incremented");

    Require(filter.Evaluate("https://ad.doubleclick.net/pixel") == FilterDecision::Block,
            "DoubleClick should be blocked");
    Require(filter.BlockedCount() == 2, "Blocked count should be 2");

    Require(filter.Evaluate("https://example.com/index.html") == FilterDecision::Allow,
            "Harmless URL should be allowed");
    Require(filter.BlockedCount() == 2, "Blocked count should remain 2");
}

void TestAllowlistExemption() {
    using namespace openbrowser::core;

    ContentFilter filter;
    Require(filter.Evaluate("https://facebook.com/tr?id=123") == FilterDecision::Block,
            "Facebook tracker should be blocked initially");

    filter.AddAllowDomain("facebook.com");
    Require(filter.Evaluate("https://facebook.com/tr?id=123") == FilterDecision::Allow,
            "Allowlist domain should bypass filter block");

    filter.RemoveAllowDomain("facebook.com");
    Require(filter.Evaluate("https://facebook.com/tr?id=123") == FilterDecision::Block,
            "Removing from allowlist should re-enable blocking");
}

void TestDisableFilter() {
    using namespace openbrowser::core;

    ContentFilter filter;
    filter.SetEnabled(false);
    Require(!filter.IsEnabled(), "Filter should report disabled");

    Require(filter.Evaluate("https://google-analytics.com/analytics.js") == FilterDecision::Allow,
            "When disabled, tracker should be allowed");

    filter.SetEnabled(true);
    Require(filter.Evaluate("https://google-analytics.com/analytics.js") == FilterDecision::Block,
            "When re-enabled, tracker should be blocked");
}

}  // namespace

int main() {
    TestDefaultBlockRules();
    TestAllowlistExemption();
    TestDisableFilter();

    if (failures != 0) {
        std::cerr << "content_filter_tests failed with " << failures << " failures.\n";
        return 1;
    }

    std::cout << "All content filter tests passed!\n";
    return 0;
}
