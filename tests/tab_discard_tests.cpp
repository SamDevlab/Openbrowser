#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session.h"
#include "core/session/tab_discard_policy.h"
#include "fakes/fake_browser_engine.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestTabActivationSequenceAndLRU() {
    using openbrowser::core::BrowserSession;
    using openbrowser::core::MemoryPressureLevel;
    using openbrowser::core::TabDiscardConfig;
    using openbrowser::core::TabDiscardPolicy;

    openbrowser::tests::FakeBrowserEngine engine;
    BrowserSession session(engine);

    session.OpenTab({.id = "tab-1", .url = "https://1.test"}, true);
    session.OpenTab({.id = "tab-2", .url = "https://2.test"}, true);
    session.OpenTab({.id = "tab-3", .url = "https://3.test"}, true);

    Require(session.ActiveTabId() == "tab-3", "tab-3 is currently active");

    const auto* t1 = session.FindTab("tab-1");
    const auto* t2 = session.FindTab("tab-2");
    const auto* t3 = session.FindTab("tab-3");
    Require(t1 && t2 && t3, "all 3 tabs exist");
    Require(t1->last_activated_sequence == 1, "tab-1 initial sequence is 1");
    Require(t2->last_activated_sequence == 2, "tab-2 initial sequence is 2");
    Require(t3->last_activated_sequence == 3, "tab-3 initial sequence is 3");

    // Activate tab-1
    session.ActivateTab("tab-1");
    Require(session.ActiveTabId() == "tab-1", "tab-1 is active");
    Require(t1->last_activated_sequence == 4, "tab-1 sequence updated to 4 on activation");

    TabDiscardPolicy policy(TabDiscardConfig{.max_alive_background_tabs = 0});
    // Eligible background tabs: tab-2 (seq 2), tab-3 (seq 3). tab-1 is active.
    const auto candidates = policy.SelectDiscardCandidates(session, nullptr, MemoryPressureLevel::Critical);
    Require(candidates.size() == 2, "2 background candidates selected");
    if (candidates.size() == 2) {
        Require(candidates[0] == "tab-2", "tab-2 is least recently used (first to discard)");
        Require(candidates[1] == "tab-3", "tab-3 is next least recently used");
    }
}

void TestDiscardPolicyLimitsAndPressure() {
    using openbrowser::core::BrowserSession;
    using openbrowser::core::MemoryPressureLevel;
    using openbrowser::core::TabDiscardConfig;
    using openbrowser::core::TabDiscardPolicy;

    openbrowser::tests::FakeBrowserEngine engine;
    BrowserSession session(engine);

    for (int i = 1; i <= 5; ++i) {
        session.OpenTab({.id = "tab-" + std::to_string(i), .url = "https://test.com/" + std::to_string(i)}, true);
    }
    // Active is tab-5. Background alive tabs: tab-1, tab-2, tab-3, tab-4 (total 4).

    TabDiscardPolicy policy(TabDiscardConfig{.max_alive_background_tabs = 2});

    // 1. None pressure: max is 2, so 4 - 2 = 2 tabs discarded
    const auto none_candidates = policy.SelectDiscardCandidates(session, nullptr, MemoryPressureLevel::None);
    Require(none_candidates.size() == 2, "None pressure discards excess 2 tabs");
    if (none_candidates.size() == 2) {
        Require(none_candidates[0] == "tab-1", "discards oldest tab-1");
        Require(none_candidates[1] == "tab-2", "discards second oldest tab-2");
    }

    // 2. Moderate pressure: target max is 2 / 2 = 1, so 4 - 1 = 3 tabs discarded
    const auto mod_candidates = policy.SelectDiscardCandidates(session, nullptr, MemoryPressureLevel::Moderate);
    Require(mod_candidates.size() == 3, "Moderate pressure discards down to half (3 tabs)");

    // 3. Critical pressure: discards all 4 background tabs
    const auto crit_candidates = policy.SelectDiscardCandidates(session, nullptr, MemoryPressureLevel::Critical);
    Require(crit_candidates.size() == 4, "Critical pressure discards all 4 background tabs");
}

void TestFocusQueueAndAudioProtection() {
    using openbrowser::core::BrowserSession;
    using openbrowser::core::FocusQueue;
    using openbrowser::core::FocusState;
    using openbrowser::core::MemoryPressureLevel;
    using openbrowser::core::TabDiscardConfig;
    using openbrowser::core::TabDiscardPolicy;

    openbrowser::tests::FakeBrowserEngine engine;
    BrowserSession session(engine);
    FocusQueue queue;

    session.OpenTab({.id = "tab-normal", .url = "https://normal.test"}, false);
    session.OpenTab({.id = "tab-focus-now", .url = "https://focus.test"}, false);
    session.OpenTab({.id = "tab-focus-next", .url = "https://next.test"}, false);
    session.OpenTab({.id = "tab-active", .url = "https://active.test"}, true);

    queue.Enqueue({.id = "item-now", .url = "https://focus.test", .tab_id = "tab-focus-now", .state = FocusState::Now});
    queue.Enqueue({.id = "item-next", .url = "https://next.test", .tab_id = "tab-focus-next", .state = FocusState::Next});

    TabDiscardPolicy policy(TabDiscardConfig{.protect_focus_queue_now = true});
    const auto candidates = policy.SelectDiscardCandidates(session, &queue, MemoryPressureLevel::Critical);

    // Should include tab-normal and tab-focus-next, but NOT tab-focus-now or tab-active
    Require(candidates.size() == 2, "protects active tab and FocusState::Now tab");
    bool has_now = false;
    bool has_active = false;
    for (const auto& id : candidates) {
        if (id == "tab-focus-now") has_now = true;
        if (id == "tab-active") has_active = true;
    }
    Require(!has_now, "FocusState::Now tab is protected from discard");
    Require(!has_active, "Active tab is protected from discard");
}

void TestDiscardAndRevival() {
    using openbrowser::core::BrowserSession;
    using openbrowser::core::TabLifecycle;

    openbrowser::tests::FakeBrowserEngine engine;
    BrowserSession session(engine);

    session.OpenTab({.id = "tab-1", .url = "https://site.test/home"}, true);
    session.OpenTab({.id = "tab-2", .url = "https://site.test/docs"}, true);

    // tab-2 is active, tab-1 is background. Discard tab-1.
    Require(session.DiscardTab("tab-1"), "tab-1 successfully discarded");
    const auto* t1 = session.FindTab("tab-1");
    Require(t1 && t1->lifecycle == TabLifecycle::Discarded, "tab-1 lifecycle is Discarded");

    // Reactivating discarded tab-1 should revive it
    Require(session.ActivateTab("tab-1"), "ActivateTab successfully revives discarded tab-1");
    Require(session.ActiveTabId() == "tab-1", "tab-1 is active");
    Require(t1->lifecycle == TabLifecycle::Active, "tab-1 lifecycle restored to Active");
}

}  // namespace

int main() {
    TestTabActivationSequenceAndLRU();
    TestDiscardPolicyLimitsAndPressure();
    TestFocusQueueAndAudioProtection();
    TestDiscardAndRevival();

    if (failures != 0) {
        std::cerr << failures << " tab discard test failure(s)\n";
        return 1;
    }

    std::cout << "Openbrowser Tab Discard Policy invariants: PASS\n";
    return 0;
}
