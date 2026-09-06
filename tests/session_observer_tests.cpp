#include "core/session/browser_session.h"
#include "core/session/browser_session_observer.h"
#include "fakes/fake_browser_engine.h"

#include <iostream>
#include <memory>
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

openbrowser::core::Tab MakeTab(std::string id, std::string url) {
    return openbrowser::core::Tab{
        .id = std::move(id),
        .url = std::move(url),
        .title = "Observer test",
        .lifecycle = openbrowser::core::TabLifecycle::Background,
        .workspace_id = std::nullopt,
    };
}

class TestSessionObserver final : public openbrowser::core::BrowserSessionObserver {
public:
    void OnBrowserSessionChanged(const openbrowser::core::BrowserSession& session) override {
        ++notifications;
        last_tab_count = session.Tabs().size();
        last_active_tab_id = session.ActiveTabId();
    }

    int notifications{0};
    std::size_t last_tab_count{0};
    std::optional<openbrowser::core::TabId> last_active_tab_id;
};

void TestObserverReceivesLifecycleAndNavigationEvents() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    TestSessionObserver observer;
    session.AddObserver(&observer);

    Require(observer.notifications == 0, "observer starts with 0 notifications");

    Require(session.OpenTab(MakeTab("tab1", "https://tab1.test")), "open first tab");
    Require(observer.notifications == 1, "observer notified on OpenTab");
    Require(observer.last_tab_count == 1, "observer sees correct tab count");
    Require(observer.last_active_tab_id == std::optional<openbrowser::core::TabId>{"tab1"}, "observer sees active tab id");

    engine.EmitNavigationStarted("tab1", "https://tab1.test");
    Require(observer.notifications == 2, "observer notified on navigation started");

    engine.EmitNavigationCommitted("tab1", "https://tab1.test");
    Require(observer.notifications == 3, "observer notified on navigation committed");

    engine.EmitTitleChanged("tab1", "Tab 1 Title");
    Require(observer.notifications == 4, "observer notified on title changed");

    Require(session.Navigate("tab1", "https://tab1.test/page2"), "navigate tab1");
    Require(observer.notifications == 5, "observer notified on session Navigate");

    Require(session.GoBack("tab1"), "go back on tab1");
    Require(observer.notifications == 6, "observer notified on GoBack");

    Require(session.GoForward("tab1"), "go forward on tab1");
    Require(observer.notifications == 7, "observer notified on GoForward");

    Require(session.Reload("tab1"), "reload tab1");
    Require(observer.notifications == 8, "observer notified on Reload");

    Require(session.OpenTab(MakeTab("tab2", "https://tab2.test"), false), "open background tab2");
    Require(observer.notifications == 9, "observer notified on background OpenTab");

    Require(session.SuspendTab("tab2"), "suspend tab2");
    Require(observer.notifications == 10, "observer notified on SuspendTab");

    Require(session.ResumeTab("tab2"), "resume tab2");
    Require(observer.notifications == 11, "observer notified on ResumeTab");

    Require(session.DiscardTab("tab2"), "discard tab2");
    Require(observer.notifications == 12, "observer notified on DiscardTab");

    engine.EmitNavigationFailed("tab1", "https://fail.test", -105, "failed");
    Require(observer.notifications == 13, "observer notified on navigation failed");

    engine.EmitRendererCrashed("tab1", "crash");
    Require(observer.notifications == 14, "observer notified on renderer crash");

    Require(session.CloseTab("tab2"), "close tab2");
    Require(observer.notifications == 15, "observer notified on CloseTab");
    Require(observer.last_tab_count == 1, "tab count updated after CloseTab");

    session.RemoveObserver(&observer);
}

void TestObserverDeregistration() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    TestSessionObserver observer;
    session.AddObserver(&observer);

    Require(session.OpenTab(MakeTab("a", "https://a.test")), "open tab");
    Require(observer.notifications == 1, "observer receives event before removal");

    session.RemoveObserver(&observer);

    Require(session.OpenTab(MakeTab("b", "https://b.test")), "open second tab after removal");
    Require(session.Navigate("b", "https://b.test/new"), "navigate after removal");
    Require(session.CloseTab("b"), "close after removal");

    Require(observer.notifications == 1, "observer receives no notifications after removal");
}

void TestMultipleObserversAndSafeDestruction() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    auto obs1 = std::make_unique<TestSessionObserver>();
    auto obs2 = std::make_unique<TestSessionObserver>();

    session.AddObserver(obs1.get());
    session.AddObserver(obs2.get());

    Require(session.OpenTab(MakeTab("m1", "https://m1.test")), "open tab with 2 observers");
    Require(obs1->notifications == 1, "obs1 notified");
    Require(obs2->notifications == 1, "obs2 notified");

    // Remove and destroy obs1
    session.RemoveObserver(obs1.get());
    obs1.reset();

    Require(session.OpenTab(MakeTab("m2", "https://m2.test")), "open tab after obs1 destroyed");
    Require(obs2->notifications == 2, "obs2 still notified after obs1 destroyed");

    session.RemoveObserver(obs2.get());
}

}  // namespace

int main() {
    TestObserverReceivesLifecycleAndNavigationEvents();
    TestObserverDeregistration();
    TestMultipleObserversAndSafeDestruction();

    if (failures != 0) {
        std::cerr << failures << " session observer assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser session observer: PASS\n";
    return 0;
}
