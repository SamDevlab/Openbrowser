#include "network_lab_panel.h"

#include "core/session/browser_session.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include <algorithm>
#include <utility>

namespace openbrowser::desktop {

class NetworkLabPanel::PanelDelegate final : public CefPanelDelegate {
public:
    CefSize GetPreferredSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(1280, 260);
    }

    CefSize GetMinimumSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(400, 160);
    }

    IMPLEMENT_REFCOUNTING(PanelDelegate);
};

class NetworkLabPanel::PanelActionDelegate final : public CefButtonDelegate {
public:
    PanelActionDelegate(
        NetworkLabPanel& panel,
        const PanelAction action,
        std::string request_id = {})
        : panel_(panel), action_(action), request_id_(std::move(request_id)) {}

    PanelActionDelegate(const PanelActionDelegate&) = delete;
    PanelActionDelegate& operator=(const PanelActionDelegate&) = delete;

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        panel_.HandleAction(action_, request_id_);
    }

private:
    NetworkLabPanel& panel_;
    PanelAction action_;
    std::string request_id_;

    IMPLEMENT_REFCOUNTING(PanelActionDelegate);
};

NetworkLabPanel::NetworkLabPanel(
    devtools::network::NetworkTraceBuffer& trace_buffer,
    core::BrowserSession& session,
    devtools::network::ConnectionRegistry* connection_registry,
    core::FilterDecisionLog* decision_log,
    devtools::network::ObtraceRecorder* obtrace_recorder)
    : trace_buffer_(trace_buffer)
    , session_(session)
    , connection_registry_(connection_registry)
    , decision_log_(decision_log)
    , obtrace_recorder_(obtrace_recorder) {
    trace_buffer_.AddObserver(this);
    session_.AddObserver(this);

    panel_delegate_ = new PanelDelegate();
    panel_ = CefPanel::CreatePanel(panel_delegate_);

    CefBoxLayoutSettings settings{};
    settings.horizontal = 0;
    settings.between_child_spacing = 2;
    settings.inside_border_horizontal_spacing = 6;
    settings.inside_border_vertical_spacing = 4;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    layout_ = panel_->SetToBoxLayout(settings);

    RebuildView();
}

NetworkLabPanel::~NetworkLabPanel() {
    trace_buffer_.RemoveObserver(this);
    session_.RemoveObserver(this);
}

CefRefPtr<CefPanel> NetworkLabPanel::View() const noexcept {
    return panel_;
}

void NetworkLabPanel::SetVisible(const bool visible) {
    CEF_REQUIRE_UI_THREAD();
    if (!panel_) {
        return;
    }
    panel_->SetVisible(visible);
    auto window = panel_->GetWindow();
    if (window) {
        window->Layout();
    }
}

bool NetworkLabPanel::IsVisible() const {
    CEF_REQUIRE_UI_THREAD();
    if (!panel_) {
        return false;
    }
    return panel_->IsVisible();
}

void NetworkLabPanel::ToggleVisibility() {
    CEF_REQUIRE_UI_THREAD();
    SetVisible(!IsVisible());
}

void NetworkLabPanel::OnTraceEventAppended(const devtools::network::NetworkEvent& /*event*/) {
    CEF_REQUIRE_UI_THREAD();
    if (IsVisible()) {
        RebuildView();
    }
}

void NetworkLabPanel::OnTraceCleared() {
    CEF_REQUIRE_UI_THREAD();
    if (IsVisible()) {
        selected_request_id_ = std::nullopt;
        RebuildView();
    }
}

void NetworkLabPanel::OnBrowserSessionChanged(const core::BrowserSession& /*session*/) {
    CEF_REQUIRE_UI_THREAD();
    if (filter_active_tab_ && IsVisible()) {
        RebuildView();
    }
}

