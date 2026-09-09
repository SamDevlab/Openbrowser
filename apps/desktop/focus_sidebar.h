#pragma once

#include "core/focus_queue/focus_queue.h"
#include "core/focus_queue/focus_sprint.h"
#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_panel.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace openbrowser::core {
class BrowserSession;
}

class CefBoxLayout;
class CefButtonDelegate;
class CefPanelDelegate;

namespace openbrowser::desktop {

class FocusSidebar final : public core::BrowserSessionObserver {
public:
    using FocusModeCallback = std::function<void(bool)>;
    using VisibilityChangedCallback = std::function<void(bool)>;

    FocusSidebar(
        core::BrowserSession& session,
        core::FocusQueue& focus_queue,
        FocusModeCallback on_focus_mode_changed = nullptr);
    ~FocusSidebar() override;

    FocusSidebar(const FocusSidebar&) = delete;
    FocusSidebar& operator=(const FocusSidebar&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    void SetVisibilityChangedCallback(VisibilityChangedCallback callback);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();
    [[nodiscard]] bool IsCompactIndicator() const noexcept { return compact_indicator_; }
    [[nodiscard]] bool IsFocusModeActive() const noexcept {
        return sprint_.State() == core::SprintState::Running;
    }

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

    void OnTimerTick();
    [[nodiscard]] const core::FocusSprint& Sprint() const noexcept { return sprint_; }

private:
    enum class FocusAction {
        EnqueueActiveTab,
        ActivateItem,
        PromoteItem,
        MarkNext,
        MarkLater,
        RemoveItem,
        StartSprint,
        PauseSprint,
        ResetSprint,
        ExpandDrawer,
    };

    class FocusActionDelegate;
    class FocusPanelDelegate;
    class SprintTickTask;

    void HandleFocusAction(FocusAction action, const std::string& item_id);
    void ScheduleTimerTick();
    void SetCompactIndicator(bool compact);
    void NotifyFocusModeChanged(bool enabled);
    void RebuildQueueView();
    void RelayoutWindow();

    core::BrowserSession& session_;
    core::FocusQueue& focus_queue_;
    core::FocusSprint sprint_;
    FocusModeCallback on_focus_mode_changed_;
    std::shared_ptr<bool> alive_token_{std::make_shared<bool>(true)};
    bool timer_running_{false};
    bool compact_indicator_{false};
    bool focus_mode_notified_{false};
    VisibilityChangedCallback on_visibility_changed_;

    CefRefPtr<CefPanelDelegate> panel_delegate_;
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
