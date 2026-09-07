#include "focus_sidebar.h"

#include "core/session/browser_session.h"
#include "core/session/focus_session_controller.h"

#include "include/cef_task.h"
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
    explicit FocusPanelDelegate(const FocusSidebar& sidebar) : sidebar_(sidebar) {}

    CefSize GetPreferredSize(CefRefPtr<CefView> /*view*/) override {
        return sidebar_.expanded_ ? CefSize(240, 600) : CefSize(76, 600);
    }

    CefSize GetMinimumSize(CefRefPtr<CefView> /*view*/) override {
        return sidebar_.expanded_ ? CefSize(180, 200) : CefSize(64, 80);
    }

private:
    const FocusSidebar& sidebar_;

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

class FocusSidebar::SprintTickTask final : public CefTask {
public:
    SprintTickTask(FocusSidebar* sidebar, std::weak_ptr<bool> alive_token)
        : sidebar_(sidebar), alive_token_(std::move(alive_token)) {}

    void Execute() override {
        auto alive = alive_token_.lock();
        if (alive && *alive && sidebar_ != nullptr) {
            sidebar_->OnTimerTick();
        }
    }

private:
    FocusSidebar* sidebar_;
    std::weak_ptr<bool> alive_token_;

    IMPLEMENT_REFCOUNTING(SprintTickTask);
};

