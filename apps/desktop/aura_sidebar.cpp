#include "aura_sidebar.h"

#include "core/session/browser_session.h"
#include "core/workspaces/workspace_manager.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

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
        return CefSize(sidebar_.IsCollapsed() ? 54 : 190, 640);
    }

    CefSize GetMinimumSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(sidebar_.IsCollapsed() ? 50 : 170, 240);
    }

private:
    AuraSidebar& sidebar_;

    IMPLEMENT_REFCOUNTING(SidebarPanelDelegate);
};

class AuraSidebar::ActionDelegate final : public CefButtonDelegate {
public:
    ActionDelegate(AuraSidebar& sidebar, const Action action)
        : sidebar_(sidebar), action_(action) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        sidebar_.HandleAction(action_);
    }

private:
    AuraSidebar& sidebar_;
    Action action_;

    IMPLEMENT_REFCOUNTING(ActionDelegate);
};

AuraSidebar::AuraSidebar(
    core::BrowserSession& session,
    core::WorkspaceManager& workspace_manager,
    ActionCallback on_cycle_workspace,
    ActionCallback on_toggle_focus,
    ActionCallback on_toggle_library,
    ActionCallback on_toggle_downloads,
    ActionCallback on_toggle_settings,
    ActionCallback on_toggle_network_lab,
    ActionCallback on_toggle_commands)
    : session_(session),
      workspace_manager_(workspace_manager),
      on_cycle_workspace_(std::move(on_cycle_workspace)),
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

std::string AuraSidebar::ActiveWorkspaceLabel() const {
    const auto& id = workspace_manager_.ActiveWorkspaceId();
    const auto* workspace = workspace_manager_.FindWorkspace(id);
    if (workspace != nullptr && !workspace->name.empty()) {
        return workspace->name;
    }
    return id.empty() ? "Default" : id;
}

void AuraSidebar::HandleAction(const Action action) {
    CEF_REQUIRE_UI_THREAD();

    switch (action) {
        case Action::ToggleCollapsed:
            ToggleCollapsed();
            return;
        case Action::CycleWorkspace:
            if (on_cycle_workspace_) {
                on_cycle_workspace_();
            }
            Refresh();
            return;
        case Action::ToggleFocus:
            if (on_toggle_focus_) {
                on_toggle_focus_();
            }
            return;
        case Action::ToggleLibrary:
            if (on_toggle_library_) {
                on_toggle_library_();
            }
            return;
        case Action::ToggleDownloads:
            if (on_toggle_downloads_) {
                on_toggle_downloads_();
            }
            return;
        case Action::ToggleSettings:
            if (on_toggle_settings_) {
                on_toggle_settings_();
            }
            return;
        case Action::ToggleNetworkLab:
            if (on_toggle_network_lab_) {
                on_toggle_network_lab_();
            }
            return;
        case Action::ToggleCommands:
            if (on_toggle_commands_) {
                on_toggle_commands_();
            }
            return;
        case Action::Hide:
            SetVisible(false);
            return;
    }
}

void AuraSidebar::Refresh() {
    CEF_REQUIRE_UI_THREAD();
    if (!panel_) {
        return;
    }

    panel_->RemoveAllChildViews();
    delegates_.clear();

    auto add_button = [this](const Action action, const std::string& expanded, const std::string& compact) {
        CefRefPtr<CefButtonDelegate> delegate(new ActionDelegate(*this, action));
        delegates_.push_back(delegate);
        auto button = CefLabelButton::CreateLabelButton(
            delegate,
            collapsed_ ? compact : expanded);
        panel_->AddChildView(button);
        layout_->SetFlexForView(button, 0);
    };

    add_button(Action::ToggleCollapsed, "O  Openbrowser     ‹", "O");
    add_button(
        Action::CycleWorkspace,
        "●  " + ActiveWorkspaceLabel(),
        "●");
    add_button(Action::ToggleFocus, "◎  Focus", "◎");
    add_button(Action::ToggleLibrary, "★  History & Bookmarks", "★");
    add_button(Action::ToggleDownloads, "↓  Downloads", "↓");
    add_button(Action::ToggleSettings, "⚙  Settings", "⚙");
    add_button(Action::ToggleCommands, "⌘  Commands", "⌘");
    add_button(Action::ToggleNetworkLab, "<>  Network Lab", "<>");
    add_button(Action::Hide, "—  Hide sidebar", "—");

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