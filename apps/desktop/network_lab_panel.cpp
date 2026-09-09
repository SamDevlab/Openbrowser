#include "network_lab_panel.h"

#include "core/network/network_filter_query.h"
#include "core/session/browser_session.h"
#include "deferred_ui_action.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include <algorithm>
#include <utility>

namespace openbrowser::desktop {
namespace {

std::string ExtractHost(const std::string& url) {
    const auto scheme = url.find("://");
    const auto start = scheme == std::string::npos ? 0 : scheme + 3;
    const auto end = url.find_first_of("/?#", start);
    std::string authority = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
    const auto at = authority.rfind('@');
    if (at != std::string::npos) {
        authority.erase(0, at + 1);
    }
    if (!authority.empty() && authority.front() == '[') {
        const auto close = authority.find(']');
        return close == std::string::npos ? authority : authority.substr(0, close + 1);
    }
    const auto colon = authority.rfind(':');
    if (colon != std::string::npos) {
        authority.resize(colon);
    }
    return authority;
}

std::string FormatBytes(const std::size_t bytes) {
    if (bytes >= 1024 * 1024) {
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    }
    if (bytes >= 1024) {
        return std::to_string(bytes / 1024) + " KB";
    }
    return std::to_string(bytes) + " B";
}

}  // namespace

class NetworkLabPanel::PanelDelegate final : public CefPanelDelegate {
public:
    CefSize GetPreferredSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(1280, 300);
    }

    CefSize GetMinimumSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(500, 180);
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

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        NetworkLabPanel* panel = &panel_;
        const PanelAction action = action_;
        const std::string request_id = request_id_;
        PostDeferredUiAction(panel_.alive_token_, [panel, action, request_id]() {
            panel->HandleAction(action, request_id);
        });
    }

private:
    NetworkLabPanel& panel_;
    PanelAction action_;
    std::string request_id_;

    IMPLEMENT_REFCOUNTING(PanelActionDelegate);
};

class NetworkLabPanel::QueryFieldDelegate final : public CefTextfieldDelegate {
public:
    explicit QueryFieldDelegate(NetworkLabPanel& panel) : panel_(panel) {}

    void OnAfterUserAction(CefRefPtr<CefTextfield> textfield) override {
        CEF_REQUIRE_UI_THREAD();
        panel_.SetPendingStructuredQuery(textfield->GetText().ToString());
    }

    bool OnKeyEvent(CefRefPtr<CefTextfield> /*textfield*/, const CefKeyEvent& event) override {
        CEF_REQUIRE_UI_THREAD();
        if (event.type != KEYEVENT_RAWKEYDOWN && event.type != KEYEVENT_KEYDOWN) {
            return false;
        }
        if (event.windows_key_code == 13) {
            NetworkLabPanel* panel = &panel_;
            PostDeferredUiAction(panel_.alive_token_, [panel]() {
                panel->ApplyStructuredQuery();
            });
            return true;
        }
        if (event.windows_key_code == 27) {
            panel_.SetPendingStructuredQuery({});
            NetworkLabPanel* panel = &panel_;
            PostDeferredUiAction(panel_.alive_token_, [panel]() {
                panel->ApplyStructuredQuery();
            });
            return true;
        }
        return false;
    }

private:
    NetworkLabPanel& panel_;
    IMPLEMENT_REFCOUNTING(QueryFieldDelegate);
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
    query_delegate_ = new QueryFieldDelegate(*this);
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
    *alive_token_ = false;
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
    const bool was_visible = panel_->IsVisible();
    panel_->SetVisible(visible);
    auto window = panel_->GetWindow();
    if (window) {
        window->Layout();
    }
    if (was_visible != panel_->IsVisible() && on_visibility_changed_) {
        on_visibility_changed_(panel_->IsVisible());
    }
}

void NetworkLabPanel::SetVisibilityChangedCallback(VisibilityChangedCallback callback) {
    on_visibility_changed_ = std::move(callback);
}

bool NetworkLabPanel::IsVisible() const {
    CEF_REQUIRE_UI_THREAD();
    return panel_ && panel_->IsVisible();
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
    selected_request_id_ = std::nullopt;
    if (IsVisible()) {
        RebuildView();
    }
}

void NetworkLabPanel::OnBrowserSessionChanged(const core::BrowserSession& /*session*/) {
    CEF_REQUIRE_UI_THREAD();
    if (filter_active_tab_ && IsVisible()) {
        RebuildView();
    }
}

void NetworkLabPanel::SetPendingStructuredQuery(std::string query) {
    pending_structured_query_ = std::move(query);
}

