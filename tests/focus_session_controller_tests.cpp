#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session.h"
#include "core/session/focus_session_controller.h"
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
        .title = "Focus Controller Test",
        .lifecycle = openbrowser::core::TabLifecycle::Background,
        .workspace_id = std::nullopt,
    };
}

void TestEnqueueActiveTab() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);
    openbrowser::core::FocusQueue queue;

    Require(!openbrowser::core::FocusSessionController::EnqueueActiveTab(session, queue), "cannot enqueue without active tab");

    Require(session.OpenTab(MakeTab("t1", "https://t1.test")), "open active tab t1");
    Require(openbrowser::core::FocusSessionController::EnqueueActiveTab(session, queue, "item1"), "enqueue active tab t1");

    const auto* item = queue.Find("item1");
    Require(item != nullptr, "item1 exists in focus queue");
    Require(item != nullptr && item->url == "https://t1.test", "item1 has tab url");
    Require(item != nullptr && item->tab_id == std::optional<openbrowser::core::TabId>{"t1"}, "item1 has tab_id t1");

    Require(!openbrowser::core::FocusSessionController::EnqueueActiveTab(session, queue, "item1"), "cannot enqueue duplicate item id");
}

void TestActivateFocusItemWithLiveTab() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);
    openbrowser::core::FocusQueue queue;

    Require(session.OpenTab(MakeTab("t1", "https://t1.test")), "open tab t1");
    Require(openbrowser::core::FocusSessionController::EnqueueActiveTab(session, queue, "item1"), "enqueue t1");

    Require(session.OpenTab(MakeTab("t2", "https://t2.test")), "open tab t2");
    Require(session.ActiveTabId() == std::optional<openbrowser::core::TabId>{"t2"}, "t2 is now active");

    Require(openbrowser::core::FocusSessionController::ActivateFocusItem(session, queue, "item1"), "activate focus item1");
    Require(session.ActiveTabId() == std::optional<openbrowser::core::TabId>{"t1"}, "activating focus item1 activates live tab t1");
}

void TestActivateFocusItemReopensClosedTab() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);
    openbrowser::core::FocusQueue queue;

    Require(session.OpenTab(MakeTab("t1", "https://t1.test")), "open tab t1");
    Require(openbrowser::core::FocusSessionController::EnqueueActiveTab(session, queue, "item1"), "enqueue t1");

    Require(session.CloseTab("t1"), "close tab t1");
    Require(session.Tabs().empty(), "session has no open tabs");

    openbrowser::core::FocusSessionController::SynchronizeTabClosures(session, queue);
    const auto* item = queue.Find("item1");
    Require(item != nullptr, "focus item1 persists in queue after tab closed");
    Require(item != nullptr && !item->tab_id.has_value(), "focus item1 has tab_id cleared after tab closed");

    Require(openbrowser::core::FocusSessionController::ActivateFocusItem(session, queue, "item1"), "activate focus item1 reopens tab");
    Require(session.Tabs().size() == 1, "new tab opened for focus item");
    Require(session.Tabs().front().url == "https://t1.test", "reopened tab has item url");
    Require(queue.Find("item1")->tab_id.has_value(), "focus item now bound to new tab_id");
}

}  // namespace

int main() {
    TestEnqueueActiveTab();
    TestActivateFocusItemWithLiveTab();
    TestActivateFocusItemReopensClosedTab();

    if (failures != 0) {
        std::cerr << failures << " focus session controller assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser focus session controller: PASS\n";
    return 0;
}
