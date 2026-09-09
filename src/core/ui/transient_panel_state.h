#pragma once

namespace openbrowser::core {

// The desktop shell has several mutually exclusive transient drawers. This
// small engine-independent state machine keeps their logical state separate
// from the CEF view objects and gives UI code one authoritative transition
// point.
enum class TransientPanel {
    None,
    Focus,
    Library,
    Downloads,
    Settings,
    NetworkLab,
};

class TransientPanelState final {
public:
    void Open(TransientPanel panel) noexcept;
    void Close() noexcept;
    void Toggle(TransientPanel panel) noexcept;

    [[nodiscard]] TransientPanel Active() const noexcept { return active_; }
    [[nodiscard]] bool IsOpen(TransientPanel panel) const noexcept {
        return active_ == panel;
    }

private:
    TransientPanel active_{TransientPanel::None};
};

}  // namespace openbrowser::core
