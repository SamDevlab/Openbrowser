#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace openbrowser::core {

enum class MitigationFlag : uint32_t {
    None = 0,
    AllowStorageAccess = 1 << 0,
    BypassTrackerProtection = 1 << 1,
    OverrideUserAgent = 1 << 2,
    DisableStrictHsts = 1 << 3
};

inline MitigationFlag operator|(MitigationFlag a, MitigationFlag b) {
    return static_cast<MitigationFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool HasFlag(MitigationFlag value, MitigationFlag flag) {
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

struct MitigationRule {
    std::string id;
    std::string origin_pattern;
    std::string reason;
    MitigationFlag flags = MitigationFlag::None;
    std::string custom_ua_override;
    int64_t expires_at_utc = 0; // 0 = permanent / until resolved

    [[nodiscard]] bool MatchesOrigin(const std::string& host_or_origin) const;
    [[nodiscard]] bool IsExpired(int64_t current_time_utc) const;
};

class CompatibilityMitigationRegistry {
public:
    CompatibilityMitigationRegistry() = default;
    ~CompatibilityMitigationRegistry() = default;

    bool RegisterRule(const MitigationRule& rule);
    bool UnregisterRule(const std::string& id);
    [[nodiscard]] std::optional<MitigationRule> GetRule(const std::string& id) const;
    [[nodiscard]] std::optional<MitigationRule> FindMitigation(const std::string& host_or_origin, int64_t current_time_utc = 0) const;
    [[nodiscard]] std::vector<MitigationRule> ListRules() const;
    [[nodiscard]] std::size_t Count() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, MitigationRule> rules_;
};

} // namespace openbrowser::core