void NetworkLabPanel::ApplyStructuredQuery() {
    structured_query_ = pending_structured_query_;
    selected_request_id_ = std::nullopt;
    RebuildView();
}

void NetworkLabPanel::HandleAction(const PanelAction action, const std::string& request_id) {
    CEF_REQUIRE_UI_THREAD();
    switch (action) {
        case PanelAction::ToggleScope:
            filter_active_tab_ = !filter_active_tab_;
            RebuildView();
            break;
        case PanelAction::CycleMethod:
            if (method_filter_ == "ALL") method_filter_ = "GET";
            else if (method_filter_ == "GET") method_filter_ = "POST";
            else method_filter_ = "ALL";
            RebuildView();
            break;
        case PanelAction::CycleStatus:
            if (status_filter_ == "ALL") status_filter_ = "2XX";
            else if (status_filter_ == "2XX") status_filter_ = "ERR";
            else status_filter_ = "ALL";
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
            break;
        case PanelAction::Close:
            SetVisible(false);
            break;
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
        case PanelAction::ExportObtrace:
            break;
    }
}

void NetworkLabPanel::RebuildView() {
    CEF_REQUIRE_UI_THREAD();
    panel_->RemoveAllChildViews();
    delegates_.clear();
    query_field_ = nullptr;

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

    devtools::network::NetworkTraceFilter legacy_filter{
        .tab_id = filter_active_tab_ ? session_.ActiveTabId() : std::nullopt,
        .search_query = {},
        .method_filter = method_filter_,
        .status_filter = status_filter_,
    };
    auto requests = trace_buffer_.QueryRequests(legacy_filter);
    if (!structured_query_.empty()) {
        const core::NetworkFilterQuery query(structured_query_);
        requests = query.Filter(requests);
    }
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

    auto title_btn = CefLabelButton::CreateLabelButton(
        nullptr, "Network Lab (" + std::to_string(requests.size()) + " reqs)");
    toolbar->AddChildView(title_btn);
    tool_layout->SetFlexForView(title_btn, 0);

    auto scope_delegate = CefRefPtr<CefButtonDelegate>(new PanelActionDelegate(*this, PanelAction::ToggleScope));
    delegates_.push_back(scope_delegate);
    auto scope_btn = CefLabelButton::CreateLabelButton(
        scope_delegate, filter_active_tab_ ? "[Scope: Tab]" : "[Scope: All]");
    toolbar->AddChildView(scope_btn);
    tool_layout->SetFlexForView(scope_btn, 0);

    auto method_delegate = CefRefPtr<CefButtonDelegate>(new PanelActionDelegate(*this, PanelAction::CycleMethod));
    delegates_.push_back(method_delegate);
    auto method_btn = CefLabelButton::CreateLabelButton(method_delegate, "[Method: " + method_filter_ + "]");
    toolbar->AddChildView(method_btn);
    tool_layout->SetFlexForView(method_btn, 0);

    auto status_delegate = CefRefPtr<CefButtonDelegate>(new PanelActionDelegate(*this, PanelAction::CycleStatus));
    delegates_.push_back(status_delegate);
    auto status_btn = CefLabelButton::CreateLabelButton(status_delegate, "[Status: " + status_filter_ + "]");
    toolbar->AddChildView(status_btn);
    tool_layout->SetFlexForView(status_btn, 0);

    auto clear_delegate = CefRefPtr<CefButtonDelegate>(new PanelActionDelegate(*this, PanelAction::Clear));
    delegates_.push_back(clear_delegate);
    auto clear_btn = CefLabelButton::CreateLabelButton(clear_delegate, "Clear");
    toolbar->AddChildView(clear_btn);
    tool_layout->SetFlexForView(clear_btn, 0);

    panel_->AddChildView(toolbar);
    layout_->SetFlexForView(toolbar, 0);

    query_field_ = CefTextfield::CreateTextfield(query_delegate_);
    query_field_->SetPlaceholderText(
        "Filter: method:POST status:>=400 host:github.com blocked:true is:failed (Enter to apply)");
    query_field_->SetText(structured_query_);
    panel_->AddChildView(query_field_);
    layout_->SetFlexForView(query_field_, 0);

    auto columns = CefLabelButton::CreateLabelButton(
        nullptr, "Method/Status | Host | URL | Size | Observed | Blocked | Attribution");
    panel_->AddChildView(columns);
    layout_->SetFlexForView(columns, 0);

    if (requests.empty()) {
        auto empty_btn = CefLabelButton::CreateLabelButton(nullptr, "No requests match active filters.");
        panel_->AddChildView(empty_btn);
        layout_->SetFlexForView(empty_btn, 0);
    } else {
        const std::size_t max_rows = 7;
        const std::size_t start_idx = requests.size() > max_rows ? requests.size() - max_rows : 0;

        for (std::size_t i = requests.size(); i > start_idx; --i) {
            const auto& req = requests[i - 1];
            auto row = CefPanel::CreatePanel(nullptr);
            CefBoxLayoutSettings row_settings{};
            row_settings.horizontal = 1;
            row_settings.between_child_spacing = 4;
            row_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
            auto row_layout = row->SetToBoxLayout(row_settings);

            const std::string status = req.status.has_value()
                ? std::to_string(*req.status)
                : (req.state == devtools::network::RequestState::Failed ? "ERR" : "...");
            auto select_delegate = CefRefPtr<CefButtonDelegate>(
                new PanelActionDelegate(*this, PanelAction::SelectRequest, req.request_id));
            delegates_.push_back(select_delegate);

            auto method_btn_row = CefLabelButton::CreateLabelButton(select_delegate, "[" + req.method + "] " + status);
            row->AddChildView(method_btn_row);
            row_layout->SetFlexForView(method_btn_row, 0);

            std::string host = ExtractHost(req.url);
            if (host.size() > 24) host = host.substr(0, 21) + "...";
            auto host_btn = CefLabelButton::CreateLabelButton(select_delegate, host);
            row->AddChildView(host_btn);
            row_layout->SetFlexForView(host_btn, 0);

            std::string display_url = req.url;
            if (display_url.size() > 48) display_url = display_url.substr(0, 45) + "...";
            auto url_btn = CefLabelButton::CreateLabelButton(select_delegate, display_url);
            row->AddChildView(url_btn);
            row_layout->SetFlexForView(url_btn, 1);

            auto size_btn = CefLabelButton::CreateLabelButton(select_delegate, FormatBytes(req.transferred_bytes));
            row->AddChildView(size_btn);
            row_layout->SetFlexForView(size_btn, 0);

            auto time_btn = CefLabelButton::CreateLabelButton(
                select_delegate, std::to_string(req.observed_duration_ms) + " ms");
            row->AddChildView(time_btn);
            row_layout->SetFlexForView(time_btn, 0);

            auto blocked_btn = CefLabelButton::CreateLabelButton(
                select_delegate, req.filter_blocked ? "Blocked" : "-");
            row->AddChildView(blocked_btn);
            row_layout->SetFlexForView(blocked_btn, 0);

            auto attr_btn = CefLabelButton::CreateLabelButton(
                select_delegate, core::AttributionToString(req.attribution));
            row->AddChildView(attr_btn);
            row_layout->SetFlexForView(attr_btn, 0);

            panel_->AddChildView(row);
            layout_->SetFlexForView(row, 0);
        }
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) window->Layout();
}

