#include "aura_sidebar.h"

#include "aura_accessibility.h"
#include "aura_motion.h"
#include "icon_system.h"
#include "core/session/browser_session.h"
#include "core/workspaces/workspace_manager.h"

#include "include/cef_task.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace openbrowser::desktop {
namespace {

AuraSidebar* g_active_sidebar = nullptr;
std::string g_preferred_sidebar_state{"compact"};

std::string NormalizeSidebarState(const std::string_view state) {
    if (state == "expanded" || state == "hidden") {
        return std::string(state);
    }
    return "compact";
}

IconId WorkspaceIcon(const std::string_view workspace_id) {
    if (workspace_id == "default") {
        return IconId::WorkspaceHome;
    }
    if (workspace_id == "work") {
        return IconId::WorkspaceWork;
    }
    if (workspace_id == "personal") {
        return IconId::WorkspacePersonal;
    }
    return IconId::WorkspaceGeneric;
}

int HexNibble(const char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return 10 + (value - 'a');
    if (value >= 'A' && value <= 'F') return 10 + (value - 'A');
    return -1;
}

cef_color_t WorkspaceColor(const std::string_view value) {
    constexpr std::uint32_t kFallback = 0xFF7567FFu;
    if (value.size() != 7 || value.front() != '#') {
        return static_cast<cef_color_t>(kFallback);
    }

    std::uint32_t rgb = 0;
    for (std::size_t i = 1; i < value.size(); ++i) {
        const int nibble = HexNibble(value[i]);
        if (nibble < 0) {
            return static_cast<cef_color_t>(kFallback);
        }
        rgb = (rgb << 4u) | static_cast<std::uint32_t>(nibble);
    }
    return static_cast<cef_color_t>(0xFF000000u | rgb);
}

class PassiveButtonDelegate final : public CefButtonDelegate {
public:
    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
    }

private:
    IMPLEMENT_REFCOUNTING(PassiveButtonDelegate);
};

class DeferredUiTask final : public CefTask {
public:
    explicit DeferredUiTask(std::function<void()> callback)
        : callback_(std::move(callback)) {}

    void Execute() override {
        CEF_REQUIRE_UI_THREAD();
        if (callback_) {
            callback_();
            callback_ = nullptr;
        }
    }

private:
    std::function<void()> callback_;

    IMPLEMENT_REFCOUNTING(DeferredUiTask);
};

}  // namespace

void ApplyGlobalAuraSidebarState(const std::string_view state) {
    CEF_REQUIRE_UI_THREAD();
    g_preferred_sidebar_state = NormalizeSidebarState(state);

    if (g_active_sidebar == nullptr) {
        return;
    }

    if (g_preferred_sidebar_state == "hidden") {
        g_active_sidebar->SetVisible(false);
        return;
    }

    g_active_sidebar->SetVisible(true);
    g_active_sidebar->SetCollapsed(g_preferred_sidebar_state != "expanded");
}

class AuraSidebar::SidebarPanelDelegate final : public CefPanelDelegate {
public:
    explicit SidebarPanelDelegate(AuraSidebar& sidebar) : sidebar_(sidebar) {}

    CefSize GetPreferredSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(sidebar_.IsCollapsed() ? 54 : 202, 640);
    }

    CefSize GetMinimumSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(sidebar_.IsCollapsed() ? 50 : 180, 240);
    }

private:
    AuraSidebar& sidebar_;

    IMPLEMENT_REFCOUNTING(SidebarPanelDelegate);
};

class AuraSidebar::ActionDelegate final : public CefButtonDelegate {
public:
    ActionDelegate(AuraSidebar& sidebar, const Action action, std::string target_id = {})
        : sidebar_(sidebar), action_(action), target_id_(std::move(target_id)) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();

        // Several Aura actions rebuild the sidebar as part of state/feedback
        // updates. Defer dispatch until this native button callback returns so
        // the currently dispatching CefLabelButton/delegate cannot be removed
        // from the view tree while CEF is still processing its press event.
        AuraSidebar* sidebar = &sidebar_;
        std::weak_ptr<bool> alive_token = sidebar_.alive_token_;
        const Action action = action_;
        const std::string target_id = target_id_;
        CefPostTask(
            TID_UI,
            new DeferredUiTask([sidebar, alive_token, action, target_id]() {
                const auto alive = alive_token.lock();
                if (alive && *alive && sidebar != nullptr) {
                    sidebar->HandleAction(action, target_id);
                }
            }));
    }

private:
    AuraSidebar& sidebar_;
    Action action_;
    std::string target_id_;

    IMPLEMENT_REFCOUNTING(ActionDelegate);
};

class AuraSidebar::FeedbackDismissTask final : public CefTask {
public:
    FeedbackDismissTask(
        AuraSidebar* sidebar,
        std::weak_ptr<bool> alive_token,
        const std::uint64_t generation)
        : sidebar_(sidebar),
          alive_token_(std::move(alive_token)),
          generation_(generation) {}

