#pragma once

#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_panel.h"

#include <functional>
#include <string>
#include <vector>

namespace openbrowser::core {
class BrowserSession;
class WorkspaceManager;
}

class CefBoxLayout;
class CefButtonDelegate;
class CefPanelDelegate;

namespace openbrowser::desktop {

class AuraSidebar final : public core::BrowserSessionObserver {
public:
    using ActionCallback = std::function<void()>;

    AuraSidebar(
        core::BrowserSession& session,
        core::WorkspaceManager& workspace_manager,
        ActionCallback on_cycle_workspace,
        ActionCallback on_toggle_focus,
        ActionCallback on_toggle_library,
        ActionCallback on_toggle_downloads,
        ActionCallback on_toggle_settings,
        ActionCallback on_toggle_network_lab,
        ActionCallback on_toggle_commands);
    ~AuraSidebar() override;

    AuraSidebar(const AuraSidebar&) = delete;
    AuraSidebar& operator=(const AuraSidebar&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();

    void SetCollapsed(bool collapsed);
    [[nodiscard]] bool IsCollapsed() const noexcept { return collapsed_; }
    void ToggleCollapsed();
    void Refresh();

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    enum class Action {
        ToggleCollapsed,
        CycleWorkspace,
        ToggleFocus,
        ToggleLibrary,
        ToggleDownloads,
        ToggleSettings,
        ToggleNetworkLab,
        ToggleCommands,
        Hide,
    };

    class ActionDelegate;
    class SidebarPanelDelegate;

    void HandleAction(Action action);
    [[nodiscard]] std::string ActiveWorkspaceLabel() const;
    void RelayoutWindow();

    core::BrowserSession& session_;
    core::WorkspaceManager& workspace_manager_;

    ActionCallback on_cycle_workspace_;
    ActionCallback on_toggle_focus_;
    ActionCallback on_toggle_library_;
    ActionCallback on_toggle_downloads_;
    ActionCallback on_toggle_settings_;
    ActionCallback on_toggle_network_lab_;
    ActionCallback on_toggle_commands_;

    bool collapsed_{true};

    CefRefPtr<CefPanelDelegate> panel_delegate_;
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
