#include "core/capabilities/capability_policy.h"

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

void TestSchemeAndOriginExtraction() {
    using openbrowser::core::CapabilityPolicy;

    Require(CapabilityPolicy::ExtractScheme("https://example.com") == std::optional<std::string>{"https"}, "https scheme");
    Require(CapabilityPolicy::ExtractScheme("HTTP://EXAMPLE.COM/PATH") == std::optional<std::string>{"http"}, "http scheme lowercased");
    Require(CapabilityPolicy::ExtractScheme("about:blank") == std::optional<std::string>{"about"}, "about scheme");
    Require(CapabilityPolicy::ExtractScheme("javascript:alert(1)") == std::optional<std::string>{"javascript"}, "javascript scheme");
    Require(!CapabilityPolicy::ExtractScheme("not_a_url").has_value(), "invalid url scheme fails");

    Require(CapabilityPolicy::ExtractOrigin("https://example.com/path/to/page") == std::optional<std::string>{"https://example.com"}, "extract https origin");
    Require(CapabilityPolicy::ExtractOrigin("http://localhost:8080/api?query=1") == std::optional<std::string>{"http://localhost:8080"}, "extract origin with port");
    Require(CapabilityPolicy::ExtractOrigin("HTTPS://SUB.DOMAIN.TEST:3000/") == std::optional<std::string>{"https://sub.domain.test:3000"}, "extract and lowercase origin");
    Require(!CapabilityPolicy::ExtractOrigin("about:blank").has_value(), "about:blank has no hierarchical origin");
}

void TestDefaultPolicyRules() {
    using openbrowser::core::Capability;
    using openbrowser::core::CapabilityDecision;
    using openbrowser::core::CapabilityPolicy;

    const auto policy = CapabilityPolicy::CreateDefault();
    openbrowser::core::CapabilityContext ctx;

    Require(policy.Resolve(Capability::PageNetwork, ctx) == CapabilityDecision::Allow, "PageNetwork allowed by default");
    Require(policy.Resolve(Capability::ThirdPartyStorage, ctx) == CapabilityDecision::Deny, "ThirdPartyStorage denied by default");
    Require(policy.Resolve(Capability::ExternalService, ctx) == CapabilityDecision::Deny, "ExternalService denied by default");
    Require(policy.Resolve(Capability::Camera, ctx) == CapabilityDecision::Ask, "Camera asks by default");
    Require(policy.Resolve(Capability::Microphone, ctx) == CapabilityDecision::Ask, "Microphone asks by default");
    Require(policy.Resolve(Capability::Geolocation, ctx) == CapabilityDecision::Ask, "Geolocation asks by default");
}

void TestNavigationEnforcement() {
    using openbrowser::core::Capability;
    using openbrowser::core::CapabilityDecision;
    using openbrowser::core::CapabilityPolicy;

    auto policy = CapabilityPolicy::CreateDefault();

    Require(policy.CanNavigate("https://example.com/home"), "can navigate to standard https URL");
    Require(policy.CanNavigate("about:blank"), "can navigate to about:blank");
    Require(!policy.CanNavigate("javascript:alert(1)"), "cannot navigate to javascript: pseudo-scheme");
    Require(!policy.CanNavigate(""), "cannot navigate to empty URL");

    // Block a specific origin
    policy.SetOrigin("https://malicious.test", Capability::PageNetwork, CapabilityDecision::Deny);
    Require(!policy.CanNavigate("https://malicious.test/exploit"), "cannot navigate to denied origin");
    Require(policy.CanNavigate("https://safe.test/page"), "can still navigate to other origins");

    // Workspace-level blocking
    policy.SetWorkspace("isolated", Capability::PageNetwork, CapabilityDecision::Deny);
    Require(
        !policy.CanNavigate("https://safe.test", {.workspace_id = "isolated"}),
        "cannot navigate when workspace denies network");
    Require(
        policy.CanNavigate("https://safe.test", {.workspace_id = "general"}),
        "can navigate in general workspace");
}

void TestResourceLoadEnforcement() {
    using openbrowser::core::Capability;
    using openbrowser::core::CapabilityDecision;
    using openbrowser::core::CapabilityPolicy;

    auto policy = CapabilityPolicy::CreateDefault();

    Require(policy.CanLoadResource("https://cdn.example.com/script.js"), "can load standard resource");
    Require(policy.CanLoadResource("data:image/png;base64,iVBORw0KGgo="), "can load data: URI resource");
    Require(!policy.CanLoadResource("javascript:alert(1)"), "cannot load javascript: as resource");

    policy.SetOrigin("https://tracker.test", Capability::PageNetwork, CapabilityDecision::Deny);
    Require(
        !policy.CanLoadResource("https://tracker.test/pixel.gif", "https://example.com"),
        "cannot load resource from denied tracking origin");
    Require(
        policy.CanLoadResource("https://other.test/pixel.gif", "https://example.com"),
        "can load resource from non-denied origin");
}

}  // namespace

int main() {
    TestSchemeAndOriginExtraction();
    TestDefaultPolicyRules();
    TestNavigationEnforcement();
    TestResourceLoadEnforcement();

    if (failures != 0) {
        std::cerr << failures << " capability policy assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser capability policy: PASS\n";
    return 0;
}
