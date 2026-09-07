#pragma once

#include "core/network/filter_decision_log.h"
#include "core/session/browser_session_observer.h"
#include "devtools/network/connection_diagnostics.h"
#include "devtools/network/network_trace.h"
#include "devtools/network/obtrace_recorder.h"

#include "include/cef_base.h"
#include "include/views/cef_panel.h"

#include <memory>
#include <optional>
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
    // M7.4: enhanced constructor accepts M7 subsystems (non-owning refs/ptrs).
    NetworkLabPanel(
        devtools::network::NetworkTraceBuffer& trace_buffer,
        core::BrowserSession& session,
        devtools::network::ConnectionRegistry* connection_registry = nullptr,
        core::FilterDecisionLog* decision_log = nullptr,
        devtools::network::ObtraceRecorder* obtrace_recorder = nullptr);
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
    // M7.4: active sub-view tab.
    enum class ActiveTab {
        Requests,
        Connections, // M7.1
        Decisions,   // M7.2
    };

    enum class PanelAction {
        ToggleScope,
        CycleMethod,
        CycleStatus,
        SelectRequest,
        DeselectRequest,
        Clear,
        Close,
        // M7.4
        SwitchToRequests,
        SwitchToConnections,
        SwitchToDecisions,
        ExportObtrace,
    };

    class PanelActionDelegate;
    class PanelDelegate;

    void HandleAction(PanelAction action, const std::string& request_id = {});
    void RebuildView();
    void BuildTableView(const std::vector<devtools::network::NetworkRequestSummary>& requests);
    void BuildDetailsView(const devtools::network::NetworkRequestSummary& request);
    void BuildConnectionsView();  // M7.1
    void BuildDecisionsView();    // M7.2
    void BuildTabBar();           // M7.4: shared tab strip

    devtools::network::NetworkTraceBuffer& trace_buffer_;
    core::BrowserSession& session_;
    bool filter_active_tab_{false};
    std::string method_filter_{"ALL"};
    std::string status_filter_{"ALL"};
    std::string search_query_{};
    std::optional<std::string> selected_request_id_{std::nullopt};
    ActiveTab active_tab_{ActiveTab::Requests}; // M7.4

    // M7 subsystems (non-owning)
    devtools::network::ConnectionRegistry* connection_registry_{nullptr};
    core::FilterDecisionLog* decision_log_{nullptr};
    devtools::network::ObtraceRecorder* obtrace_recorder_{nullptr};

    CefRefPtr<CefPanelDelegate> panel_delegate_;
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
