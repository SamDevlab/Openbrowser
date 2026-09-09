#include "core/ui/transient_panel_state.h"

namespace openbrowser::core {

void TransientPanelState::Open(const TransientPanel panel) noexcept {
    if (panel == TransientPanel::None) {
        Close();
        return;
    }
    active_ = panel;
}

void TransientPanelState::Close() noexcept {
    active_ = TransientPanel::None;
}

void TransientPanelState::Toggle(const TransientPanel panel) noexcept {
    if (IsOpen(panel)) {
        Close();
    } else {
        Open(panel);
    }
}

}  // namespace openbrowser::core
