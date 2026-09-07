#pragma once

#include <optional>
#include <string>
#include <vector>

namespace openbrowser::core {

enum class ScenarioExpectation {
    AllowAndLoad,
    BlockResource,
    RedirectChain,
};

[[nodiscard]] std::string ScenarioExpectationToString(ScenarioExpectation exp);

struct ScenarioAssertion {
    std::string name;
    std::string target_url;
    ScenarioExpectation expectation{ScenarioExpectation::AllowAndLoad};
    std::optional<int> expected_status{200};
    std::vector<std::string> required_headers;
    std::vector<std::string> forbidden_headers;
    std::vector<std::string> blocked_url_patterns;
};

enum class ScenarioVerdict {
    Passed,
    Failed,
    Degraded,
};

[[nodiscard]] std::string ScenarioVerdictToString(ScenarioVerdict verdict);

struct ScenarioReport {
    std::string scenario_name;
    ScenarioVerdict verdict{ScenarioVerdict::Passed};
    std::vector<std::string> failure_reasons;
};

}  // namespace openbrowser::core
