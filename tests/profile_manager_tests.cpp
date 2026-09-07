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

void TestDefaultProfileInitialization() {
    using namespace openbrowser::core;

    ProfileManager manager;
    Require(manager.Count() == 1, "Initial count is 1 for default profile");

    auto active = manager.GetActiveProfile();
    Require(active != nullptr, "Active profile exists");
    Require(active->GetId() == "default", "Active profile is default");
    Require(!active->IsEphemeral(), "Default profile is persistent");
    Require(active->GetType() == ProfileType::Persistent, "Default profile type is Persistent");
    Require(!active->GetStoragePath().empty(), "Default profile has storage path");
}

void TestPersistentProfileCreationAndSwitching() {
    using namespace openbrowser::core;

    ProfileManager manager;
    ProfileConfig work_cfg;
    work_cfg.id = "work";
    work_cfg.name = "Work Profile";
    work_cfg.type = ProfileType::Persistent;
    work_cfg.storage_path = "profiles/work";

    Require(manager.CreateProfile(work_cfg), "Created work profile");
    Require(!manager.CreateProfile(work_cfg), "Duplicate profile creation fails");
    Require(manager.Count() == 2, "Profile count is 2");

    Require(manager.SetActiveProfile("work"), "Switched to work profile");
    Require(manager.GetActiveProfile()->GetId() == "work", "Active profile is work");

    Require(!manager.RemoveProfile("default"), "Cannot remove default profile");
}

void TestEphemeralProfileLifecycleAndPurge() {
    using namespace openbrowser::core;

    ProfileManager manager;
    auto ephemeral1 = manager.CreateEphemeralProfile("Private Session");
    Require(ephemeral1 != nullptr, "Ephemeral profile created");
    Require(ephemeral1->IsEphemeral(), "Is ephemeral");
    Require(ephemeral1->GetStoragePath().empty(), "Ephemeral profile has empty storage path");
    Require(manager.Count() == 2, "Count includes ephemeral profile");

    // Vault and session data
    ephemeral1->SetSecret("oauth_token", "secret12345");
    ephemeral1->SetSessionData("cookie", "sess=abc");

    Require(ephemeral1->HasSecret("oauth_token"), "Secret stored");
    Require(ephemeral1->GetSecret("oauth_token") == "secret12345", "Secret value matches");
    Require(ephemeral1->GetSessionData("cookie") == "sess=abc", "Session data matches");

    ephemeral1->RemoveSecret("oauth_token");
    Require(!ephemeral1->HasSecret("oauth_token"), "Secret removed");

    // Purge all ephemeral profiles
    manager.PurgeEphemeralProfiles();
    Require(manager.Count() == 1, "Ephemeral profiles purged from manager");
    Require(manager.GetProfile(ephemeral1->GetId()) == nullptr, "Ephemeral profile no longer registered");
}

void TestProfilePurgeWipesMemory() {
    using namespace openbrowser::core;

    ProfileConfig cfg;
    cfg.id = "temp";
    cfg.name = "Temp Profile";
    cfg.type = ProfileType::Ephemeral;

    Profile profile(cfg);
    profile.SetSecret("api_key", "topsecret");
    profile.SetSessionData("user", "test_user");

    Require(profile.HasSecret("api_key"), "Secret exists");
    profile.Purge();

    Require(!profile.HasSecret("api_key"), "Secret purged");
    Require(!profile.HasSessionData("user"), "Session data purged");
    Require(profile.GetSecret("api_key").empty(), "Secret returns empty string after purge");
}

} // namespace

int main() {
    TestDefaultProfileInitialization();
    TestPersistentProfileCreationAndSwitching();
    TestEphemeralProfileLifecycleAndPurge();
    TestProfilePurgeWipesMemory();

    std::cout << "All ProfileManager tests passed successfully." << std::endl;
    return 0;
}