void NetworkLabPanel::BuildDetailsView(
    const devtools::network::NetworkRequestSummary& req) {
    auto header = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings header_settings{};
    header_settings.horizontal = 1;
    header_settings.between_child_spacing = 6;
    header_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    auto header_layout = header->SetToBoxLayout(header_settings);

    auto back_delegate = CefRefPtr<CefButtonDelegate>(new PanelActionDelegate(*this, PanelAction::DeselectRequest));
    delegates_.push_back(back_delegate);
    auto back_btn = CefLabelButton::CreateLabelButton(back_delegate, "Back");
    header->AddChildView(back_btn);
    header_layout->SetFlexForView(back_btn, 0);

    const std::string status = req.status.has_value()
        ? std::to_string(*req.status)
        : (req.state == devtools::network::RequestState::Failed ? "ERR" : "Pending");
    std::string title = "[" + req.method + " " + status + "] " + req.url;
    if (title.size() > 85) title = title.substr(0, 82) + "...";
    auto title_btn = CefLabelButton::CreateLabelButton(nullptr, title);
    header->AddChildView(title_btn);
    header_layout->SetFlexForView(title_btn, 1);

    panel_->AddChildView(header);
    layout_->SetFlexForView(header, 0);

    const std::string protocol = req.protocol.empty() ? "unknown" : req.protocol;
    std::string overview = "Protocol: " + protocol +
        " | Transferred: " + FormatBytes(req.transferred_bytes) +
        " | Observed callback span: " + std::to_string(req.observed_duration_ms) + " ms" +
        " | Attribution: " + core::AttributionToString(req.attribution) +
        " | Blocked: " + (req.filter_blocked ? "yes" : "no");
    if (req.connection_id.has_value()) overview += " | Connection: " + *req.connection_id;
    if (!req.error.empty()) overview += " | Error: " + req.error;
    auto overview_btn = CefLabelButton::CreateLabelButton(nullptr, overview);
    panel_->AddChildView(overview_btn);
    layout_->SetFlexForView(overview_btn, 0);

    if (req.filter_blocked || !req.filter_rule_source.empty()) {
        std::string decision = std::string{"Filter decision: "} +
            (req.filter_blocked ? "BLOCK" : "ALLOW");
        if (!req.filter_layer.empty()) decision += " | Layer: " + req.filter_layer;
        if (!req.filter_rule_source.empty()) decision += " | Rule: " + req.filter_rule_source;
        auto decision_btn = CefLabelButton::CreateLabelButton(nullptr, decision);
        panel_->AddChildView(decision_btn);
        layout_->SetFlexForView(decision_btn, 0);
    }

    if (!req.request_headers.empty()) {
        auto title_hdr = CefLabelButton::CreateLabelButton(
            nullptr, "--- Request Headers (" + std::to_string(req.request_headers.size()) + ") ---");
        panel_->AddChildView(title_hdr);
        const std::size_t max_show = std::min(req.request_headers.size(), static_cast<std::size_t>(6));
        for (std::size_t i = 0; i < max_show; ++i) {
            std::string line = req.request_headers[i].name + ": " + req.request_headers[i].value;
            if (line.size() > 100) line = line.substr(0, 97) + "...";
            panel_->AddChildView(CefLabelButton::CreateLabelButton(nullptr, "  " + line));
        }
    }

    if (!req.response_headers.empty()) {
        auto title_hdr = CefLabelButton::CreateLabelButton(
            nullptr, "--- Response Headers (" + std::to_string(req.response_headers.size()) + ") ---");
        panel_->AddChildView(title_hdr);
        const std::size_t max_show = std::min(req.response_headers.size(), static_cast<std::size_t>(6));
        for (std::size_t i = 0; i < max_show; ++i) {
            std::string line = req.response_headers[i].name + ": " + req.response_headers[i].value;
            if (line.size() > 100) line = line.substr(0, 97) + "...";
            panel_->AddChildView(CefLabelButton::CreateLabelButton(nullptr, "  " + line));
        }
    }

    if (!req.body_preview.empty()) {
        panel_->AddChildView(CefLabelButton::CreateLabelButton(nullptr, "--- Captured Body Preview (not exported raw) ---"));
        std::string body = req.body_preview;
        if (body.size() > 120) body = body.substr(0, 117) + "...";
        panel_->AddChildView(CefLabelButton::CreateLabelButton(nullptr, "  " + body));
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) window->Layout();
}

