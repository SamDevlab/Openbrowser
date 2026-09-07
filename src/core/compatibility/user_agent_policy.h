#pragma once

#include "core/compatibility/compatibility_mitigations.h"

#include <string>

namespace openbrowser::core {

enum class UserAgentMode {
    StandardChromium,
    AntiFingerprintUniform,
    SiteScoped
};

struct ClientHints {
    std::string sec_ch_ua;
    std::string sec_ch_ua_mobile = "?0";
    std::string sec_ch_ua_platform;
};

class UserAgentPolicyEngine {
public:
    explicit UserAgentPolicyEngine(UserAgentMode mode = UserAgentMode::StandardChromium);
    ~UserAgentPolicyEngine() = default;

    void SetMode(UserAgentMode mode) noexcept;
    [[nodiscard]] UserAgentMode GetMode() const noexcept;

    void SetChromiumVersion(uint32_t major, const std::string& full_version);
    [[nodiscard]] uint32_t GetChromiumMajorVersion() const noexcept;
    [[nodiscard]] const std::string& GetChromiumFullVersion() const noexcept;

    [[nodiscard]] std::string BuildUserAgent(
        const std::string& host_or_origin,
        const std::string& os_name,
        const CompatibilityMitigationRegistry* registry = nullptr) const;

    [[nodiscard]] ClientHints BuildClientHints(
        const std::string& host_or_origin,
        const std::string& os_name,
        const CompatibilityMitigationRegistry* registry = nullptr) const;

private:
    UserAgentMode mode_;
    uint32_t major_version_ = 122;
    std::string full_version_ = "122.0.6261.94";
};

} // namespace openbrowser::core