void NetworkLabPanel::HandleAction(const PanelAction action, const std::string& request_id) {
    CEF_REQUIRE_UI_THREAD();
    switch (action) {
        case PanelAction::ToggleScope:
            filter_active_tab_ = !filter_active_tab_;
            RebuildView();
            break;
        case PanelAction::CycleMethod:
            if (method_filter_ == "ALL") {
                method_filter_ = "GET";
            } else if (method_filter_ == "GET") {
                method_filter_ = "POST";
            } else {
                method_filter_ = "ALL";
            }
            RebuildView();
            break;
        case PanelAction::CycleStatus:
            if (status_filter_ == "ALL") {
                status_filter_ = "2XX";
            } else if (status_filter_ == "2XX") {
                status_filter_ = "ERR";
            } else {
                status_filter_ = "ALL";
            }
            RebuildView();
            break;
        case PanelAction::SelectRequest:
            if (!request_id.empty()) {
                selected_request_id_ = request_id;
                RebuildView();
            }
            break;
        case PanelAction::DeselectRequest:
            selected_request_id_ = std::nullopt;
            RebuildView();
            break;
        case PanelAction::Clear:
            trace_buffer_.Clear();
            selected_request_id_ = std::nullopt;
            RebuildView();
            break;
        case PanelAction::Close:
            SetVisible(false);
            break;
        // M7.4: tab switching
        case PanelAction::SwitchToRequests:
            active_tab_ = ActiveTab::Requests;
            selected_request_id_ = std::nullopt;
            RebuildView();
            break;
        case PanelAction::SwitchToConnections:
            active_tab_ = ActiveTab::Connections;
            RebuildView();
            break;
        case PanelAction::SwitchToDecisions:
            active_tab_ = ActiveTab::Decisions;
            RebuildView();
            break;
        // M7.4: export .obtrace snapshot
        case PanelAction::ExportObtrace:
            // Trigger is handled by DesktopApp via ActionRegistry;
            // here we just ensure the recorder knows about it.
            break;
    }
}

void NetworkLabPanel::RebuildView() {
    CEF_REQUIRE_UI_THREAD();
    panel_->RemoveAllChildViews();
    delegates_.clear();

    // Always render the shared tab bar first.
    BuildTabBar();

    switch (active_tab_) {
        case ActiveTab::Connections:
            BuildConnectionsView();
            return;
        case ActiveTab::Decisions:
            BuildDecisionsView();
            return;
        case ActiveTab::Requests:
        default:
            break;
    }

    if (selected_request_id_.has_value()) {
        const auto req = trace_buffer_.FindRequest(*selected_request_id_);
        if (req.has_value()) {
            BuildDetailsView(*req);
            return;
        }
        selected_request_id_ = std::nullopt;
    }

    devtools::network::NetworkTraceFilter filter{
        .tab_id = filter_active_tab_ ? session_.ActiveTabId() : std::nullopt,
        .search_query = search_query_,
        .method_filter = method_filter_,
        .status_filter = status_filter_,
    };
    const auto requests = trace_buffer_.QueryRequests(filter);
    BuildTableView(requests);
}