FocusSidebar::FocusSidebar(core::BrowserSession& session, core::FocusQueue& focus_queue)
    : session_(session), focus_queue_(focus_queue) {
    session_.AddObserver(this);

    panel_delegate_ = new FocusPanelDelegate(*this);
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
    *alive_token_ = false;
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

void FocusSidebar::OnTimerTick() {
    CEF_REQUIRE_UI_THREAD();
    if (!timer_running_ || sprint_.State() != core::SprintState::Running) {
        return;
    }

    const bool completed = core::FocusSessionController::RecordSprintTick(
        sprint_, session_, focus_queue_, 1);
    if (completed) {
        timer_running_ = false;
    }

    RebuildQueueView();

    if (timer_running_ && sprint_.State() == core::SprintState::Running) {
        ScheduleTimerTick();
    }
}

void FocusSidebar::ScheduleTimerTick() {
    CEF_REQUIRE_UI_THREAD();
    if (!timer_running_) {
        return;
    }
    CefPostDelayedTask(TID_UI, new SprintTickTask(this, alive_token_), 1000);
}

void FocusSidebar::HandleFocusAction(const FocusAction action, const std::string& item_id) {
    CEF_REQUIRE_UI_THREAD();

    switch (action) {
        case FocusAction::ToggleExpanded:
            expanded_ = !expanded_;
            RebuildQueueView();
            break;
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
        case FocusAction::MarkNext:
            if (!item_id.empty()) {
                static_cast<void>(focus_queue_.SetState(item_id, core::FocusState::Next));
                RebuildQueueView();
            }
            break;
        case FocusAction::MarkLater:
            if (!item_id.empty()) {
                static_cast<void>(focus_queue_.SetState(item_id, core::FocusState::Later));
                RebuildQueueView();
            }
            break;
        case FocusAction::RemoveItem:
            if (!item_id.empty()) {
                static_cast<void>(focus_queue_.Remove(item_id));
                RebuildQueueView();
            }
            break;
        case FocusAction::StartSprint:
            if (sprint_.State() == core::SprintState::Paused) {
                sprint_.Resume();
            } else {
                sprint_.Start();
            }
            timer_running_ = true;
            ScheduleTimerTick();
            RebuildQueueView();
            break;
        case FocusAction::PauseSprint:
            sprint_.Pause();
            timer_running_ = false;
            RebuildQueueView();
            break;
        case FocusAction::ResetSprint:
            sprint_.Reset();
            timer_running_ = false;
            RebuildQueueView();
            break;
    }
}

void FocusSidebar::RebuildQueueView() {
    CEF_REQUIRE_UI_THREAD();

    panel_->RemoveAllChildViews();
    delegates_.clear();

    auto toggle_delegate = CefRefPtr<CefButtonDelegate>(
        new FocusActionDelegate(*this, FocusAction::ToggleExpanded, ""));
    delegates_.push_back(toggle_delegate);
    auto toggle_btn = CefLabelButton::CreateLabelButton(
        toggle_delegate, expanded_ ? "Focus <" : "Focus >");
    panel_->AddChildView(toggle_btn);
    layout_->SetFlexForView(toggle_btn, 0);

    if (!expanded_) {
        panel_->InvalidateLayout();
        panel_->Layout();
        auto window = panel_->GetWindow();
        if (window) {
            window->Layout();
        }
        return;
    }

    auto sprint_panel = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings sprint_settings{};
    sprint_settings.horizontal = 0;
    sprint_settings.between_child_spacing = 2;
    sprint_settings.inside_border_horizontal_spacing = 4;
    sprint_settings.inside_border_vertical_spacing = 4;
    sprint_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    auto sprint_layout = sprint_panel->SetToBoxLayout(sprint_settings);

    std::string state_str = "Idle";
    if (sprint_.State() == core::SprintState::Running) {
        state_str = "Running";
    } else if (sprint_.State() == core::SprintState::Paused) {
        state_str = "Paused";
    } else if (sprint_.State() == core::SprintState::Completed) {
        state_str = "Completed";
    }

    std::string timer_label = "Timer: " + sprint_.FormattedTime() + " (" + state_str + ")";
    auto timer_btn = CefLabelButton::CreateLabelButton(nullptr, timer_label);
    sprint_panel->AddChildView(timer_btn);
    sprint_layout->SetFlexForView(timer_btn, 0);

    auto controls_panel = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings controls_settings{};
    controls_settings.horizontal = 1;
    controls_settings.between_child_spacing = 4;
    controls_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    auto controls_layout = controls_panel->SetToBoxLayout(controls_settings);

    if (sprint_.State() == core::SprintState::Running) {
        auto pause_delegate = CefRefPtr<CefButtonDelegate>(
            new FocusActionDelegate(*this, FocusAction::PauseSprint, ""));
        delegates_.push_back(pause_delegate);
        auto pause_btn = CefLabelButton::CreateLabelButton(pause_delegate, "Pause");
        controls_panel->AddChildView(pause_btn);
        controls_layout->SetFlexForView(pause_btn, 1);
    } else if (sprint_.State() == core::SprintState::Paused) {
        auto resume_delegate = CefRefPtr<CefButtonDelegate>(
            new FocusActionDelegate(*this, FocusAction::StartSprint, ""));
        delegates_.push_back(resume_delegate);
        auto resume_btn = CefLabelButton::CreateLabelButton(resume_delegate, "Resume");
        controls_panel->AddChildView(resume_btn);
        controls_layout->SetFlexForView(resume_btn, 1);
    } else {
        auto start_delegate = CefRefPtr<CefButtonDelegate>(
            new FocusActionDelegate(*this, FocusAction::StartSprint, ""));
        delegates_.push_back(start_delegate);
        auto start_btn = CefLabelButton::CreateLabelButton(start_delegate, "Start");
        controls_panel->AddChildView(start_btn);
        controls_layout->SetFlexForView(start_btn, 1);
    }

    auto reset_delegate = CefRefPtr<CefButtonDelegate>(
        new FocusActionDelegate(*this, FocusAction::ResetSprint, ""));
    delegates_.push_back(reset_delegate);
    auto reset_btn = CefLabelButton::CreateLabelButton(reset_delegate, "Reset");
    controls_panel->AddChildView(reset_btn);
    controls_layout->SetFlexForView(reset_btn, 1);

    sprint_panel->AddChildView(controls_panel);
    sprint_layout->SetFlexForView(controls_panel, 0);

    auto metrics_btn = CefLabelButton::CreateLabelButton(nullptr, sprint_.FormattedMetrics());
    sprint_panel->AddChildView(metrics_btn);
    sprint_layout->SetFlexForView(metrics_btn, 0);

    panel_->AddChildView(sprint_panel);
    layout_->SetFlexForView(sprint_panel, 0);

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
        if (item.workspace_id.has_value() && !item.workspace_id->empty() && *item.workspace_id != "default") {
            item_label = "[" + *item.workspace_id + "] " + item_label;
        }
        if (item_label.size() > 28) {
            item_label = item_label.substr(0, 25) + "...";
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
            auto promote_btn = CefLabelButton::CreateLabelButton(promote_delegate, "Now");
            item_panel->AddChildView(promote_btn);
            item_layout->SetFlexForView(promote_btn, 0);
        }

        if (item.state != core::FocusState::Next) {
            auto next_delegate = CefRefPtr<CefButtonDelegate>(
                new FocusActionDelegate(*this, FocusAction::MarkNext, item.id));
            delegates_.push_back(next_delegate);
            auto next_btn = CefLabelButton::CreateLabelButton(next_delegate, "Next");
            item_panel->AddChildView(next_btn);
            item_layout->SetFlexForView(next_btn, 0);
        }

        if (item.state != core::FocusState::Later) {
            auto later_delegate = CefRefPtr<CefButtonDelegate>(
                new FocusActionDelegate(*this, FocusAction::MarkLater, item.id));
            delegates_.push_back(later_delegate);
            auto later_btn = CefLabelButton::CreateLabelButton(later_delegate, "Later");
            item_panel->AddChildView(later_btn);
            item_layout->SetFlexForView(later_btn, 0);
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

    panel_->InvalidateLayout();
    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) {
        window->Layout();
    }
}

}  // namespace openbrowser::desktop
