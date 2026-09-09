#include "core/ui/transient_panel_state.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

void TestOpenAndClose() {
    using openbrowser::core::TransientPanel;
    using openbrowser::core::TransientPanelState;

    TransientPanelState state;
    Require(state.Active() == TransientPanel::None, "starts closed");

    state.Open(TransientPanel::Focus);
    Require(state.IsOpen(TransientPanel::Focus), "focus opens");
    Require(!state.IsOpen(TransientPanel::Library), "other panel stays closed");

    state.Close();
    Require(state.Active() == TransientPanel::None, "close clears active panel");
}

void TestToggleIsMutuallyExclusive() {
    using openbrowser::core::TransientPanel;
    using openbrowser::core::TransientPanelState;

    TransientPanelState state;
    state.Toggle(TransientPanel::Downloads);
    Require(state.IsOpen(TransientPanel::Downloads), "downloads opens on toggle");

    state.Toggle(TransientPanel::Settings);
    Require(state.IsOpen(TransientPanel::Settings), "opening another panel replaces downloads");
    Require(!state.IsOpen(TransientPanel::Downloads), "downloads is no longer active");

    state.Toggle(TransientPanel::Settings);
    Require(state.Active() == TransientPanel::None, "toggling active panel closes it");
}

void TestNoneAlwaysCloses() {
    using openbrowser::core::TransientPanel;
    using openbrowser::core::TransientPanelState;

    TransientPanelState state;
    state.Open(TransientPanel::NetworkLab);
    state.Open(TransientPanel::None);
    Require(state.Active() == TransientPanel::None, "opening None closes the state");
}

}  // namespace

int main() {
    TestOpenAndClose();
    TestToggleIsMutuallyExclusive();
    TestNoneAlwaysCloses();
    std::cout << "All transient panel state tests passed successfully." << std::endl;
    return 0;
}