void NetworkLabPanel::BuildTableView(
    const std::vector<devtools::network::NetworkRequestSummary>& requests) {
    auto toolbar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings tool_settings{};
    tool_settings.horizontal = 1;
    tool_settings.between_child_spacing = 6;
    tool_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    auto tool_layout = toolbar->SetToBoxLayout(tool_settings);

    std::string title_text = "Network Lab (" + std::to_string(requests.size()) + " reqs)";
    auto title_btn = CefLabelButton::CreateLabelButton(nullptr, title_text);
    toolbar->AddChildView(title_btn);
    tool_layout->SetFlexForView(title_btn, 0);

    auto scope_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::ToggleScope));
    delegates_.push_back(scope_delegate);
    std::string scope_label = filter_active_tab_ ? "[Scope: Tab]" : "[Scope: All]";
    auto scope_btn = CefLabelButton::CreateLabelButton(scope_delegate, scope_label);
    toolbar->AddChildView(scope_btn);
    tool_layout->SetFlexForView(scope_btn, 0);

    auto method_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::CycleMethod));
    delegates_.push_back(method_delegate);
    std::string method_label = "[Method: " + method_filter_ + "]";
    auto method_btn = CefLabelButton::CreateLabelButton(method_delegate, method_label);
    toolbar->AddChildView(method_btn);
    tool_layout->SetFlexForView(method_btn, 0);

    auto status_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::CycleStatus));
    delegates_.push_back(status_delegate);
    std::string status_label = "[Status: " + status_filter_ + "]";
    auto status_btn = CefLabelButton::CreateLabelButton(status_delegate, status_label);
    toolbar->AddChildView(status_btn);
    tool_layout->SetFlexForView(status_btn, 0);

    auto clear_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::Clear));
    delegates_.push_back(clear_delegate);
    auto clear_btn = CefLabelButton::CreateLabelButton(clear_delegate, "Clear");
    toolbar->AddChildView(clear_btn);
    tool_layout->SetFlexForView(clear_btn, 0);

    auto close_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::Close));
    delegates_.push_back(close_delegate);
    auto close_btn = CefLabelButton::CreateLabelButton(close_delegate, "Close");
    toolbar->AddChildView(close_btn);
    tool_layout->SetFlexForView(close_btn, 0);

    panel_->AddChildView(toolbar);
    layout_->SetFlexForView(toolbar, 0);

    if (requests.empty()) {
        auto empty_btn = CefLabelButton::CreateLabelButton(nullptr, "No requests match active filters.");
        panel_->AddChildView(empty_btn);
        layout_->SetFlexForView(empty_btn, 0);
    } else {
        const std::size_t max_rows = 6;
        const std::size_t start_idx = requests.size() > max_rows ? (requests.size() - max_rows) : 0;

        for (std::size_t i = requests.size(); i > start_idx; --i) {
            const auto& req = requests[i - 1];

            auto row_panel = CefPanel::CreatePanel(nullptr);
            CefBoxLayoutSettings row_settings{};
            row_settings.horizontal = 1;
            row_settings.between_child_spacing = 4;
            row_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
            auto row_layout = row_panel->SetToBoxLayout(row_settings);

            std::string status_str = req.status.has_value()
                ? std::to_string(*req.status)
                : (req.state == devtools::network::RequestState::Failed ? "ERR" : "...");
            std::string method_label = "[" + req.method + "] " + status_str;

            auto select_delegate = CefRefPtr<CefButtonDelegate>(
                new PanelActionDelegate(*this, PanelAction::SelectRequest, req.request_id));
            delegates_.push_back(select_delegate);

            auto row_method_btn = CefLabelButton::CreateLabelButton(select_delegate, method_label);
            row_panel->AddChildView(row_method_btn);
            row_layout->SetFlexForView(row_method_btn, 0);

            std::string display_url = req.url;
            if (display_url.size() > 55) {
                display_url = display_url.substr(0, 52) + "...";
            }
            auto url_btn = CefLabelButton::CreateLabelButton(select_delegate, display_url);
            row_panel->AddChildView(url_btn);
            row_layout->SetFlexForView(url_btn, 1);

            std::string size_str = std::to_string(req.transferred_bytes) + " B";
            if (req.transferred_bytes >= 1024) {
                size_str = std::to_string(req.transferred_bytes / 1024) + " KB";
            }
            if (!req.body_preview.empty()) {
                size_str += " (body)";
            }
            auto size_btn = CefLabelButton::CreateLabelButton(select_delegate, size_str);
            row_panel->AddChildView(size_btn);
            row_layout->SetFlexForView(size_btn, 0);

            panel_->AddChildView(row_panel);
            layout_->SetFlexForView(row_panel, 0);
        }
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) {
        window->Layout();
    }
}

