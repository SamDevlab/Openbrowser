#pragma once

#include "core/network/filter_decision_log.h"
#include "core/session/browser_session_observer.h"
#include "devtools/network/connection_diagnostics.h"
#include "devtools/network/network_trace.h"
#include "devtools/network/obtrace_recorder.h"

#include "include/cef_base.h"
#include "include/views/cef_panel.h"

#include <functional>
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
class CefTextfield;
class CefTextfieldDelegate;

namespace openbrowser::desktop {

class NetworkLabPanel final : public devtools::network::NetworkTraceObserver,
                              public core::BrowserSessionObserver {
public:
    using VisibilityChangedCallback = std::function<void(bool)>;
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
    void SetVisibilityChangedCallback(VisibilityChangedCallback callback);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();

    void OnTraceEventAppended(const devtools::network::NetworkEvent& event) override;
    void OnTraceCleared() override;
    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    enum class ActiveTab {
        Requests,
        Connections,
        Decisions,
    };

    enum class PanelAction {
        ToggleScope,
        CycleMethod,
        CycleStatus,
        SelectRequest,
        DeselectRequest,
        Clear,
        Close,
        SwitchToRequests,
        SwitchToConnections,
        SwitchToDecisions,
        ExportObtrace,
    };

    class PanelActionDelegate;
    class PanelDelegate;
    class QueryFieldDelegate;

    void HandleAction(PanelAction action, const std::string& request_id = {});
    void SetPendingStructuredQuery(std::string query);
    void ApplyStructuredQuery();
    void RebuildView();
    void BuildTableView(const std::vector<devtools::network::NetworkRequestSummary>& requests);
    void BuildDetailsView(const devtools::network::NetworkRequestSummary& request);
    void BuildConnectionsView();
    void BuildDecisionsView();
    void BuildTabBar();

    devtools::network::NetworkTraceBuffer& trace_buffer_;
    core::BrowserSession& session_;
    bool filter_active_tab_{false};
    std::string method_filter_{"ALL"};
    std::string status_filter_{"ALL"};
    std::string structured_query_{};
    std::string pending_structured_query_{};
    std::optional<std::string> selected_request_id_{std::nullopt};
    ActiveTab active_tab_{ActiveTab::Requests};
    std::shared_ptr<bool> alive_token_{std::make_shared<bool>(true)};

    devtools::network::ConnectionRegistry* connection_registry_{nullptr};
    core::FilterDecisionLog* decision_log_{nullptr};
    devtools::network::ObtraceRecorder* obtrace_recorder_{nullptr};

    CefRefPtr<CefPanelDelegate> panel_delegate_;
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    CefRefPtr<CefTextfieldDelegate> query_delegate_;
    CefRefPtr<CefTextfield> query_field_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
    VisibilityChangedCallback on_visibility_changed_;
};

}  // namespace openbrowser::desktop
