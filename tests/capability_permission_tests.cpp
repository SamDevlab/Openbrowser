#include "core/capabilities/capability_policy.h"
#include "core/capabilities/permission_request.h"

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

    // Default is Ask for Camera and Mic
    Require(
        policy.Resolve(Capability::Camera, {.origin = origin}) == CapabilityDecision::Ask,
        "default camera policy is Ask");
    Require(
        policy.Resolve(Capability::Microphone, {.origin = origin}) == CapabilityDecision::Ask,
        "default mic policy is Ask");
    Require(policy.GetOriginRules(origin).empty(), "origin rules start empty");

    // Grant camera, block microphone
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

    // Clear origin rules
    policy.ClearOriginRules(origin);
    Require(policy.GetOriginRules(origin).empty(), "origin rules cleared");
    Require(
        policy.Resolve(Capability::Camera, {.origin = origin}) == CapabilityDecision::Ask,
        "reverts to default Ask after clear");
}

}  // namespace

int main() {
    TestPermissionTypesAndStrings();
    TestOriginPermissionManagement();

    if (failures != 0) {
        std::cerr << failures << " capability permission test failure(s)\n";
        return 1;
    }

    std::cout << "Openbrowser capability permission tests: PASS\n";
    return 0;
}
