#include "core/compatibility/compatibility_scenario.h"

namespace openbrowser::core {

std::string ScenarioExpectationToString(const ScenarioExpectation exp) {
    switch (exp) {
        case ScenarioExpectation::AllowAndLoad:
            return "AllowAndLoad";
        case ScenarioExpectation::BlockResource:
            return "BlockResource";
        case ScenarioExpectation::RedirectChain:
            return "RedirectChain";
    }
    return "AllowAndLoad";
}

std::string ScenarioVerdictToString(const ScenarioVerdict verdict) {
    switch (verdict) {
        case ScenarioVerdict::Passed:
            return "Passed";
        case ScenarioVerdict::Failed:
            return "Failed";
        case ScenarioVerdict::Degraded:
            return "Degraded";
    }
    return "Passed";
}

}  // namespace openbrowser::core
