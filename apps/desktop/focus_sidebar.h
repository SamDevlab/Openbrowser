#pragma once

#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_panel.h"

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

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    enum class FocusAction {
        EnqueueActiveTab,
        ActivateItem,
        PromoteItem,
        RemoveItem,
    };

    class FocusActionDelegate;
    class FocusPanelDelegate;

    void HandleFocusAction(FocusAction action, const std::string& item_id);
    void RebuildQueueView();

    core::BrowserSession& session_;
    core::FocusQueue& focus_queue_;
    CefRefPtr<CefPanelDelegate> panel_delegate_;
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
