#include "core/capabilities/capability_policy.h"
#include "core/capabilities/permission_request.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestPermissionTypesAndStrings() {
    using openbrowser::core::Capability;
    using openbrowser::core::PermissionResponse;
    using openbrowser::core::ToString;

    Require(ToString(Capability::Camera) == "Camera", "Camera string representation");
    Require(ToString(Capability::Microphone) == "Microphone", "Microphone string representation");
    Require(ToString(Capability::Geolocation) == "Geolocation", "Geolocation string representation");
    Require(ToString(Capability::Notifications) == "Notifications", "Notifications string representation");
    Require(ToString(Capability::ClipboardRead) == "Clipboard Read", "ClipboardRead string representation");

    Require(ToString(PermissionResponse::Allow) == "Allow", "Allow string representation");
    Require(ToString(PermissionResponse::Block) == "Block", "Block string representation");
    Require(ToString(PermissionResponse::Dismiss) == "Dismiss", "Dismiss string representation");
}

void TestOriginPermissionManagement() {
    using openbrowser::core::Capability;
    using openbrowser::core::CapabilityDecision;
    using openbrowser::core::CapabilityPolicy;

    auto policy = CapabilityPolicy::CreateDefault();
    const std::string origin = "https://meet.example.com";

    Require(
        policy.Resolve(Capability::Camera, {.origin = origin}) == CapabilityDecision::Ask,
        "default camera policy is Ask");
    Require(
        policy.Resolve(Capability::Microphone, {.origin = origin}) == CapabilityDecision::Ask,
        "default mic policy is Ask");
    Require(policy.GetOriginRules(origin).empty(), "origin rules start empty");

    policy.SetOrigin(origin, Capability::Camera, CapabilityDecision::Allow);
    policy.SetOrigin(origin, Capability::Microphone, CapabilityDecision::Deny);

    Require(
        policy.Resolve(Capability::Camera, {.origin = origin}) == CapabilityDecision::Allow,
        "origin camera policy becomes Allow");
    Require(
        policy.Resolve(Capability::Microphone, {.origin = origin}) == CapabilityDecision::Deny,
        "origin mic policy becomes Deny");

    const auto rules = policy.GetOriginRules(origin);
    Require(rules.size() == 2, "origin rules size is 2");
    Require(rules.at(Capability::Camera) == CapabilityDecision::Allow, "rules contains Allow Camera");
    Require(rules.at(Capability::Microphone) == CapabilityDecision::Deny, "rules contains Deny Microphone");

    policy.ClearOriginRules(origin);
    Require(policy.GetOriginRules(origin).empty(), "origin rules cleared");
    Require(
        policy.Resolve(Capability::Camera, {.origin = origin}) == CapabilityDecision::Ask,
        "reverts to default Ask after clear");
}

void TestRememberedOriginPermissionPersistence() {
    using openbrowser::core::Capability;
    using openbrowser::core::CapabilityDecision;
    using openbrowser::core::CapabilityPolicy;

    const auto path = std::filesystem::temp_directory_path() / "openbrowser_permission_rules_test.json";
    const auto backup = std::filesystem::path(path.string() + ".bak");
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(backup, ec);

    const std::string origin = "https://permissions.example";

    auto policy = CapabilityPolicy::CreateDefault();
    policy.SetAutoSavePath(path);
    policy.SetOrigin(origin, Capability::Camera, CapabilityDecision::Allow);
    policy.SetOrigin(origin, Capability::Notifications, CapabilityDecision::Deny);

    Require(std::filesystem::exists(path), "remembered permission rules auto-save to disk");

    auto reloaded = CapabilityPolicy::CreateDefault();
    Require(reloaded.LoadOriginRulesFromFile(path), "remembered permission rules reload");
    Require(
        reloaded.Resolve(Capability::Camera, {.origin = origin}) == CapabilityDecision::Allow,
        "Always allow survives restart");
    Require(
        reloaded.Resolve(Capability::Notifications, {.origin = origin}) == CapabilityDecision::Deny,
        "remembered block survives restart");

    reloaded.SetAutoSavePath(path);
    reloaded.ClearOriginRules(origin);

    auto cleared = CapabilityPolicy::CreateDefault();
    Require(cleared.LoadOriginRulesFromFile(path), "cleared permission store reloads");
    Require(
        cleared.Resolve(Capability::Camera, {.origin = origin}) == CapabilityDecision::Ask,
        "reset rules returns to Ask after restart");

    std::filesystem::remove(path, ec);
    std::filesystem::remove(backup, ec);
}

void TestPrivatePermissionRememberingPolicy() {
    using openbrowser::core::ShouldRememberPermissionForOrigin;

    Require(ShouldRememberPermissionForOrigin(true, false),
            "normal mode may remember an explicit site decision");
    Require(!ShouldRememberPermissionForOrigin(false, false),
            "Allow once remains non-persistent in normal mode");
    Require(!ShouldRememberPermissionForOrigin(true, true),
            "private mode never persists an origin decision");
    Require(!ShouldRememberPermissionForOrigin(false, true),
            "private one-shot decision remains non-persistent");
}

}  // namespace

int main() {
    TestPermissionTypesAndStrings();
    TestOriginPermissionManagement();
    TestRememberedOriginPermissionPersistence();
    TestPrivatePermissionRememberingPolicy();

    if (failures != 0) {
        std::cerr << failures << " capability permission test failure(s)\n";
        return 1;
    }

    std::cout << "Openbrowser capability permission tests: PASS\n";
    return 0;
}
