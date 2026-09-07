#pragma once

#include "core/compatibility/compatibility_scenario.h"

#include <string>
#include <vector>

namespace openbrowser::core {

struct ObservedHeader {
    std::string name;
    std::string value;
};

struct ObservedRequest {
    std::string url;
    std::optional<int> status;
    std::vector<ObservedHeader> headers;
    bool is_failed{false};
    std::string error;
};

struct CompatibilitySummary {
    std::size_t total_scenarios{0};
    std::size_t passed_scenarios{0};
    std::size_t failed_scenarios{0};
    std::size_t degraded_scenarios{0};
    std::vector<ScenarioReport> reports;
};

class CompatibilityRunner {
public:
    CompatibilityRunner() = default;

    void AddScenario(ScenarioAssertion scenario);
    [[nodiscard]] std::size_t ScenarioCount() const noexcept;

    [[nodiscard]] ScenarioReport EvaluateScenario(
        const ScenarioAssertion& scenario,
        const std::vector<ObservedRequest>& observations) const;

    [[nodiscard]] CompatibilitySummary RunAll(
        const std::vector<ObservedRequest>& observations) const;

private:
    std::vector<ScenarioAssertion> scenarios_;
};

}  // namespace openbrowser::core
