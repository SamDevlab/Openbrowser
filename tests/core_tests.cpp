#include "core/capabilities/capability_policy.h"
#include "core/focus_queue/focus_queue.h"

#include <iostream>
#include <optional>
#include <string>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestFocusQueueIdentityAndOrdering() {
    openbrowser::core::FocusQueue queue;

    Require(queue.Enqueue({.id = "docs", .url = "https://example.test/docs", .tab_id = std::nullopt, .workspace_id = std::nullopt, .state = openbrowser::core::FocusState::Next}), "enqueue docs");
    Require(queue.Enqueue({.id = "issue", .url = "https://example.test/issue", .tab_id = std::nullopt, .workspace_id = std::nullopt, .state = openbrowser::core::FocusState::Next}), "enqueue issue");
    Require(!queue.Enqueue({.id = "docs", .url = "https://example.test/duplicate", .tab_id = std::nullopt, .workspace_id = std::nullopt, .state = openbrowser::core::FocusState::Next}), "reject duplicate queue id");
    Require(queue.Move("issue", 0), "move issue to front");
    Require(queue.Items().front().id == "issue", "queue order follows explicit priority");
}

void TestFocusQueueAllowsUrlWithoutLiveTab() {
    openbrowser::core::FocusQueue queue;
    Require(
        queue.Enqueue({.id = "later", .url = "https://example.test/later", .tab_id = std::nullopt, .workspace_id = std::nullopt, .state = openbrowser::core::FocusState::Later}),
        "enqueue URL-only focus item");

    const auto* item = queue.Find("later");
    Require(item != nullptr, "find URL-only item");
    Require(item != nullptr && !item->tab_id.has_value(), "focus item does not require a live tab");
}

void TestOnlyOnePromotedNowItem() {
    openbrowser::core::FocusQueue queue;
    Require(queue.Enqueue({.id = "a", .url = "https://a.test", .tab_id = std::nullopt, .workspace_id = std::nullopt, .state = openbrowser::core::FocusState::Next}), "enqueue a");
    Require(queue.Enqueue({.id = "b", .url = "https://b.test", .tab_id = std::nullopt, .workspace_id = std::nullopt, .state = openbrowser::core::FocusState::Next}), "enqueue b");
    Require(queue.PromoteToNow("a"), "promote a");
    Require(queue.PromoteToNow("b"), "promote b");

    const auto* a = queue.Find("a");
    const auto* b = queue.Find("b");
    Require(a != nullptr && a->state == openbrowser::core::FocusState::Next, "previous now item is demoted");
    Require(b != nullptr && b->state == openbrowser::core::FocusState::Now, "selected item becomes now");
    Require(queue.Items().front().id == "b", "now item moves to front");
}

void TestCapabilitiesDenyByDefault() {
    openbrowser::core::CapabilityPolicy policy;
    const openbrowser::core::CapabilityContext context{};

    Require(
        policy.Resolve(openbrowser::core::Capability::Sync, context) == openbrowser::core::CapabilityDecision::Deny,
        "undeclared capability is denied");
    Require(
        policy.Resolve(openbrowser::core::Capability::CrashReport, context) == openbrowser::core::CapabilityDecision::Deny,
        "undeclared browser egress is denied");
}

void TestCapabilityPrecedence() {
    using openbrowser::core::Capability;
    using openbrowser::core::CapabilityContext;
    using openbrowser::core::CapabilityDecision;
    using openbrowser::core::CapabilityPolicy;

    CapabilityPolicy policy;
    policy.SetGlobal(Capability::PageNetwork, CapabilityDecision::Allow);
    policy.SetWorkspace("work", Capability::PageNetwork, CapabilityDecision::Ask);
    policy.SetOrigin("https://private.test", Capability::PageNetwork, CapabilityDecision::Deny);
    policy.SetSession("temporary-override", Capability::PageNetwork, CapabilityDecision::Allow);

    Require(
        policy.Resolve(Capability::PageNetwork, CapabilityContext{}) == CapabilityDecision::Allow,
        "global policy applies without narrower scope");
    Require(
        policy.Resolve(Capability::PageNetwork, CapabilityContext{.workspace_id = "work", .origin = std::nullopt, .session_id = std::nullopt}) == CapabilityDecision::Ask,
        "workspace overrides global");
    Require(
        policy.Resolve(
            Capability::PageNetwork,
            CapabilityContext{.workspace_id = "work", .origin = "https://private.test", .session_id = std::nullopt}) == CapabilityDecision::Deny,
        "origin overrides workspace");
    Require(
        policy.Resolve(
            Capability::PageNetwork,
            CapabilityContext{
                .workspace_id = "work",
                .origin = "https://private.test",
                .session_id = "temporary-override"}) == CapabilityDecision::Allow,
        "session override has highest precedence");
}

}  // namespace

int main() {
    TestFocusQueueIdentityAndOrdering();
    TestFocusQueueAllowsUrlWithoutLiveTab();
    TestOnlyOnePromotedNowItem();
    TestCapabilitiesDenyByDefault();
    TestCapabilityPrecedence();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser core invariants: PASS\n";
    return 0;
}
