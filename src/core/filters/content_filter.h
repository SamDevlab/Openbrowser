#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace openbrowser::core {

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
};

}  // namespace openbrowser::core