    void Execute() override {
        auto alive = alive_token_.lock();
        if (alive && *alive && sidebar_ != nullptr) {
            sidebar_->ClearTransientFeedback(generation_);
        }
    }

private:
    AuraSidebar* sidebar_;
    std::weak_ptr<bool> alive_token_;
    std::uint64_t generation_;

    IMPLEMENT_REFCOUNTING(FeedbackDismissTask);
};

AuraSidebar::AuraSidebar(
    core::BrowserSession& session,
    core::WorkspaceManager& workspace_manager,
    WorkspaceSelectCallback on_select_workspace,
    ActionCallback on_toggle_focus,
    ActionCallback on_toggle_library,
    ActionCallback on_toggle_downloads,
    ActionCallback on_toggle_settings,
    ActionCallback on_toggle_network_lab,
    ActionCallback on_toggle_commands)
    : session_(session),
      workspace_manager_(workspace_manager),
      on_select_workspace_(std::move(on_select_workspace)),
      on_toggle_focus_(std::move(on_toggle_focus)),
      on_toggle_library_(std::move(on_toggle_library)),
      on_toggle_downloads_(std::move(on_toggle_downloads)),
      on_toggle_settings_(std::move(on_toggle_settings)),
      on_toggle_network_lab_(std::move(on_toggle_network_lab)),
      on_toggle_commands_(std::move(on_toggle_commands)) {
    session_.AddObserver(this);

    panel_delegate_ = new SidebarPanelDelegate(*this);
    panel_ = CefPanel::CreatePanel(panel_delegate_);

    CefBoxLayoutSettings settings{};
    settings.horizontal = 0;
    settings.between_child_spacing = 5;
    settings.inside_border_horizontal_spacing = 6;
    settings.inside_border_vertical_spacing = 8;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    layout_ = panel_->SetToBoxLayout(settings);

    g_active_sidebar = this;
    collapsed_ = g_preferred_sidebar_state != "expanded";
    panel_->SetVisible(g_preferred_sidebar_state != "hidden");
    Refresh();
}

AuraSidebar::~AuraSidebar() {
    *alive_token_ = false;
    if (g_active_sidebar == this) {
        g_active_sidebar = nullptr;
    }
    session_.RemoveObserver(this);
}

CefRefPtr<CefPanel> AuraSidebar::View() const noexcept {
    return panel_;
}

void AuraSidebar::SetVisible(const bool visible) {
    CEF_REQUIRE_UI_THREAD();
    if (!panel_) {
        return;
    }
    panel_->SetVisible(visible);
    if (visible) {
        Refresh();
    }
    RelayoutWindow();
}

bool AuraSidebar::IsVisible() const {
    return panel_ && panel_->IsVisible();
}

void AuraSidebar::ToggleVisibility() {
    SetVisible(!IsVisible());
}

void AuraSidebar::SetCollapsed(const bool collapsed) {
    CEF_REQUIRE_UI_THREAD();
    if (collapsed_ == collapsed) {
        return;
    }
    collapsed_ = collapsed;
    Refresh();
    if (panel_) {
        panel_->InvalidateLayout();
    }
    RelayoutWindow();
}

void AuraSidebar::ToggleCollapsed() {
    SetCollapsed(!collapsed_);
}

void AuraSidebar::OnBrowserSessionChanged(const core::BrowserSession& /*session*/) {
    CEF_REQUIRE_UI_THREAD();
    Refresh();
}

void AuraSidebar::HandleAction(const Action action, const std::string& target_id) {
    CEF_REQUIRE_UI_THREAD();

    switch (action) {
        case Action::ToggleCollapsed:
            ToggleCollapsed();
            ShowTransientFeedback(collapsed_ ? "Sidebar - compact" : "Sidebar - expanded");
            return;
        case Action::SelectWorkspace:
            if (!target_id.empty() && on_select_workspace_) {
                on_select_workspace_(target_id);
                if (const auto* workspace = workspace_manager_.FindWorkspace(target_id); workspace != nullptr) {
                    ShowTransientFeedback("Workspace - " + workspace->name);
                }
            }
            Refresh();
            return;
        case Action::ToggleFocus:
            if (on_toggle_focus_) {
                on_toggle_focus_();
            }
            ShowTransientFeedback("Focus");
            return;
        case Action::ToggleLibrary:
            if (on_toggle_library_) {
                on_toggle_library_();
            }
            ShowTransientFeedback("History & Bookmarks");
            return;
        case Action::ToggleDownloads:
            if (on_toggle_downloads_) {
                on_toggle_downloads_();
            }
            ShowTransientFeedback("Downloads");
            return;
        case Action::ToggleSettings:
            if (on_toggle_settings_) {
                on_toggle_settings_();
            }
            ShowTransientFeedback("Settings");
            return;
        case Action::ToggleNetworkLab:
            if (on_toggle_network_lab_) {
                on_toggle_network_lab_();
            }
            ShowTransientFeedback("Network Lab");
            return;
        case Action::ToggleCommands:
            if (on_toggle_commands_) {
                on_toggle_commands_();
            }
            ShowTransientFeedback("Commands");
            return;
        case Action::Hide:
            SetVisible(false);
            return;
    }
}