void NetworkLabPanel::BuildDetailsView(
    const devtools::network::NetworkRequestSummary& req) {
    auto header_panel = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings header_settings{};
    header_settings.horizontal = 1;
    header_settings.between_child_spacing = 6;
    header_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    auto header_layout = header_panel->SetToBoxLayout(header_settings);

    auto back_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::DeselectRequest));
    delegates_.push_back(back_delegate);
    auto back_btn = CefLabelButton::CreateLabelButton(back_delegate, "⬅ Back");
    header_panel->AddChildView(back_btn);
    header_layout->SetFlexForView(back_btn, 0);

    std::string status_str = req.status.has_value()
        ? std::to_string(*req.status)
        : (req.state == devtools::network::RequestState::Failed ? "ERR" : "Pending");
    std::string summary_title = "[" + req.method + " " + status_str + "] " + req.url;
    if (summary_title.size() > 70) {
        summary_title = summary_title.substr(0, 67) + "...";
    }
    auto title_btn = CefLabelButton::CreateLabelButton(nullptr, summary_title);
    header_panel->AddChildView(title_btn);
    header_layout->SetFlexForView(title_btn, 1);

    auto close_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::Close));
    delegates_.push_back(close_delegate);
    auto close_btn = CefLabelButton::CreateLabelButton(close_delegate, "Close");
    header_panel->AddChildView(close_btn);
    header_layout->SetFlexForView(close_btn, 0);

    panel_->AddChildView(header_panel);
    layout_->SetFlexForView(header_panel, 0);

    // Overview details row
    std::string size_str = std::to_string(req.transferred_bytes) + " B";
    if (req.transferred_bytes >= 1024) {
        size_str = std::to_string(req.transferred_bytes / 1024) + " KB";
    }
    std::string overview_str = "Proto: " + (req.protocol.empty() ? "http/1.1" : req.protocol) +
                               " | Transferred: " + size_str;
    if (!req.error.empty()) {
        overview_str += " | Error: " + req.error;
    }
    auto overview_btn = CefLabelButton::CreateLabelButton(nullptr, overview_str);
    panel_->AddChildView(overview_btn);
    layout_->SetFlexForView(overview_btn, 0);

    // Request Headers
    if (!req.request_headers.empty()) {
        auto req_hdr_title = CefLabelButton::CreateLabelButton(
            nullptr, "--- Request Headers (" + std::to_string(req.request_headers.size()) + ") ---");
        panel_->AddChildView(req_hdr_title);
        layout_->SetFlexForView(req_hdr_title, 0);

        const std::size_t max_show = std::min(req.request_headers.size(), static_cast<std::size_t>(6));
        for (std::size_t i = 0; i < max_show; ++i) {
            const auto& h = req.request_headers[i];
            std::string line = h.name + ": " + h.value;
            if (line.size() > 80) {
                line = line.substr(0, 77) + "...";
            }
            auto h_btn = CefLabelButton::CreateLabelButton(nullptr, "  " + line);
            panel_->AddChildView(h_btn);
            layout_->SetFlexForView(h_btn, 0);
        }
    }

    // Response Headers
    if (!req.response_headers.empty()) {
        auto res_hdr_title = CefLabelButton::CreateLabelButton(
            nullptr, "--- Response Headers (" + std::to_string(req.response_headers.size()) + ") ---");
        panel_->AddChildView(res_hdr_title);
        layout_->SetFlexForView(res_hdr_title, 0);

        const std::size_t max_show = std::min(req.response_headers.size(), static_cast<std::size_t>(6));
        for (std::size_t i = 0; i < max_show; ++i) {
            const auto& h = req.response_headers[i];
            std::string line = h.name + ": " + h.value;
            if (line.size() > 80) {
                line = line.substr(0, 77) + "...";
            }
            auto h_btn = CefLabelButton::CreateLabelButton(nullptr, "  " + line);
            panel_->AddChildView(h_btn);
            layout_->SetFlexForView(h_btn, 0);
        }
    }

    // Body Payload Preview
    if (!req.body_preview.empty()) {
        auto body_title = CefLabelButton::CreateLabelButton(nullptr, "--- Body Preview ---");
        panel_->AddChildView(body_title);
        layout_->SetFlexForView(body_title, 0);

        std::string body_text = req.body_preview;
        if (body_text.size() > 120) {
            body_text = body_text.substr(0, 117) + "...";
        }
        auto body_btn = CefLabelButton::CreateLabelButton(nullptr, "  " + body_text);
        panel_->AddChildView(body_btn);
        layout_->SetFlexForView(body_btn, 0);
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) {
        window->Layout();
    }
}

