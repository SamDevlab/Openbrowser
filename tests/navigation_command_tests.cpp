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

openbrowser::core::Tab MakeTab(std::string id, std::string url) {
    return openbrowser::core::Tab{
        .id = std::move(id),
        .url = std::move(url),
        .title = "Navigation test",
        .lifecycle = openbrowser::core::TabLifecycle::Background,
        .workspace_id = std::nullopt,
    };
}

void TestHistoryCommandsReachEngine() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(session.OpenTab(MakeTab("a", "https://a.test")), "open navigation command tab");
    engine.EmitNavigationCommitted("a", "https://a.test");

    Require(session.GoBack("a"), "back command accepted");
    Require(session.GoForward("a"), "forward command accepted");
    Require(session.Reload("a"), "reload command accepted");

    Require(engine.Count(openbrowser::tests::EngineCommandType::GoBack) == 1, "engine receives one back command");
    Require(engine.Count(openbrowser::tests::EngineCommandType::GoForward) == 1, "engine receives one forward command");
    Require(engine.Count(openbrowser::tests::EngineCommandType::Reload) == 1, "engine receives one reload command");
    Require(session.FindTab("a")->url == "https://a.test", "history commands do not invent committed URLs");
}

void TestUnknownTabRejectsHistoryCommands() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(!session.GoBack("missing"), "reject back for unknown tab");
    Require(!session.GoForward("missing"), "reject forward for unknown tab");
    Require(!session.Reload("missing"), "reject reload for unknown tab");
    Require(engine.commands.empty(), "invalid history commands never cross engine boundary");
}

void TestSuspendedTabResumesBeforeHistoryCommand() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(session.OpenTab(MakeTab("active", "https://active.test")), "open active tab");
    Require(session.OpenTab(MakeTab("background", "https://background.test"), false), "open background tab");
    Require(session.SuspendTab("background"), "suspend background tab");

    const auto command_count_before_back = engine.commands.size();
    Require(session.GoBack("background"), "back resumes suspended tab");
    Require(engine.commands.size() == command_count_before_back + 2, "resume and back are emitted");
    Require(engine.commands[command_count_before_back].type == openbrowser::tests::EngineCommandType::Resume, "resume occurs before history navigation");
    Require(engine.commands[command_count_before_back + 1].type == openbrowser::tests::EngineCommandType::GoBack, "back follows resume");
    Require(
        session.FindTab("background")->lifecycle == openbrowser::core::TabLifecycle::Background,
        "history command leaves resumed non-active tab in background lifecycle");
}

}  // namespace

int main() {
    TestHistoryCommandsReachEngine();
    TestUnknownTabRejectsHistoryCommands();
    TestSuspendedTabResumesBeforeHistoryCommand();

    if (failures != 0) {
        std::cerr << failures << " navigation command assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser navigation commands: PASS\n";
    return 0;
}
