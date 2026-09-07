#include "tab_strip.h"

#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/workspaces/workspace_manager.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include <chrono>
#include <string>
#include <utility>

namespace openbrowser::desktop {

class TabStrip::TabActionDelegate final : public CefButtonDelegate {
public:
    TabActionDelegate(TabStrip& tab_strip, const TabAction action, std::string tab_id)
        : tab_strip_(tab_strip), action_(action), tab_id_(std::move(tab_id)) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        tab_strip_.HandleTabAction(action_, tab_id_);
    }

private:
    TabStrip& tab_strip_;
    TabAction action_;
    std::string tab_id_;

    IMPLEMENT_REFCOUNTING(TabActionDelegate);
};

TabStrip::TabStrip(
    core::BrowserSession& session,
    core::WorkspaceManager* workspace_manager,
    core::ProfileManager* profile_manager)
    : session_(session),
      workspace_manager_(workspace_manager),
      profile_manager_(profile_manager) {
    session_.AddObserver(this);

    panel_ = CefPanel::CreatePanel(nullptr);

    CefBoxLayoutSettings settings{};
    settings.horizontal = 1;
    settings.between_child_spacing = 4;
    settings.inside_border_horizontal_spacing = 8;
    settings.inside_border_vertical_spacing = 4;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    layout_ = panel_->SetToBoxLayout(settings);

    RebuildTabs();
}

TabStrip::~TabStrip() {
    session_.RemoveObserver(this);
}

CefRefPtr<CefPanel> TabStrip::View() const noexcept {
    return panel_;
}

void TabStrip::OnBrowserSessionChanged(const core::BrowserSession& /*session*/) {
    CEF_REQUIRE_UI_THREAD();
    RebuildTabs();
}

void TabStrip::HandleTabAction(const TabAction action, const std::string& tab_id) {
    CEF_REQUIRE_UI_THREAD();

    switch (action) {
        case TabAction::Activate:
            if (!tab_id.empty()) {
                static_cast<void>(session_.ActivateTab(tab_id));
            }
            break;
        case TabAction::Close:
            if (!tab_id.empty()) {
                static_cast<void>(session_.CloseTab(tab_id));
            }
            break;
        case TabAction::CycleWorkspace:
            if (workspace_manager_ != nullptr) {
                workspace_manager_->CycleNextWorkspace();
                RebuildTabs();
            }
            break;
        case TabAction::NewTab: {
            std::string new_id = "tab-" + std::to_string(++next_tab_index_);
            while (session_.FindTab(new_id) != nullptr) {
                new_id = "tab-" + std::to_string(++next_tab_index_);
            }
            std::optional<core::WorkspaceId> ws_id = std::nullopt;
            if (workspace_manager_ != nullptr) {
                ws_id = workspace_manager_->ActiveWorkspaceId();
            }
            const bool is_ephemeral = (profile_manager_ != nullptr &&
                                       profile_manager_->GetActiveProfile() != nullptr &&
                                       profile_manager_->GetActiveProfile()->IsEphemeral());
            core::Tab new_tab;
            new_tab.id = new_id;
            new_tab.url = "https://example.com/";
            new_tab.title = is_ephemeral ? "Private Tab" : "New Tab";
            new_tab.lifecycle = core::TabLifecycle::Active;
            new_tab.workspace_id = ws_id;
            new_tab.is_ephemeral = is_ephemeral;
            static_cast<void>(session_.OpenTab(std::move(new_tab), true));
            break;
        }
    }
}

void TabStrip::RebuildTabs() {
    CEF_REQUIRE_UI_THREAD();

    panel_->RemoveAllChildViews();
    delegates_.clear();

    const auto& tabs = session_.Tabs();
    const auto& active_id = session_.ActiveTabId();

    for (const auto& tab : tabs) {
        const bool is_active = (active_id.has_value() && *active_id == tab.id);
        const bool is_discarded = (tab.lifecycle == core::TabLifecycle::Discarded);
        const std::string title_text = tab.title.empty() ? tab.url : tab.title;

        std::string prefix;
        if (is_discarded) {
            prefix += "💤 ";
        }
        if (tab.workspace_id.has_value() && !tab.workspace_id->empty() && *tab.workspace_id != "default") {
            prefix += "[" + *tab.workspace_id + "] ";
        }

        std::string label;
        if (is_active) {
            label = "[ " + prefix + title_text + " ]";
        } else {
            label = prefix + title_text;
        }

        if (label.size() > 28) {
            label = label.substr(0, 25) + "...";
            if (is_active) {
                label += " ]";
            }
        }

        auto activate_delegate = CefRefPtr<CefButtonDelegate>(
            new TabActionDelegate(*this, TabAction::Activate, tab.id));
        delegates_.push_back(activate_delegate);
        auto tab_btn = CefLabelButton::CreateLabelButton(activate_delegate, label);
        panel_->AddChildView(tab_btn);
        layout_->SetFlexForView(tab_btn, 0);

        auto close_delegate = CefRefPtr<CefButtonDelegate>(
            new TabActionDelegate(*this, TabAction::Close, tab.id));
        delegates_.push_back(close_delegate);
        auto close_btn = CefLabelButton::CreateLabelButton(close_delegate, "x");
        panel_->AddChildView(close_btn);
        layout_->SetFlexForView(close_btn, 0);
    }

    auto new_tab_delegate = CefRefPtr<CefButtonDelegate>(
        new TabActionDelegate(*this, TabAction::NewTab, ""));
    delegates_.push_back(new_tab_delegate);
    auto new_tab_btn = CefLabelButton::CreateLabelButton(new_tab_delegate, "+");
    panel_->AddChildView(new_tab_btn);
    layout_->SetFlexForView(new_tab_btn, 0);

    if (workspace_manager_ != nullptr) {
        auto ws_delegate = CefRefPtr<CefButtonDelegate>(
            new TabActionDelegate(*this, TabAction::CycleWorkspace, ""));
        delegates_.push_back(ws_delegate);
        const std::string ws_label = "📁 " + workspace_manager_->ActiveWorkspaceId();
        auto ws_btn = CefLabelButton::CreateLabelButton(ws_delegate, ws_label);
        panel_->AddChildView(ws_btn);
        layout_->SetFlexForView(ws_btn, 0);
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) {
        window->Layout();
    }
}

}  // namespace openbrowser::desktop
