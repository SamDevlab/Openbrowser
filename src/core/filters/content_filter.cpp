#include "core/filters/content_filter.h"
#include "core/network/filter_decision_log.h"

#include <algorithm>
#include <utility>

namespace openbrowser::core {

namespace {

std::string ToLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

}  // namespace

ContentFilter::ContentFilter() {
    // Seed default common tracker and telemetry patterns
    AddBlockRule("doubleclick.net", TrackerCategory::Advertising);
    AddBlockRule("google-analytics.com", TrackerCategory::Analytics);
    AddBlockRule("adservice.google.com", TrackerCategory::Advertising);
    AddBlockRule("facebook.com/tr", TrackerCategory::Social);
    AddBlockRule("adnxs.com", TrackerCategory::Advertising);
    AddBlockRule("criteo.com", TrackerCategory::Advertising);
    AddBlockRule("scorecardresearch.com", TrackerCategory::Analytics);
    AddBlockRule("hotjar.com", TrackerCategory::Analytics);
    AddBlockRule("coinhive.com", TrackerCategory::Cryptomining);
    AddBlockRule("fingerprintjs.com", TrackerCategory::Fingerprinting);
}

void ContentFilter::AddBlockRule(std::string pattern, const TrackerCategory category) {
    if (pattern.empty()) {
        return;
    }
    rules_.push_back(FilterRule{
        .pattern = ToLower(pattern),
        .category = category,
    });
}

void ContentFilter::AddAllowDomain(std::string domain) {
    if (!domain.empty()) {
        allowlist_domains_.insert(ToLower(domain));
    }
}

void ContentFilter::RemoveAllowDomain(const std::string& domain) noexcept {
    allowlist_domains_.erase(ToLower(domain));
}

FilterDecision ContentFilter::Evaluate(const std::string_view url) const {
    if (!enabled_ || url.empty()) {
        return FilterDecision::Allow;
    }

    const auto lower_url = ToLower(url);

    for (const auto& domain : allowlist_domains_) {
        if (lower_url.find(domain) != std::string::npos) {
            return FilterDecision::Allow;
        }
    }

    for (const auto& rule : rules_) {
        if (lower_url.find(rule.pattern) != std::string::npos) {
            ++blocked_count_;
            return FilterDecision::Block;
        }
    }

    return FilterDecision::Allow;
}

FilterDecision ContentFilter::EvaluateWithId(
    const std::string_view url,
    const std::string& request_id,
    const std::string& scope) const {
    const FilterDecision decision = Evaluate(url);
    if (decision_log_ != nullptr) {
        FilterDecisionRecord record;
        record.request_id = request_id;
        record.url = std::string(url);
        record.blocked = (decision == FilterDecision::Block);
        record.layer = FilterDecisionLayer::ContentFilter;
        record.scope = scope;
        if (record.blocked) {
            // Find the matching rule for the explanation.
            const auto lower_url = ToLower(url);
            for (const auto& rule : rules_) {
                if (lower_url.find(rule.pattern) != std::string::npos) {
                    record.rule_source = rule.pattern;
                    record.human_explanation =
                        std::string("Blocked by tracker rule: ") + rule.pattern;
                    break;
                }
            }
        } else {
            record.human_explanation = "Allowed by content filter policy";
        }
        decision_log_->AddDecision(std::move(record));
    }
    return decision;
}

void ContentFilter::SetDecisionLog(FilterDecisionLog* log) noexcept {
    decision_log_ = log;
}

std::size_t ContentFilter::TotalRules() const noexcept {
    return rules_.size();
}

std::size_t ContentFilter::BlockedCount() const noexcept {
    return blocked_count_;
}

void ContentFilter::ResetStats() noexcept {
    blocked_count_ = 0;
}

bool ContentFilter::IsEnabled() const noexcept {
    return enabled_;
}

void ContentFilter::SetEnabled(const bool enabled) noexcept {
    enabled_ = enabled;
}

}  // namespace openbrowser::core