// M7.4: shared tab strip row rendered at the top of every sub-view.
void NetworkLabPanel::BuildTabBar() {
    auto tab_bar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings tab_settings{};
    tab_settings.horizontal = 1;
    tab_settings.between_child_spacing = 4;
    tab_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    auto tab_layout = tab_bar->SetToBoxLayout(tab_settings);

    auto req_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::SwitchToRequests));
    delegates_.push_back(req_delegate);
    std::string req_label = active_tab_ == ActiveTab::Requests ? "[▶ Requests]" : "Requests";
    auto req_btn = CefLabelButton::CreateLabelButton(req_delegate, req_label);
    tab_bar->AddChildView(req_btn);
    tab_layout->SetFlexForView(req_btn, 0);

    auto conn_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::SwitchToConnections));
    delegates_.push_back(conn_delegate);
    std::string conn_label = active_tab_ == ActiveTab::Connections ? "[▶ Connections]" : "Connections";
    auto conn_btn = CefLabelButton::CreateLabelButton(conn_delegate, conn_label);
    tab_bar->AddChildView(conn_btn);
    tab_layout->SetFlexForView(conn_btn, 0);

    auto dec_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::SwitchToDecisions));
    delegates_.push_back(dec_delegate);
    std::string dec_label = active_tab_ == ActiveTab::Decisions ? "[▶ Decisions]" : "Decisions";
    auto dec_btn = CefLabelButton::CreateLabelButton(dec_delegate, dec_label);
    tab_bar->AddChildView(dec_btn);
    tab_layout->SetFlexForView(dec_btn, 0);

    auto close_delegate = CefRefPtr<CefButtonDelegate>(
        new PanelActionDelegate(*this, PanelAction::Close));
    delegates_.push_back(close_delegate);
    auto close_btn = CefLabelButton::CreateLabelButton(close_delegate, "✕");
    tab_bar->AddChildView(close_btn);
    tab_layout->SetFlexForView(close_btn, 0);

    panel_->AddChildView(tab_bar);
    layout_->SetFlexForView(tab_bar, 0);
}

// M7.1: Connections sub-view — lists ConnectionRegistry entries.
void NetworkLabPanel::BuildConnectionsView() {
    if (connection_registry_ == nullptr) {
        auto empty_btn = CefLabelButton::CreateLabelButton(
            nullptr, "Connection diagnostics not available (registry not wired).");
        panel_->AddChildView(empty_btn);
        layout_->SetFlexForView(empty_btn, 0);
        return;
    }

    const auto connections = connection_registry_->All();
    if (connections.empty()) {
        auto empty_btn = CefLabelButton::CreateLabelButton(
            nullptr, "No connection diagnostics recorded yet.");
        panel_->AddChildView(empty_btn);
        layout_->SetFlexForView(empty_btn, 0);
        return;
    }

    const std::size_t max_rows = 8;
    const std::size_t start_idx = connections.size() > max_rows
        ? (connections.size() - max_rows) : 0;

    for (std::size_t i = start_idx; i < connections.size(); ++i) {
        const auto& conn = connections[i];

        auto row = CefPanel::CreatePanel(nullptr);
        CefBoxLayoutSettings row_settings{};
        row_settings.horizontal = 1;
        row_settings.between_child_spacing = 4;
        row_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
        auto row_layout = row->SetToBoxLayout(row_settings);

        // Protocol + reuse indicator
        std::string proto_label = "[" + (conn.protocol.empty() ? "?" : conn.protocol) + "]";
        if (conn.is_reused) { proto_label += " ♻"; }
        auto proto_btn = CefLabelButton::CreateLabelButton(nullptr, proto_label);
        row->AddChildView(proto_btn);
        row_layout->SetFlexForView(proto_btn, 0);

        // Host/endpoint
        std::string endpoint = conn.remote_host;
        if (!conn.remote_ip.empty() && conn.remote_ip != conn.remote_host) {
            endpoint += " (" + conn.remote_ip + ")";
        }
        if (endpoint.size() > 40) { endpoint = endpoint.substr(0, 37) + "..."; }
        auto host_btn = CefLabelButton::CreateLabelButton(nullptr, endpoint);
        row->AddChildView(host_btn);
        row_layout->SetFlexForView(host_btn, 1);

        // TLS info
        std::string tls_label = "plain";
        if (conn.tls_info.has_value()) {
            tls_label = conn.tls_info->tls_version + " / " + conn.tls_info->alpn;
        }
        auto tls_btn = CefLabelButton::CreateLabelButton(nullptr, tls_label);
        row->AddChildView(tls_btn);
        row_layout->SetFlexForView(tls_btn, 0);

        // Timing summary
        std::string timing = "DNS:";
        timing += conn.dns_ms >= 0.0 ? std::to_string(static_cast<int>(conn.dns_ms)) + "ms" : "?";
        timing += " TCP:";
        timing += conn.connect_ms >= 0.0 ? std::to_string(static_cast<int>(conn.connect_ms)) + "ms" : "?";
        auto timing_btn = CefLabelButton::CreateLabelButton(nullptr, timing);
        row->AddChildView(timing_btn);
        row_layout->SetFlexForView(timing_btn, 0);

        panel_->AddChildView(row);
        layout_->SetFlexForView(row, 0);
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) { window->Layout(); }
}

