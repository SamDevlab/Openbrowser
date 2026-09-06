#include "focus_sidebar.h"

#include "core/session/browser_session.h"
#include "core/session/focus_session_controller.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include <utility>

namespace openbrowser::desktop {

class FocusSidebar::FocusPanelDelegate final : public CefPanelDelegate {
public:
    CefSize GetPreferredSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(200, 600);
    }

    CefSize GetMinimumSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(160, 200);
    }

    IMPLEMENT_REFCOUNTING(FocusPanelDelegate);
};

class FocusSidebar::FocusActionDelegate final : public CefButtonDelegate {
public:
    FocusActionDelegate(FocusSidebar& sidebar, const FocusAction action, std::string item_id)
        : sidebar_(sidebar), action_(action), item_id_(std::move(item_id)) {}

    FocusActionDelegate(const FocusActionDelegate&) = delete;
    FocusActionDelegate& operator=(const FocusActionDelegate&) = delete;

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        sidebar_.HandleFocusAction(action_, item_id_);
    }

private:
    FocusSidebar& sidebar_;
    FocusAction action_;
    std::string item_id_;

    IMPLEMENT_REFCOUNTING(FocusActionDelegate);
};

FocusSidebar::FocusSidebar(core::BrowserSession& session, core::FocusQueue& focus_queue)
    : session_(session), focus_queue_(focus_queue) {
    session_.AddObserver(this);

    panel_delegate_ = new FocusPanelDelegate();
    panel_ = CefPanel::CreatePanel(panel_delegate_);

    CefBoxLayoutSettings settings{};
    settings.horizontal = 0;
    settings.between_child_spacing = 4;
    settings.inside_border_horizontal_spacing = 6;
    settings.inside_border_vertical_spacing = 6;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    layout_ = panel_->SetToBoxLayout(settings);

    RebuildQueueView();
}

FocusSidebar::~FocusSidebar() {
    session_.RemoveObserver(this);
}

CefRefPtr<CefPanel> FocusSidebar::View() const noexcept {
    return panel_;
}

void FocusSidebar::OnBrowserSessionChanged(const core::BrowserSession& /*session*/) {
    CEF_REQUIRE_UI_THREAD();
    core::FocusSessionController::SynchronizeTabClosures(session_, focus_queue_);
    RebuildQueueView();
}

void FocusSidebar::HandleFocusAction(const FocusAction action, const std::string& item_id) {
    CEF_REQUIRE_UI_THREAD();

    switch (action) {
        case FocusAction::EnqueueActiveTab:
            static_cast<void>(core::FocusSessionController::EnqueueActiveTab(session_, focus_queue_));
            RebuildQueueView();
            break;
        case FocusAction::ActivateItem:
            if (!item_id.empty()) {
                static_cast<void>(core::FocusSessionController::ActivateFocusItem(session_, focus_queue_, item_id));
                RebuildQueueView();
            }
            break;
        case FocusAction::PromoteItem:
            if (!item_id.empty()) {
                static_cast<void>(focus_queue_.PromoteToNow(item_id));
                RebuildQueueView();
            }
            break;
        case FocusAction::RemoveItem:
            if (!item_id.empty()) {
                static_cast<void>(focus_queue_.Remove(item_id));
                RebuildQueueView();
            }
            break;
    }
}

void FocusSidebar::RebuildQueueView() {
    CEF_REQUIRE_UI_THREAD();

    panel_->RemoveAllChildViews();
    delegates_.clear();

    auto enqueue_delegate = CefRefPtr<CefButtonDelegate>(
        new FocusActionDelegate(*this, FocusAction::EnqueueActiveTab, ""));
    delegates_.push_back(enqueue_delegate);
    auto enqueue_btn = CefLabelButton::CreateLabelButton(enqueue_delegate, "+ Focus Active Tab");
    panel_->AddChildView(enqueue_btn);
    layout_->SetFlexForView(enqueue_btn, 0);

    for (const auto& item : focus_queue_.Items()) {
        std::string state_prefix;
        switch (item.state) {
            case core::FocusState::Now:
                state_prefix = "[Now] ";
                break;
            case core::FocusState::Next:
                state_prefix = "[Next] ";
                break;
            case core::FocusState::Later:
                state_prefix = "[Later] ";
                break;
            case core::FocusState::Paused:
                state_prefix = "[Paused] ";
                break;
        }

        auto item_panel = CefPanel::CreatePanel(nullptr);
        CefBoxLayoutSettings item_settings{};
        item_settings.horizontal = 1;
        item_settings.between_child_spacing = 2;
        item_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
        auto item_layout = item_panel->SetToBoxLayout(item_settings);

        std::string item_label = state_prefix + item.url;
        if (item_label.size() > 18) {
            item_label = item_label.substr(0, 15) + "...";
        }
        auto activate_delegate = CefRefPtr<CefButtonDelegate>(
            new FocusActionDelegate(*this, FocusAction::ActivateItem, item.id));
        delegates_.push_back(activate_delegate);
        auto activate_btn = CefLabelButton::CreateLabelButton(activate_delegate, item_label);
        item_panel->AddChildView(activate_btn);
        item_layout->SetFlexForView(activate_btn, 1);

        if (item.state != core::FocusState::Now) {
            auto promote_delegate = CefRefPtr<CefButtonDelegate>(
                new FocusActionDelegate(*this, FocusAction::PromoteItem, item.id));
            delegates_.push_back(promote_delegate);
            auto promote_btn = CefLabelButton::CreateLabelButton(promote_delegate, "^");
            item_panel->AddChildView(promote_btn);
            item_layout->SetFlexForView(promote_btn, 0);
        }

        auto remove_delegate = CefRefPtr<CefButtonDelegate>(
            new FocusActionDelegate(*this, FocusAction::RemoveItem, item.id));
        delegates_.push_back(remove_delegate);
        auto remove_btn = CefLabelButton::CreateLabelButton(remove_delegate, "x");
        item_panel->AddChildView(remove_btn);
        item_layout->SetFlexForView(remove_btn, 0);

        panel_->AddChildView(item_panel);
        layout_->SetFlexForView(item_panel, 0);
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) {
        window->Layout();
    }
}

}  // namespace openbrowser::desktop
