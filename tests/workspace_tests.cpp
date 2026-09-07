#include "core/capabilities/capability_policy.h"
#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session.h"
#include "core/session/session_persistence.h"
#include "core/workspaces/workspace_manager.h"
#include "fakes/fake_browser_engine.h"

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

void TestWorkspaceManagerDefaultsAndListing() {
    openbrowser::core::WorkspaceManager manager;

    Require(manager.HasWorkspace("default"), "Default workspace should exist");
    Require(manager.HasWorkspace("work"), "Work workspace should exist");
    Require(manager.HasWorkspace("personal"), "Personal workspace should exist");

    const auto* def = manager.FindWorkspace("default");
    Require(def != nullptr, "FindWorkspace(default) should return workspace");
    Require(def->name == "Default", "Default workspace name should match");
    Require(!def->is_ephemeral, "Default workspace should not be ephemeral");

    const auto& default_ref = manager.DefaultWorkspace();
    Require(default_ref.id == "default", "DefaultWorkspace() id should be default");

    const auto list = manager.ListWorkspaces();
    Require(list.size() == 3, "Initial list should contain 3 workspaces");
}

void TestWorkspaceCreationAndRemoval() {
    openbrowser::core::WorkspaceManager manager;

    const bool created = manager.CreateWorkspace({
        .id = "finance",
        .name = "Finance & Banking",
        .badge_color = "#EAB308",
        .is_ephemeral = false,
    });
    Require(created, "Creating new workspace finance should succeed");
    Require(manager.HasWorkspace("finance"), "HasWorkspace(finance) should be true");

    const bool duplicate = manager.CreateWorkspace({
        .id = "finance",
        .name = "Duplicate",
        .badge_color = "#FFFFFF",
        .is_ephemeral = false,
    });
    Require(!duplicate, "Creating duplicate workspace should fail");

    const bool remove_default = manager.RemoveWorkspace("default");
    Require(!remove_default, "Removing default workspace should be disallowed");

    const bool removed_finance = manager.RemoveWorkspace("finance");
    Require(removed_finance, "Removing custom workspace should succeed");
    Require(!manager.HasWorkspace("finance"), "HasWorkspace(finance) should now be false");
}

void TestActiveWorkspaceAndCycling() {
    openbrowser::core::WorkspaceManager manager;

    Require(manager.ActiveWorkspaceId() == "default", "Initial active workspace should be default");

    const auto next1 = manager.CycleNextWorkspace();
    Require(next1 != "default", "CycleNextWorkspace should advance from default");
    Require(manager.ActiveWorkspaceId() == next1, "Active workspace should update on cycle");

    const bool set_personal = manager.SetActiveWorkspace("personal");
    Require(set_personal, "SetActiveWorkspace(personal) should succeed");
    Require(manager.ActiveWorkspaceId() == "personal", "Active workspace should now be personal");

    const bool remove_active = manager.RemoveWorkspace("personal");
    Require(!remove_active, "Removing currently active workspace should fail");

    const bool set_invalid = manager.SetActiveWorkspace("nonexistent");
    Require(!set_invalid, "SetActiveWorkspace on unknown id should fail");
}

void TestBrowserSessionWithWorkspaces() {
    openbrowser::tests::FakeBrowserEngine engine;
    openbrowser::core::BrowserSession session(engine);

    openbrowser::core::Tab tab1;
    tab1.id = "tab-work";
    tab1.url = "https://work.internal";
    tab1.title = "Internal Portal";
    tab1.lifecycle = openbrowser::core::TabLifecycle::Active;
    tab1.workspace_id = "work";

    openbrowser::core::Tab tab2;
    tab2.id = "tab-personal";
    tab2.url = "https://social.example";
    tab2.title = "Social";
    tab2.lifecycle = openbrowser::core::TabLifecycle::Background;
    tab2.workspace_id = "personal";

    openbrowser::core::Tab tab3;
    tab3.id = "tab-default";
    tab3.url = "https://docs.example";
    tab3.title = "Docs";
    tab3.lifecycle = openbrowser::core::TabLifecycle::Background;
    tab3.workspace_id = std::nullopt;

    Require(session.OpenTab(std::move(tab1), true), "Opening work tab should succeed");
    Require(session.OpenTab(std::move(tab2), false), "Opening personal tab should succeed");
    Require(session.OpenTab(std::move(tab3), false), "Opening default tab should succeed");

    std::vector<openbrowser::tests::EngineCommand> create_commands;
    for (const auto& cmd : engine.commands) {
        if (cmd.type == openbrowser::tests::EngineCommandType::Create) {
            create_commands.push_back(cmd);
        }
    }

    Require(create_commands.size() == 3, "Engine should receive 3 create commands");
    Require(create_commands[0].workspace_id.has_value() && *create_commands[0].workspace_id == "work",
            "Command 0 should have work workspace");
    Require(create_commands[1].workspace_id.has_value() && *create_commands[1].workspace_id == "personal",
            "Command 1 should have personal workspace");
    Require(!create_commands[2].workspace_id.has_value(),
            "Command 2 should have nullopt workspace");

    const auto* found_work = session.FindTab("tab-work");
    Require(found_work != nullptr && found_work->workspace_id == "work",
            "Session should preserve tab-work workspace");
}

