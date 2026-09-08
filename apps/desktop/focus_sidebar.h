#pragma once

#include "core/focus_queue/focus_queue.h"
#include "core/focus_queue/focus_sprint.h"
#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_panel.h"

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
    FocusSidebar(core::BrowserSession& session, core::FocusQueue& focus_queue);
    ~FocusSidebar() override;

    FocusSidebar(const FocusSidebar&) = delete;
    FocusSidebar& operator=(const FocusSidebar&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();

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
    };

    class FocusActionDelegate;
    class FocusPanelDelegate;
    class SprintTickTask;

    void HandleFocusAction(FocusAction action, const std::string& item_id);
    void ScheduleTimerTick();
    void RebuildQueueView();

    core::BrowserSession& session_;
    core::FocusQueue& focus_queue_;
    core::FocusSprint sprint_;
    std::shared_ptr<bool> alive_token_{std::make_shared<bool>(true)};
    bool timer_running_{false};

    CefRefPtr<CefPanelDelegate> panel_delegate_;
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
