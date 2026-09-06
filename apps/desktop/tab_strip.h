#pragma once

#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_panel.h"

#include <cstddef>
#include <string>
#include <vector>

namespace openbrowser::core {
class BrowserSession;
}

class CefBoxLayout;
class CefButtonDelegate;

namespace openbrowser::desktop {

class TabStrip final : public core::BrowserSessionObserver {
public:
    explicit TabStrip(core::BrowserSession& session);
    ~TabStrip() override;

    TabStrip(const TabStrip&) = delete;
    TabStrip& operator=(const TabStrip&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    enum class TabAction {
        Activate,
        Close,
        NewTab,
    };

    class TabActionDelegate;

    void HandleTabAction(TabAction action, const std::string& tab_id);
    void RebuildTabs();

    core::BrowserSession& session_;
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::size_t next_tab_index_{1};
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
