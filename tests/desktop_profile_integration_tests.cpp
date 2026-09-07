#include "core/compatibility/compatibility_mitigations.h"
#include "core/compatibility/user_agent_policy.h"
#include "core/profiles/profile_manager.h"

#include <iostream>
#include <string>

namespace {
void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

void TestDesktopProfileIntegration() {
    using namespace openbrowser::core;

    ProfileManager profile_manager;
    CompatibilityMitigationRegistry mitigation_registry;
    UserAgentPolicyEngine ua_engine;

    // Initial state: Default profile
    auto active = profile_manager.GetActiveProfile();
    Require(active != nullptr, "Active profile exists");
    Require(active->GetId() == "default", "Initial profile is default");
    Require(!active->IsEphemeral(), "Default profile is not ephemeral");

    // Launch ephemeral incognito session
    auto ephemeral = profile_manager.CreateEphemeralProfile("Private Session");
    Require(ephemeral != nullptr, "Created ephemeral profile");
    Require(ephemeral->IsEphemeral(), "Is ephemeral");
    Require(profile_manager.SetActiveProfile(ephemeral->GetId()), "Set ephemeral as active");

    active = profile_manager.GetActiveProfile();
    Require(active->GetId() == ephemeral->GetId(), "Active profile is ephemeral");
    Require(active->IsEphemeral(), "Active profile is ephemeral");

    // Store private session token
    active->SetSecret("session_token", "temp_token_xyz");
    Require(active->GetSecret("session_token") == "temp_token_xyz", "Private token stored in memory");

    // Close private profile and return to default
    profile_manager.SetActiveProfile("default");
    profile_manager.PurgeEphemeralProfiles();

    active = profile_manager.GetActiveProfile();
    Require(active->GetId() == "default", "Returned to default profile");
    Require(profile_manager.GetProfile(ephemeral->GetId()) == nullptr, "Ephemeral profile purged");

    // User-Agent policy generation in desktop
    std::string ua = ua_engine.BuildUserAgent("https://example.com", "Windows", &mitigation_registry);
    Require(!ua.empty(), "User-agent string generated");
    Require(ua.find("Chrome/") != std::string::npos, "User-agent contains Chrome token");
}

} // namespace

int main() {
    TestDesktopProfileIntegration();

    std::cout << "All DesktopProfileIntegration tests passed successfully." << std::endl;
    return 0;
}