void AuraSidebar::ShowTransientFeedback(std::string message) {
    CEF_REQUIRE_UI_THREAD();
    if (!panel_ || !panel_->IsVisible()) {
        return;
    }

    feedback_message_ = std::move(message);
    const std::uint64_t generation = ++feedback_generation_;
    Refresh();
    CefPostDelayedTask(
        TID_UI,
        new FeedbackDismissTask(this, alive_token_, generation),
        aura::kTransientFeedbackDurationMs);
}

void AuraSidebar::ClearTransientFeedback(const std::uint64_t generation) {
    CEF_REQUIRE_UI_THREAD();
    if (generation != feedback_generation_ || feedback_message_.empty()) {
        return;
    }
    feedback_message_.clear();
    Refresh();
}

void AuraSidebar::Refresh() {
    CEF_REQUIRE_UI_THREAD();
    if (!panel_) {
        return;
    }

    panel_->RemoveAllChildViews();
    delegates_.clear();

    auto add_button = [this](
        const Action action,
        const IconId icon,
        const std::string& expanded,
        const std::string& accessible_name,
        const std::string& tooltip,
        std::string target_id = {}) {
        CefRefPtr<CefButtonDelegate> delegate(
            new ActionDelegate(*this, action, std::move(target_id)));
        delegates_.push_back(delegate);
        auto button = CefLabelButton::CreateLabelButton(
            delegate,
            collapsed_ ? "" : expanded);
        ConfigureIconButton(button, icon, collapsed_ ? "" : expanded, accessible_name, tooltip);
        aura::EnableButtonMotion(button);
        aura::ConfigureActionButton(
            button,
            accessible_name,
            tooltip,
            aura::kSidebarAccessibilityGroupId);
        panel_->AddChildView(button);
        layout_->SetFlexForView(button, 0);
        return button;
    };

    add_button(
        Action::ToggleCollapsed,
        collapsed_ ? IconId::Sidebar : IconId::SidebarCollapse,
        "Openbrowser",
        collapsed_ ? "Expand Aura sidebar" : "Compact Aura sidebar",
        collapsed_ ? "Expand Aura sidebar" : "Compact Aura sidebar");

    const auto& active_workspace_id = workspace_manager_.ActiveWorkspaceId();
    for (const auto& workspace : workspace_manager_.ListWorkspaces()) {
        const bool is_active = workspace.id == active_workspace_id;
        const std::string expanded = is_active
            ? workspace.name + " (active)"
            : workspace.name;
        const std::string accessible_name = is_active
            ? "Workspace " + workspace.name + ", active"
            : "Switch to workspace " + workspace.name;
        const std::string tooltip = is_active
            ? workspace.name + " - Active workspace"
            : "Switch to " + workspace.name;
        auto button = add_button(
            Action::SelectWorkspace,
            WorkspaceIcon(workspace.id),
            expanded,
            accessible_name,
            tooltip,
            workspace.id);
        const auto workspace_color = WorkspaceColor(workspace.badge_color);
        if (is_active) {
            aura::ApplySelectedAccent(button, workspace_color);
        } else {
            aura::ApplyHoverAccent(button, workspace_color);
        }
    }

    add_button(Action::ToggleFocus, IconId::Focus, "Focus", "Open Focus", "Open Focus");
    add_button(
        Action::ToggleLibrary,
        IconId::Library,
        "History & Bookmarks",
        "Open History and Bookmarks",
        "Open History & Bookmarks");
    add_button(Action::ToggleDownloads, IconId::Download, "Downloads", "Open Downloads", "Open Downloads");
    add_button(Action::ToggleSettings, IconId::Settings, "Settings", "Open Settings", "Open Settings");
    add_button(Action::ToggleCommands, IconId::Commands, "Commands", "Open Commands", "Open Commands");
    add_button(
        Action::ToggleNetworkLab,
        IconId::Network,
        "Network Lab",
        "Open Network Lab",
        "Open Network Lab");
    add_button(
        Action::Hide,
        IconId::SidebarCollapse,
        "Hide sidebar",
        "Hide Aura sidebar",
        "Hide Aura sidebar - Ctrl+Shift+\\ restores it");

    if (!feedback_message_.empty() && !collapsed_) {
        CefRefPtr<CefButtonDelegate> feedback_delegate(new PassiveButtonDelegate());
        delegates_.push_back(feedback_delegate);
        auto feedback = CefLabelButton::CreateLabelButton(
            feedback_delegate, "Status: " + feedback_message_);
        aura::ConfigurePassiveStatus(feedback, "Status: " + feedback_message_);
        feedback->SetEnabled(false);
        panel_->AddChildView(feedback);
        layout_->SetFlexForView(feedback, 0);
    }

    panel_->InvalidateLayout();
    panel_->Layout();
    RelayoutWindow();
}

void AuraSidebar::RelayoutWindow() {
    if (!panel_) {
        return;
    }
    auto window = panel_->GetWindow();
    if (window) {
        window->Layout();
    }
}

}  // namespace openbrowser::desktop