void NetworkLabPanel::BuildTabBar() {
    auto tab_bar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings settings{};
    settings.horizontal = 1;
    settings.between_child_spacing = 4;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    auto tab_layout = tab_bar->SetToBoxLayout(settings);

    auto req_delegate = CefRefPtr<CefButtonDelegate>(new PanelActionDelegate(*this, PanelAction::SwitchToRequests));
    delegates_.push_back(req_delegate);
    auto req_btn = CefLabelButton::CreateLabelButton(
        req_delegate, active_tab_ == ActiveTab::Requests ? "[Requests]" : "Requests");
    tab_bar->AddChildView(req_btn);
    tab_layout->SetFlexForView(req_btn, 0);

    auto conn_delegate = CefRefPtr<CefButtonDelegate>(new PanelActionDelegate(*this, PanelAction::SwitchToConnections));
    delegates_.push_back(conn_delegate);
    auto conn_btn = CefLabelButton::CreateLabelButton(
        conn_delegate, active_tab_ == ActiveTab::Connections ? "[Connections]" : "Connections");
    tab_bar->AddChildView(conn_btn);
    tab_layout->SetFlexForView(conn_btn, 0);

    auto dec_delegate = CefRefPtr<CefButtonDelegate>(new PanelActionDelegate(*this, PanelAction::SwitchToDecisions));
    delegates_.push_back(dec_delegate);
    auto dec_btn = CefLabelButton::CreateLabelButton(
        dec_delegate, active_tab_ == ActiveTab::Decisions ? "[Decisions]" : "Decisions");
    tab_bar->AddChildView(dec_btn);
    tab_layout->SetFlexForView(dec_btn, 0);

    auto close_delegate = CefRefPtr<CefButtonDelegate>(new PanelActionDelegate(*this, PanelAction::Close));
    delegates_.push_back(close_delegate);
    auto close_btn = CefLabelButton::CreateLabelButton(close_delegate, "x");
    tab_bar->AddChildView(close_btn);
    tab_layout->SetFlexForView(close_btn, 0);

    panel_->AddChildView(tab_bar);
    layout_->SetFlexForView(tab_bar, 0);
}

