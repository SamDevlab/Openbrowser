#include "core/compatibility/compatibility_runner.h"
#include "core/compatibility/compatibility_scenario.h"

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

void TestScenarioEnumConversions() {
    using namespace openbrowser::core;

    Require(ScenarioExpectationToString(ScenarioExpectation::AllowAndLoad) == "AllowAndLoad", "AllowAndLoad string");
    Require(ScenarioExpectationToString(ScenarioExpectation::BlockResource) == "BlockResource", "BlockResource string");
    Require(ScenarioExpectationToString(ScenarioExpectation::RedirectChain) == "RedirectChain", "RedirectChain string");

    Require(ScenarioVerdictToString(ScenarioVerdict::Passed) == "Passed", "Passed verdict string");
    Require(ScenarioVerdictToString(ScenarioVerdict::Failed) == "Failed", "Failed verdict string");
    Require(ScenarioVerdictToString(ScenarioVerdict::Degraded) == "Degraded", "Degraded verdict string");
}

void TestCompatibilityRunnerEvaluation() {
    using namespace openbrowser::core;

    CompatibilityRunner runner;

    // Scenario 1: Standard web page load
    ScenarioAssertion s1;
    s1.name = "HomePage Load";
    s1.target_url = "https://example.com/index.html";
    s1.expectation = ScenarioExpectation::AllowAndLoad;
    s1.expected_status = 200;
    s1.required_headers = {"content-type", "etag"};
    s1.forbidden_headers = {"x-insecure-debug"};
    runner.AddScenario(s1);

    // Scenario 2: Ad Tracker blocking
    ScenarioAssertion s2;
    s2.name = "Tracker Blocked";
    s2.target_url = "https://tracker.test/pixel.js";
    s2.expectation = ScenarioExpectation::BlockResource;
    runner.AddScenario(s2);

    Require(runner.ScenarioCount() == 2, "Scenario count is 2");

    std::vector<ObservedRequest> observations = {
        ObservedRequest{
            .url = "https://example.com/index.html",
            .status = 200,
            .headers = {
                ObservedHeader{.name = "Content-Type", .value = "text/html"},
                ObservedHeader{.name = "ETag", .value = "\"12345\""},
            },
            .is_failed = false,
            .error = {},
        },
        ObservedRequest{
            .url = "https://tracker.test/pixel.js",
            .status = std::nullopt,
            .headers = {},
            .is_failed = true,
            .error = "BLOCKED_BY_CONTENT_FILTER",
        },
    };

    const auto summary = runner.RunAll(observations);
    Require(summary.total_scenarios == 2, "Total scenarios evaluated is 2");
    Require(summary.passed_scenarios == 2, "All 2 scenarios passed");
    Require(summary.failed_scenarios == 0, "0 failures");
    Require(summary.degraded_scenarios == 0, "0 degraded");
}

void TestCompatibilityRunnerFailures() {
    using namespace openbrowser::core;

    CompatibilityRunner runner;

    ScenarioAssertion s1;
    s1.name = "Status Check";
    s1.target_url = "https://example.com/api";
    s1.expected_status = 200;
    runner.AddScenario(s1);

    std::vector<ObservedRequest> observations = {
        ObservedRequest{
            .url = "https://example.com/api",
            .status = 500,
            .headers = {},
            .is_failed = false,
            .error = {},
        },
    };

    const auto summary = runner.RunAll(observations);
    Require(summary.failed_scenarios == 1, "Status mismatch detected as failure");
    Require(!summary.reports.empty() && summary.reports[0].verdict == ScenarioVerdict::Failed, "Report verdict is Failed");
}

}  // namespace

int main() {
    TestScenarioEnumConversions();
    TestCompatibilityRunnerEvaluation();
    TestCompatibilityRunnerFailures();

    if (failures != 0) {
        std::cerr << failures << " Compatibility test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser Compatibility Scenario invariants: PASS\n";
    return 0;
}
