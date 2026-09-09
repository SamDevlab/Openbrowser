#pragma once

#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_panel.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace openbrowser::core {
class BrowserSession;
class WorkspaceManager;
}

class CefBoxLayout;
class CefButtonDelegate;
class CefPanelDelegate;

namespace openbrowser::desktop {

// Applies the user's persisted Aura sidebar preference. The value is retained
// even before the native sidebar exists so startup preferences can be supplied
// by SettingsPanel during construction.
void ApplyGlobalAuraSidebarState(std::string_view state);

class AuraSidebar final : public core::BrowserSessionObserver {
public:
    using ActionCallback = std::function<void()>;
    using WorkspaceSelectCallback = std::function<void(const std::string&)>;

    AuraSidebar(
        core::BrowserSession& session,
        core::WorkspaceManager& workspace_manager,
        WorkspaceSelectCallback on_select_workspace,
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
        SelectWorkspace,
        ToggleFocus,
        ToggleLibrary,
        ToggleDownloads,
        ToggleSettings,
        ToggleNetworkLab,
        ToggleCommands,
        Hide,
    };

    class ActionDelegate;
    class FeedbackDismissTask;
    class SidebarPanelDelegate;

    // Native button delegates defer this call so Refresh() never removes the
    // currently dispatching CefLabelButton from inside OnButtonPressed().
    void HandleAction(Action action, const std::string& target_id);
    void ShowTransientFeedback(std::string message);
    void ClearTransientFeedback(std::uint64_t generation);
    void RelayoutWindow();

    core::BrowserSession& session_;
    core::WorkspaceManager& workspace_manager_;

    WorkspaceSelectCallback on_select_workspace_;
    ActionCallback on_toggle_focus_;
    ActionCallback on_toggle_library_;
    ActionCallback on_toggle_downloads_;
    ActionCallback on_toggle_settings_;
    ActionCallback on_toggle_network_lab_;
    ActionCallback on_toggle_commands_;

    bool collapsed_{true};
    std::string feedback_message_;
    std::uint64_t feedback_generation_{0};
    std::shared_ptr<bool> alive_token_{std::make_shared<bool>(true)};

    CefRefPtr<CefPanelDelegate> panel_delegate_;
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
