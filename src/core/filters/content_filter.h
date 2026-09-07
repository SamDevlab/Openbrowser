#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace openbrowser::core {

// Forward declaration — avoids pulling the full header into every TU.
class FilterDecisionLog;

enum class FilterDecision {
    Allow,
    Block,
};

enum class TrackerCategory {
    Advertising,
    Analytics,
    Fingerprinting,
    Cryptomining,
    Social,
};

struct FilterRule {
    std::string pattern;
    TrackerCategory category;
};

class ContentFilter {
public:
    ContentFilter();

    void AddBlockRule(std::string pattern, TrackerCategory category = TrackerCategory::Advertising);
    void AddAllowDomain(std::string domain);
    void RemoveAllowDomain(const std::string& domain) noexcept;

    [[nodiscard]] FilterDecision Evaluate(std::string_view url) const;

    // M7.2: evaluate and emit a FilterDecisionRecord to the attached log.
    // request_id is used to cross-reference with the NetworkTraceBuffer.
    [[nodiscard]] FilterDecision EvaluateWithId(
        std::string_view url,
        const std::string& request_id,
        const std::string& scope = "global") const;

    // Attach a non-owning decision log (nullptr clears it).
    void SetDecisionLog(FilterDecisionLog* log) noexcept;

    [[nodiscard]] std::size_t TotalRules() const noexcept;
    [[nodiscard]] std::size_t BlockedCount() const noexcept;
    void ResetStats() noexcept;

    [[nodiscard]] bool IsEnabled() const noexcept;
    void SetEnabled(bool enabled) noexcept;

private:
    std::vector<FilterRule> rules_;
    std::unordered_set<std::string> allowlist_domains_;
    mutable std::size_t blocked_count_{0};
    bool enabled_{true};
    FilterDecisionLog* decision_log_{nullptr}; // M7.2 — non-owning
};

}  // namespace openbrowser::core
