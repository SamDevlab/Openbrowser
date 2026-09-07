#include "core/compatibility/user_agent_policy.h"

namespace openbrowser::core {

namespace {
std::string GetPlatformToken(const std::string& os_name) {
    if (os_name == "Windows") {
        return "Windows NT 10.0; Win64; x64";
    }
    if (os_name == "macOS") {
        return "Macintosh; Intel Mac OS X 10_15_7";
    }
    if (os_name == "Linux") {
        return "X11; Linux x86_64";
    }
    return "X11; Linux x86_64";
}
} // namespace

UserAgentPolicyEngine::UserAgentPolicyEngine(UserAgentMode mode)
    : mode_(mode) {}

void UserAgentPolicyEngine::SetMode(UserAgentMode mode) noexcept {
    mode_ = mode;
}

UserAgentMode UserAgentPolicyEngine::GetMode() const noexcept {
    return mode_;
}

void UserAgentPolicyEngine::SetChromiumVersion(uint32_t major, const std::string& full_version) {
    major_version_ = major;
    full_version_ = full_version;
}

uint32_t UserAgentPolicyEngine::GetChromiumMajorVersion() const noexcept {
    return major_version_;
}

const std::string& UserAgentPolicyEngine::GetChromiumFullVersion() const noexcept {
    return full_version_;
}

std::string UserAgentPolicyEngine::BuildUserAgent(
    const std::string& host_or_origin,
    const std::string& os_name,
    const CompatibilityMitigationRegistry* registry) const {

    if (mode_ == UserAgentMode::SiteScoped && registry != nullptr) {
        auto mit = registry->FindMitigation(host_or_origin);
        if (mit.has_value() &&
            HasFlag(mit->flags, MitigationFlag::OverrideUserAgent) &&
            !mit->custom_ua_override.empty()) {
            return mit->custom_ua_override;
        }
    }

    if (mode_ == UserAgentMode::AntiFingerprintUniform) {
        // Uniform generic platform and sanitized version without specific patch builds
        return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/" +
               std::to_string(major_version_) + ".0.0.0 Safari/537.36";
    }

    // Standard Chromium format
    const std::string platform = GetPlatformToken(os_name);
    return "Mozilla/5.0 (" + platform + ") AppleWebKit/537.36 (KHTML, like Gecko) Chrome/" +
           full_version_ + " Safari/537.36";
}

ClientHints UserAgentPolicyEngine::BuildClientHints(
    const std::string& host_or_origin,
    const std::string& os_name,
    const CompatibilityMitigationRegistry* registry) const {

    (void)host_or_origin;
    (void)registry;

    ClientHints hints;
    hints.sec_ch_ua = "\"Chromium\";v=\"" + std::to_string(major_version_) + "\", \"Not(A:Brand\";v=\"24\"";
    hints.sec_ch_ua_mobile = "?0";

    if (mode_ == UserAgentMode::AntiFingerprintUniform) {
        hints.sec_ch_ua_platform = "\"Windows\"";
    } else {
        hints.sec_ch_ua_platform = "\"" + (os_name.empty() ? "Linux" : os_name) + "\"";
    }

    return hints;
}

} // namespace openbrowser::core