void TestCapabilityPolicyWorkspaceRules() {
    using namespace openbrowser::core;

    auto policy = CapabilityPolicy::CreateDefault();
    policy.SetGlobal(Capability::Geolocation, CapabilityDecision::Deny);
    policy.SetWorkspace("work", Capability::Geolocation, CapabilityDecision::Allow);

    CapabilityContext work_ctx;
    work_ctx.workspace_id = "work";
    work_ctx.origin = "https://internal.corp";

    CapabilityContext personal_ctx;
    personal_ctx.workspace_id = "personal";
    personal_ctx.origin = "https://random.site";

    Require(policy.Resolve(Capability::Geolocation, work_ctx) == CapabilityDecision::Allow,
            "Workspace rule should override global rule to Allow for work workspace");
    Require(policy.Resolve(Capability::Geolocation, personal_ctx) == CapabilityDecision::Deny,
            "Personal workspace without rule should fallback to global Deny");
}

void TestSessionPersistenceWorkspaceRoundTrip() {
    using namespace openbrowser::core;

    openbrowser::tests::FakeBrowserEngine engine;
    BrowserSession session(engine);
    FocusQueue queue;

    Tab t1;
    t1.id = "t1";
    t1.url = "https://work.slack.com";
    t1.title = "Slack";
    t1.lifecycle = TabLifecycle::Active;
    t1.workspace_id = "work";

    Tab t2;
    t2.id = "t2";
    t2.url = "https://youtube.com";
    t2.title = "YouTube";
    t2.lifecycle = TabLifecycle::Background;
    t2.workspace_id = "personal";

    Require(session.OpenTab(std::move(t1), true), "OpenTab t1 should succeed");
    Require(session.OpenTab(std::move(t2), false), "OpenTab t2 should succeed");

    const auto snapshot = SessionPersistence::CaptureSnapshot(session, queue, true);
    const auto serialized = SessionPersistence::Serialize(snapshot);
    const auto parsed = SessionPersistence::Deserialize(serialized);

    Require(parsed.has_value(), "Snapshot deserialization should succeed");
    Require(parsed->tabs.size() == 2, "Snapshot should contain 2 tabs");
    Require(parsed->tabs[0].workspace_id == "work", "First tab workspace should be work");
    Require(parsed->tabs[1].workspace_id == "personal", "Second tab workspace should be personal");

    openbrowser::tests::FakeBrowserEngine engine2;
    BrowserSession restored_session(engine2);
    FocusQueue restored_queue;
    const bool restored = SessionPersistence::RestoreSession(restored_session, restored_queue, *parsed);
    Require(restored, "Restoring session should succeed");

    const auto* restored_t1 = restored_session.FindTab("t1");
    Require(restored_t1 != nullptr && restored_t1->workspace_id == "work",
            "Restored t1 workspace should be work");
    const auto* restored_t2 = restored_session.FindTab("t2");
    Require(restored_t2 != nullptr && restored_t2->workspace_id == "personal",
            "Restored t2 workspace should be personal");
}

}  // namespace

int main() {
    TestWorkspaceManagerDefaultsAndListing();
    TestWorkspaceCreationAndRemoval();
    TestActiveWorkspaceAndCycling();
    TestBrowserSessionWithWorkspaces();
    TestCapabilityPolicyWorkspaceRules();
    TestSessionPersistenceWorkspaceRoundTrip();

    if (failures != 0) {
        std::cerr << "workspace_tests failed with " << failures << " failures.\n";
        return 1;
    }

    std::cout << "All workspace tests passed!\n";
    return 0;
}
