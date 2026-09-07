#include "core/commands/action_registry.h"
#include "core/commands/command_palette.h"

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

void TestActionRegistryRegistrationAndExecution() {
    using namespace openbrowser::core;

    ActionRegistry registry;
    Require(registry.TotalActions() == 0, "Initial actions count is 0");

    bool executed = false;
    ActionDefinition action1;
    action1.id = "tab.new";
    action1.title = "New Tab";
    action1.description = "Open a new browser tab";
    action1.category = ActionCategory::Navigation;
    action1.shortcut_hint = "Ctrl+T";
    action1.handler = [&executed]() {
        executed = true;
        return true;
    };

    Require(registry.RegisterAction(action1), "RegisterAction succeeds");
    Require(registry.TotalActions() == 1, "Action count is 1");
    Require(registry.HasAction("tab.new"), "HasAction returns true");
    Require(!registry.RegisterAction(action1), "Duplicate registration rejected");

    Require(registry.ExecuteAction("tab.new"), "ExecuteAction succeeds");
    Require(executed, "Action handler was executed");

    Require(!registry.ExecuteAction("nonexistent"), "Execute non-existent action fails");

    Require(registry.UnregisterAction("tab.new"), "UnregisterAction succeeds");
    Require(registry.TotalActions() == 0, "Action count back to 0");
}

void TestCommandPaletteSearchAndRanking() {
    using namespace openbrowser::core;

    ActionRegistry registry;

    ActionDefinition a1;
    a1.id = "tab.new";
    a1.title = "New Tab";
    a1.category = ActionCategory::Navigation;
    a1.handler = []() { return true; };
    registry.RegisterAction(a1);

    ActionDefinition a2;
    a2.id = "tab.close";
    a2.title = "Close Tab";
    a2.category = ActionCategory::Navigation;
    a2.handler = []() { return true; };
    registry.RegisterAction(a2);

    ActionDefinition a3;
    a3.id = "network_lab.toggle";
    a3.title = "Toggle Network Lab";
    a3.category = ActionCategory::NetworkLab;
    a3.handler = []() { return true; };
    registry.RegisterAction(a3);

    CommandPalette palette(registry);

    // Empty query returns all actions
    const auto all = palette.Search("");
    Require(all.size() == 3, "Empty query returns all 3 actions");

    // Search "tab" matches tab.new and tab.close
    const auto tabs = palette.Search("tab");
    Require(tabs.size() == 2, "Search 'tab' returns 2 actions");

    // Search "Network" matches network_lab.toggle
    const auto net = palette.Search("Network");
    Require(net.size() == 1, "Search 'Network' returns 1 action");
    Require(net[0].action.id == "network_lab.toggle", "Matched action is network_lab.toggle");

    // Category filter
    const auto nav_only = palette.Search("", ActionCategory::Navigation);
    Require(nav_only.size() == 2, "Category filter returns 2 Navigation actions");

    const auto net_only = palette.Search("", ActionCategory::NetworkLab);
    Require(net_only.size() == 1, "Category filter returns 1 NetworkLab action");

    // Non-existent search
    const auto none = palette.Search("xyz12345");
    Require(none.empty(), "Unmatched query returns empty list");
}

}  // namespace

int main() {
    TestActionRegistryRegistrationAndExecution();
    TestCommandPaletteSearchAndRanking();

    if (failures != 0) {
        std::cerr << failures << " CommandPalette test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser CommandPalette & ActionRegistry invariants: PASS\n";
    return 0;
}
