#include "core/compatibility/compatibility_mitigations.h"

#include <algorithm>

namespace openbrowser::core {

namespace {
std::string ExtractHost(const std::string& input) {
    std::string s = input;
    const std::string http = "http://";
    const std::string https = "https://";
    if (s.rfind(http, 0) == 0) {
        s = s.substr(http.length());
    } else if (s.rfind(https, 0) == 0) {
        s = s.substr(https.length());
    }

    const auto slash = s.find('/');
    if (slash != std::string::npos) {
        s = s.substr(0, slash);
    }
    const auto colon = s.find(':');
    if (colon != std::string::npos) {
        s = s.substr(0, colon);
    }
    return s;
}

bool WildcardDomainMatch(const std::string& host, const std::string& pattern) {
    if (pattern.empty()) {
        return false;
    }
    if (pattern == host) {
        return true;
    }
    if (pattern.rfind("*.", 0) == 0) {
        const std::string base = pattern.substr(2);
        if (host == base) {
            return true;
        }
        if (host.length() > base.length() &&
            host.rfind(base) == host.length() - base.length() &&
            host[host.length() - base.length() - 1] == '.') {
            return true;
        }
    }
    return false;
}
} // namespace

bool MitigationRule::MatchesOrigin(const std::string& host_or_origin) const {
    const std::string target_host = ExtractHost(host_or_origin);
    const std::string pattern_host = ExtractHost(origin_pattern);
    return WildcardDomainMatch(target_host, pattern_host);
}

bool MitigationRule::IsExpired(int64_t current_time_utc) const {
    if (expires_at_utc <= 0) {
        return false;
    }
    return current_time_utc >= expires_at_utc;
}

bool CompatibilityMitigationRegistry::RegisterRule(const MitigationRule& rule) {
    if (rule.id.empty() || rule.origin_pattern.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    rules_[rule.id] = rule;
    return true;
}

bool CompatibilityMitigationRegistry::UnregisterRule(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return rules_.erase(id) > 0;
}

std::optional<MitigationRule> CompatibilityMitigationRegistry::GetRule(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = rules_.find(id);
    if (it != rules_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<MitigationRule> CompatibilityMitigationRegistry::FindMitigation(
    const std::string& host_or_origin,
    int64_t current_time_utc) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [_, rule] : rules_) {
        if (!rule.IsExpired(current_time_utc) && rule.MatchesOrigin(host_or_origin)) {
            return rule;
        }
    }
    return std::nullopt;
}

std::vector<MitigationRule> CompatibilityMitigationRegistry::ListRules() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MitigationRule> result;
    result.reserve(rules_.size());
    for (const auto& [_, rule] : rules_) {
        result.push_back(rule);
    }
    return result;
}

std::size_t CompatibilityMitigationRegistry::Count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rules_.size();
}

} // namespace openbrowser::core