void NetworkLabPanel::BuildConnectionsView() {
    if (connection_registry_ == nullptr) {
        panel_->AddChildView(CefLabelButton::CreateLabelButton(nullptr, "Connection diagnostics not available."));
        return;
    }

    const auto connections = connection_registry_->All();
    if (connections.empty()) {
        panel_->AddChildView(CefLabelButton::CreateLabelButton(nullptr, "No connection diagnostics recorded yet."));
        return;
    }

    const std::size_t max_rows = 8;
    const std::size_t start_idx = connections.size() > max_rows ? connections.size() - max_rows : 0;
    for (std::size_t i = start_idx; i < connections.size(); ++i) {
        const auto& conn = connections[i];
        auto row = CefPanel::CreatePanel(nullptr);
        CefBoxLayoutSettings row_settings{};
        row_settings.horizontal = 1;
        row_settings.between_child_spacing = 4;
        row_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
        auto row_layout = row->SetToBoxLayout(row_settings);

        std::string proto = "[" + (conn.protocol.empty() ? "unknown" : conn.protocol) + "]";
        if (conn.is_reused) proto += " reused";
        auto proto_btn = CefLabelButton::CreateLabelButton(nullptr, proto);
        row->AddChildView(proto_btn);
        row_layout->SetFlexForView(proto_btn, 0);

        std::string endpoint = conn.remote_host;
        if (!conn.remote_ip.empty() && conn.remote_ip != conn.remote_host) endpoint += " (" + conn.remote_ip + ")";
        if (endpoint.size() > 42) endpoint = endpoint.substr(0, 39) + "...";
        auto host_btn = CefLabelButton::CreateLabelButton(nullptr, endpoint);
        row->AddChildView(host_btn);
        row_layout->SetFlexForView(host_btn, 1);

        std::string tls = "TLS: unknown";
        if (conn.tls_info.has_value()) {
            tls = conn.tls_info->tls_version.empty() ? "TLS: observed" : conn.tls_info->tls_version;
            if (!conn.tls_info->alpn.empty()) tls += " / " + conn.tls_info->alpn;
        }
        row->AddChildView(CefLabelButton::CreateLabelButton(nullptr, tls));

        std::string timing = "DNS:";
        timing += conn.dns_ms >= 0.0 ? std::to_string(static_cast<int>(conn.dns_ms)) + "ms" : "?";
        timing += " TCP:";
        timing += conn.connect_ms >= 0.0 ? std::to_string(static_cast<int>(conn.connect_ms)) + "ms" : "?";
        row->AddChildView(CefLabelButton::CreateLabelButton(nullptr, timing));

        panel_->AddChildView(row);
        layout_->SetFlexForView(row, 0);
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) window->Layout();
}

void NetworkLabPanel::BuildDecisionsView() {
    if (decision_log_ == nullptr) {
        panel_->AddChildView(CefLabelButton::CreateLabelButton(nullptr, "Filter decision log not available."));
        return;
    }

    const auto decisions = decision_log_->All();
    if (decisions.empty()) {
        panel_->AddChildView(CefLabelButton::CreateLabelButton(nullptr, "No filter decisions recorded yet."));
        return;
    }

    const std::size_t max_rows = 8;
    const std::size_t start_idx = decisions.size() > max_rows ? decisions.size() - max_rows : 0;
    for (std::size_t i = decisions.size(); i > start_idx; --i) {
        const auto& dec = decisions[i - 1];
        auto row = CefPanel::CreatePanel(nullptr);
        CefBoxLayoutSettings row_settings{};
        row_settings.horizontal = 1;
        row_settings.between_child_spacing = 4;
        row_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
        auto row_layout = row->SetToBoxLayout(row_settings);

        auto badge = CefLabelButton::CreateLabelButton(nullptr, dec.blocked ? "Block" : "Allow");
        row->AddChildView(badge);
        row_layout->SetFlexForView(badge, 0);

        std::string url = dec.url;
        if (url.size() > 50) url = url.substr(0, 47) + "...";
        auto url_btn = CefLabelButton::CreateLabelButton(nullptr, url);
        row->AddChildView(url_btn);
        row_layout->SetFlexForView(url_btn, 1);

        std::string explanation = dec.human_explanation;
        if (explanation.size() > 46) explanation = explanation.substr(0, 43) + "...";
        row->AddChildView(CefLabelButton::CreateLabelButton(nullptr, explanation));

        panel_->AddChildView(row);
        layout_->SetFlexForView(row, 0);
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) window->Layout();
}

}  // namespace openbrowser::desktop