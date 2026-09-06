#include "core/capabilities/capability_policy.h"
#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session.h"
#include "fakes/fake_browser_engine.h"

#include <iostream>
#include <optional>
#include <string>
#include <utility>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

openbrowser::core::Tab MakeTab(std::string id, std::string url, std::string title) {
    return openbrowser::core::Tab{
        .id = std::move(id),
        .url = std::move(url),
        .title = std::move(title),
        .lifecycle = openbrowser::core::TabLifecycle::Background,
        .workspace_id = std::nullopt,
    };
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

void TestBrowserSessionActivationInvariant() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(session.OpenTab(MakeTab("a", "https://a.test", "A")), "open first tab");
    Require(session.ActiveTabId() == std::optional<openbrowser::core::TabId>{"a"}, "first tab is active");
    Require(session.OpenTab(MakeTab("b", "https://b.test", "B")), "open second tab");
    Require(session.ActiveTabId() == std::optional<openbrowser::core::TabId>{"b"}, "new active tab replaces previous active tab");

    const auto* a = session.FindTab("a");
    const auto* b = session.FindTab("b");
    Require(a != nullptr && a->lifecycle == openbrowser::core::TabLifecycle::Background, "previous active tab becomes background");
    Require(b != nullptr && b->lifecycle == openbrowser::core::TabLifecycle::Active, "new tab is active");
}

void TestBrowserSessionBackgroundAndSuspension() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(session.OpenTab(MakeTab("a", "https://a.test", "A")), "open active tab");
    Require(session.OpenTab(MakeTab("b", "https://b.test", "B"), false), "open background tab");
    Require(!session.SuspendTab("a"), "active tab cannot be suspended");
    Require(session.SuspendTab("b"), "background tab can be suspended");
    Require(session.FindTab("b")->lifecycle == openbrowser::core::TabLifecycle::Suspended, "background lifecycle becomes suspended");
    Require(session.ActivateTab("b"), "activating suspended tab resumes it");
    Require(session.FindTab("b")->lifecycle == openbrowser::core::TabLifecycle::Active, "resumed tab becomes active");
    Require(engine.Count(openbrowser::tests::EngineCommandType::Resume) == 1, "engine receives resume before activation");
}

void TestBrowserSessionNavigationAndCloseFallback() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(session.OpenTab(MakeTab("a", "https://a.test", "A")), "open tab a");
    Require(session.OpenTab(MakeTab("b", "https://b.test", "B")), "open tab b");
    Require(session.OpenTab(MakeTab("c", "https://c.test", "C"), false), "open tab c in background");

    Require(session.Navigate("b", "https://b.test/docs"), "navigate active tab");
    Require(session.FindTab("b")->url == "https://b.test/docs", "session records requested URL");
    Require(engine.commands.back().type == openbrowser::tests::EngineCommandType::Navigate, "engine receives navigation command");

    Require(session.CloseTab("b"), "close active tab");
    Require(session.ActiveTabId().has_value(), "closing active tab selects fallback");
    Require(*session.ActiveTabId() == "c", "fallback prefers tab at closed index");
    Require(session.FindTab("c")->lifecycle == openbrowser::core::TabLifecycle::Active, "fallback tab becomes active");
}

void TestBrowserSessionNormalizesNewTabLifecycle() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(session.OpenTab(MakeTab("a", "https://a.test", "A")), "open active tab for lifecycle normalization");

    auto suspended = MakeTab("b", "https://b.test", "B");
    suspended.lifecycle = openbrowser::core::TabLifecycle::Suspended;
    Require(session.OpenTab(std::move(suspended), false), "open pre-labeled suspended tab in background");
    Require(
        session.FindTab("b") != nullptr &&
            session.FindTab("b")->lifecycle == openbrowser::core::TabLifecycle::Background,
        "new background tab normalizes suspended lifecycle");

    auto discarded = MakeTab("c", "https://c.test", "C");
    discarded.lifecycle = openbrowser::core::TabLifecycle::Discarded;
    Require(session.OpenTab(std::move(discarded), false), "open pre-labeled discarded tab in background");
    Require(
        session.FindTab("c") != nullptr &&
            session.FindTab("c")->lifecycle == openbrowser::core::TabLifecycle::Background,
        "new background tab normalizes discarded lifecycle");

    Require(
        engine.Count(openbrowser::tests::EngineCommandType::Suspend) == 0,
        "opening normalized tabs does not emit an implicit suspend command");
}

void TestBrowserSessionRejectsInvalidIdentity() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(!session.OpenTab(MakeTab("", "https://a.test", "A")), "reject empty tab id");
    Require(!session.OpenTab(MakeTab("a", "", "A")), "reject empty URL");
    Require(session.OpenTab(MakeTab("a", "https://a.test", "A")), "open valid tab");
    Require(!session.OpenTab(MakeTab("a", "https://duplicate.test", "Duplicate")), "reject duplicate tab id");
}

}  // namespace

int main() {
    TestFocusQueueIdentityAndOrdering();
    TestFocusQueueAllowsUrlWithoutLiveTab();
    TestOnlyOnePromotedNowItem();
    TestCapabilitiesDenyByDefault();
    TestCapabilityPrecedence();
    TestBrowserSessionActivationInvariant();
    TestBrowserSessionBackgroundAndSuspension();
    TestBrowserSessionNavigationAndCloseFallback();
    TestBrowserSessionNormalizesNewTabLifecycle();
    TestBrowserSessionRejectsInvalidIdentity();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser core invariants: PASS\n";
    return 0;
}
