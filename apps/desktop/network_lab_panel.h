#pragma once

#include "core/session/browser_session_observer.h"
#include "devtools/network/network_trace.h"

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

class NetworkLabPanel final : public devtools::network::NetworkTraceObserver,
                              public core::BrowserSessionObserver {
public:
    NetworkLabPanel(
        devtools::network::NetworkTraceBuffer& trace_buffer,
        core::BrowserSession& session);
    ~NetworkLabPanel() override;

    NetworkLabPanel(const NetworkLabPanel&) = delete;
    NetworkLabPanel& operator=(const NetworkLabPanel&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();

    // NetworkTraceObserver
    void OnTraceEventAppended(const devtools::network::NetworkEvent& event) override;
    void OnTraceCleared() override;

    // BrowserSessionObserver
    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    enum class PanelAction {
        ToggleScope,
        Clear,
        Close,
    };

    class PanelActionDelegate;
    class PanelDelegate;

    void HandleAction(PanelAction action);
    void RebuildView();

    devtools::network::NetworkTraceBuffer& trace_buffer_;
    core::BrowserSession& session_;
    bool filter_active_tab_{false};

    CefRefPtr<CefPanelDelegate> panel_delegate_;
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
