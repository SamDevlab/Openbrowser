#include "core/compatibility/compatibility_runner.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace openbrowser::core {
namespace {

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool HasHeader(const std::vector<ObservedHeader>& headers, const std::string& name) {
    const auto target = LowerAscii(name);
    for (const auto& h : headers) {
        if (LowerAscii(h.name) == target) {
            return true;
        }
    }
    return false;
}

}  // namespace

void CompatibilityRunner::AddScenario(ScenarioAssertion scenario) {
    scenarios_.push_back(std::move(scenario));
}

std::size_t CompatibilityRunner::ScenarioCount() const noexcept {
    return scenarios_.size();
}

ScenarioReport CompatibilityRunner::EvaluateScenario(
    const ScenarioAssertion& scenario,
    const std::vector<ObservedRequest>& observations) const {
    ScenarioReport report;
    report.scenario_name = scenario.name;
    report.verdict = ScenarioVerdict::Passed;

    const ObservedRequest* target_match = nullptr;
    for (const auto& obs : observations) {
        if (obs.url == scenario.target_url) {
            target_match = &obs;
            break;
        }
    }

    if (scenario.expectation == ScenarioExpectation::BlockResource) {
        if (target_match != nullptr && !target_match->is_failed) {
            report.verdict = ScenarioVerdict::Failed;
            report.failure_reasons.push_back("Expected resource to be blocked, but it loaded successfully");
        }
        return report;
    }

    if (target_match == nullptr) {
        report.verdict = ScenarioVerdict::Failed;
        report.failure_reasons.push_back("Target URL was not observed in network traces: " + scenario.target_url);
        return report;
    }

    if (scenario.expected_status.has_value()) {
        if (!target_match->status.has_value() || *target_match->status != *scenario.expected_status) {
            report.verdict = ScenarioVerdict::Failed;
            report.failure_reasons.push_back(
                "Status mismatch: expected " + std::to_string(*scenario.expected_status) +
                ", got " + (target_match->status.has_value() ? std::to_string(*target_match->status) : "none"));
        }
    }

    for (const auto& req_hdr : scenario.required_headers) {
        if (!HasHeader(target_match->headers, req_hdr)) {
            report.verdict = ScenarioVerdict::Failed;
            report.failure_reasons.push_back("Missing required header: " + req_hdr);
        }
    }

    for (const auto& forbid_hdr : scenario.forbidden_headers) {
        if (HasHeader(target_match->headers, forbid_hdr)) {
            report.verdict = ScenarioVerdict::Failed;
            report.failure_reasons.push_back("Forbidden header observed: " + forbid_hdr);
        }
    }

    for (const auto& pattern : scenario.blocked_url_patterns) {
        for (const auto& obs : observations) {
            if (obs.url.find(pattern) != std::string::npos && !obs.is_failed) {
                report.verdict = ScenarioVerdict::Degraded;
                report.failure_reasons.push_back("Blocked pattern loaded unexpectedly: " + obs.url);
            }
        }
    }

    return report;
}

CompatibilitySummary CompatibilityRunner::RunAll(
    const std::vector<ObservedRequest>& observations) const {
    CompatibilitySummary summary;
    summary.total_scenarios = scenarios_.size();

    for (const auto& scenario : scenarios_) {
        auto report = EvaluateScenario(scenario, observations);
        if (report.verdict == ScenarioVerdict::Passed) {
            ++summary.passed_scenarios;
        } else if (report.verdict == ScenarioVerdict::Failed) {
            ++summary.failed_scenarios;
        } else if (report.verdict == ScenarioVerdict::Degraded) {
            ++summary.degraded_scenarios;
        }
        summary.reports.push_back(std::move(report));
    }

    return summary;
}

}  // namespace openbrowser::core
