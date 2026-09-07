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
    core::BrowserSession& session)
    : trace_buffer_(trace_buffer), session_(session) {
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
    }
}

void NetworkLabPanel::RebuildView() {
    CEF_REQUIRE_UI_THREAD();
    panel_->RemoveAllChildViews();
    delegates_.clear();

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

}  // namespace openbrowser::desktop
