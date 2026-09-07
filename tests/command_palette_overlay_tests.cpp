#include "core/commands/action_registry.h"
#include "core/commands/command_palette.h"

#include <iostream>
#include <string>

namespace {
void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

void TestCommandPaletteOverlayExecution() {
    using namespace openbrowser::core;

    ActionRegistry registry;
    bool navigated = false;
    bool lab_toggled = false;

    registry.RegisterAction({
        .id = "nav.reload",
        .title = "Reload Current Tab",
        .description = "Reload active tab content",
        .category = ActionCategory::Navigation,
        .shortcut_hint = "Ctrl+R",
        .handler = [&navigated]() {
            navigated = true;
            return true;
        }
    });

    registry.RegisterAction({
        .id = "network_lab.toggle",
        .title = "Toggle Network Lab",
        .description = "Show or hide developer network inspector",
        .category = ActionCategory::NetworkLab,
        .shortcut_hint = "Ctrl+Shift+L",
        .handler = [&lab_toggled]() {
            lab_toggled = true;
            return true;
        }
    });

    CommandPalette palette(registry);

    // Search query "reload"
    auto results = palette.Search("reload");
    Require(!results.empty(), "Found reload action");
    Require(results[0].action.id == "nav.reload", "Top result is nav.reload");

    // Execute top result
    Require(registry.ExecuteAction(results[0].action.id), "Dispatched nav.reload");
    Require(navigated, "Navigated flag set");
    Require(!lab_toggled, "Lab not toggled");

    // Search query "network"
    auto lab_results = palette.Search("network");
    Require(!lab_results.empty(), "Found network action");
    Require(lab_results[0].action.id == "network_lab.toggle", "Top result is network_lab.toggle");
    Require(registry.ExecuteAction(lab_results[0].action.id), "Dispatched network_lab.toggle");
    Require(lab_toggled, "Lab toggled flag set");
}

void TestCategoryFilteringAndFormatting() {
    using namespace openbrowser::core;

    ActionRegistry registry;
    registry.RegisterAction({
        .id = "focus.timer",
        .title = "Start Focus Sprint",
        .description = "Start a 25-minute Pomodoro timer",
        .category = ActionCategory::Focus,
        .shortcut_hint = "Alt+F",
        .handler = []() { return true; }
    });

    CommandPalette palette(registry);
    auto results = palette.Search("");
    Require(!results.empty(), "Found all actions with empty query");
    Require(ActionCategoryToString(results[0].action.category) == "Focus", "Category is Focus");
}

} // namespace

int main() {
    TestCommandPaletteOverlayExecution();
    TestCategoryFilteringAndFormatting();

    std::cout << "All CommandPaletteOverlay tests passed successfully." << std::endl;
    return 0;
}
