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

void TestDiscardedTabRejectsHistoryCommands() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(session.OpenTab(MakeTab("active", "https://active.test")), "open active tab");
    Require(session.OpenTab(MakeTab("background", "https://background.test"), false), "open background tab");
    Require(session.DiscardTab("background"), "discard background tab");
    Require(session.FindTab("background")->lifecycle == openbrowser::core::TabLifecycle::Discarded, "tab is discarded");

    const auto command_count_before = engine.commands.size();
    Require(!session.GoBack("background"), "reject back for discarded tab");
    Require(!session.GoForward("background"), "reject forward for discarded tab");
    Require(!session.Reload("background"), "reject reload for discarded tab");
    Require(engine.commands.size() == command_count_before, "discarded tab history commands never cross engine boundary");
}

void TestSuspendedTabResumesBeforeHistoryCommand() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    Require(session.OpenTab(MakeTab("active", "https://active.test")), "open active tab");
    Require(session.OpenTab(MakeTab("background", "https://background.test"), false), "open background tab");

    // Test Back command resumes suspended tab
    Require(session.SuspendTab("background"), "suspend background tab for back");
    auto command_count_before = engine.commands.size();
    Require(session.GoBack("background"), "back resumes suspended tab");
    Require(engine.commands.size() == command_count_before + 2, "resume and back are emitted");
    Require(engine.commands[command_count_before].type == openbrowser::tests::EngineCommandType::Resume, "resume occurs before back");
    Require(engine.commands[command_count_before + 1].type == openbrowser::tests::EngineCommandType::GoBack, "back follows resume");
    Require(
        session.FindTab("background")->lifecycle == openbrowser::core::TabLifecycle::Background,
        "history command leaves resumed non-active tab in background lifecycle");

    // Test Forward command resumes suspended tab
    Require(session.SuspendTab("background"), "suspend background tab for forward");
    command_count_before = engine.commands.size();
    Require(session.GoForward("background"), "forward resumes suspended tab");
    Require(engine.commands.size() == command_count_before + 2, "resume and forward are emitted");
    Require(engine.commands[command_count_before].type == openbrowser::tests::EngineCommandType::Resume, "resume occurs before forward");
    Require(engine.commands[command_count_before + 1].type == openbrowser::tests::EngineCommandType::GoForward, "forward follows resume");
    Require(
        session.FindTab("background")->lifecycle == openbrowser::core::TabLifecycle::Background,
        "forward command leaves resumed non-active tab in background lifecycle");

    // Test Reload command resumes suspended tab
    Require(session.SuspendTab("background"), "suspend background tab for reload");
    command_count_before = engine.commands.size();
    Require(session.Reload("background"), "reload resumes suspended tab");
    Require(engine.commands.size() == command_count_before + 2, "resume and reload are emitted");
    Require(engine.commands[command_count_before].type == openbrowser::tests::EngineCommandType::Resume, "resume occurs before reload");
    Require(engine.commands[command_count_before + 1].type == openbrowser::tests::EngineCommandType::Reload, "reload follows resume");
    Require(
        session.FindTab("background")->lifecycle == openbrowser::core::TabLifecycle::Background,
        "reload command leaves resumed non-active tab in background lifecycle");
}

}  // namespace

int main() {
    TestHistoryCommandsReachEngine();
    TestUnknownTabRejectsHistoryCommands();
    TestDiscardedTabRejectsHistoryCommands();
    TestSuspendedTabResumesBeforeHistoryCommand();

    if (failures != 0) {
        std::cerr << failures << " navigation command assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser navigation commands: PASS\n";
    return 0;
}