// M7.2: Decisions sub-view — lists FilterDecisionLog entries.
void NetworkLabPanel::BuildDecisionsView() {
    if (decision_log_ == nullptr) {
        auto empty_btn = CefLabelButton::CreateLabelButton(
            nullptr, "Filter decision log not available.");
        panel_->AddChildView(empty_btn);
        layout_->SetFlexForView(empty_btn, 0);
        return;
    }

    const auto decisions = decision_log_->All();
    if (decisions.empty()) {
        auto empty_btn = CefLabelButton::CreateLabelButton(
            nullptr, "No filter decisions recorded yet.");
        panel_->AddChildView(empty_btn);
        layout_->SetFlexForView(empty_btn, 0);
        return;
    }

    const std::size_t max_rows = 8;
    const std::size_t start_idx = decisions.size() > max_rows
        ? (decisions.size() - max_rows) : 0;

    for (std::size_t i = decisions.size(); i > start_idx; --i) {
        const auto& dec = decisions[i - 1];

        auto row = CefPanel::CreatePanel(nullptr);
        CefBoxLayoutSettings row_settings{};
        row_settings.horizontal = 1;
        row_settings.between_child_spacing = 4;
        row_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
        auto row_layout = row->SetToBoxLayout(row_settings);

        // Decision badge
        std::string badge = dec.blocked ? "✗ Block" : "✓ Allow";
        auto badge_btn = CefLabelButton::CreateLabelButton(nullptr, badge);
        row->AddChildView(badge_btn);
        row_layout->SetFlexForView(badge_btn, 0);

        // URL (truncated)
        std::string url = dec.url;
        if (url.size() > 50) { url = url.substr(0, 47) + "..."; }
        auto url_btn = CefLabelButton::CreateLabelButton(nullptr, url);
        row->AddChildView(url_btn);
        row_layout->SetFlexForView(url_btn, 1);

        // Layer + explanation (truncated)
        std::string explanation = dec.human_explanation;
        if (explanation.size() > 40) { explanation = explanation.substr(0, 37) + "..."; }
        auto expl_btn = CefLabelButton::CreateLabelButton(nullptr, explanation);
        row->AddChildView(expl_btn);
        row_layout->SetFlexForView(expl_btn, 0);

        panel_->AddChildView(row);
        layout_->SetFlexForView(row, 0);
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) { window->Layout(); }
}

}  // namespace openbrowser::desktop
