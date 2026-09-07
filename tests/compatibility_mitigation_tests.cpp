#include "core/compatibility/compatibility_mitigations.h"
#include "core/compatibility/user_agent_policy.h"

#include <iostream>
#include <string>

namespace {
void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

void TestMitigationRuleMatching() {
    using namespace openbrowser::core;

    MitigationRule rule;
    rule.id = "compat.legacy-hr-01";
    rule.origin_pattern = "*.legacy-corp.test";
    rule.reason = "Requires third-party cookies and custom UA";
    rule.flags = MitigationFlag::AllowStorageAccess | MitigationFlag::OverrideUserAgent;
    rule.custom_ua_override = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/100.0.0.0 Safari/537.36";
    rule.expires_at_utc = 2000000000;

    Require(rule.MatchesOrigin("app.legacy-corp.test"), "Matches sub-domain");
    Require(rule.MatchesOrigin("https://portal.legacy-corp.test/login"), "Matches full URL");
    Require(rule.MatchesOrigin("legacy-corp.test"), "Matches base apex domain");
    Require(!rule.MatchesOrigin("other-corp.test"), "Does not match unrelated domain");

    Require(!rule.IsExpired(1700000000), "Rule not expired at earlier time");
    Require(rule.IsExpired(2100000000), "Rule expired after expiry time");
}

void TestMitigationRegistry() {
    using namespace openbrowser::core;

    CompatibilityMitigationRegistry registry;
    Require(registry.Count() == 0, "Initial count is 0");

    MitigationRule r1;
    r1.id = "compat.001";
    r1.origin_pattern = "bank.example.com";
    r1.flags = MitigationFlag::BypassTrackerProtection;

    Require(registry.RegisterRule(r1), "Register rule succeeded");
    Require(registry.Count() == 1, "Count is 1");

    auto found = registry.FindMitigation("bank.example.com");
    Require(found.has_value(), "Found registered mitigation");
    Require(found->id == "compat.001", "ID matches");
    Require(HasFlag(found->flags, MitigationFlag::BypassTrackerProtection), "Flag is present");

    Require(registry.UnregisterRule("compat.001"), "Rule removed");
    Require(registry.Count() == 0, "Count is 0 after removal");
}

void TestUserAgentPolicyEngine() {
    using namespace openbrowser::core;

    CompatibilityMitigationRegistry registry;
    MitigationRule mit;
    mit.id = "mit.legacy";
    mit.origin_pattern = "legacy.site.com";
    mit.flags = MitigationFlag::OverrideUserAgent;
    mit.custom_ua_override = "LegacyBrowser/1.0";
    registry.RegisterRule(mit);

    UserAgentPolicyEngine engine(UserAgentMode::StandardChromium);
    engine.SetChromiumVersion(122, "122.0.6261.94");

    // Standard mode
    std::string ua_std = engine.BuildUserAgent("https://normal.com", "Windows", &registry);
    Require(ua_std.find("Chrome/122.0.6261.94") != std::string::npos, "Standard UA contains exact full version");

    // Anti-Fingerprinting mode
    engine.SetMode(UserAgentMode::AntiFingerprintUniform);
    std::string ua_af = engine.BuildUserAgent("https://normal.com", "macOS", &registry);
    Require(ua_af.find("Chrome/122.0.0.0") != std::string::npos, "Anti-fingerprint UA hides patch version");
    Require(ua_af.find("Windows NT 10.0") != std::string::npos, "Anti-fingerprint UA normalizes OS");

    // Site-scoped override mode
    engine.SetMode(UserAgentMode::SiteScoped);
    std::string ua_override = engine.BuildUserAgent("https://legacy.site.com/app", "Linux", &registry);
    Require(ua_override == "LegacyBrowser/1.0", "Site-scoped UA matches override");

    std::string ua_fallback = engine.BuildUserAgent("https://normal.com", "Linux", &registry);
    Require(ua_fallback.find("Chrome/122.0.6261.94") != std::string::npos, "Site-scoped mode falls back to standard");

    // Explicit Linux and Windows platform token verification
    engine.SetMode(UserAgentMode::StandardChromium);
    std::string ua_linux = engine.BuildUserAgent("https://example.org", "Linux");
    Require(ua_linux.find("X11; Linux x86_64") != std::string::npos, "Linux UA contains X11; Linux x86_64");
    auto hints_linux = engine.BuildClientHints("https://example.org", "Linux");
    Require(hints_linux.sec_ch_ua_platform == "\"Linux\"", "Linux ClientHints platform is \"Linux\"");

    std::string ua_win = engine.BuildUserAgent("https://example.org", "Windows");
    Require(ua_win.find("Windows NT 10.0; Win64; x64") != std::string::npos, "Windows UA contains Windows NT 10.0; Win64; x64");
    auto hints_win = engine.BuildClientHints("https://example.org", "Windows");
    Require(hints_win.sec_ch_ua_platform == "\"Windows\"", "Windows ClientHints platform is \"Windows\"");
}

} // namespace

int main() {
    TestMitigationRuleMatching();
    TestMitigationRegistry();
    TestUserAgentPolicyEngine();

    std::cout << "All CompatibilityMitigation tests passed successfully." << std::endl;
    return 0;
}
