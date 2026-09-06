#include "network_lab_panel.h"

#include "core/session/browser_session.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include <utility>

namespace openbrowser::desktop {

class NetworkLabPanel::PanelDelegate final : public CefPanelDelegate {
public:
    CefSize GetPreferredSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(1280, 200);
    }

    CefSize GetMinimumSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(400, 140);
    }

    IMPLEMENT_REFCOUNTING(PanelDelegate);
};

class NetworkLabPanel::PanelActionDelegate final : public CefButtonDelegate {
public:
    PanelActionDelegate(NetworkLabPanel& panel, const PanelAction action)
        : panel_(panel), action_(action) {}

    PanelActionDelegate(const PanelActionDelegate&) = delete;
    PanelActionDelegate& operator=(const PanelActionDelegate&) = delete;

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        panel_.HandleAction(action_);
    }

private:
    NetworkLabPanel& panel_;
    PanelAction action_;

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
        RebuildView();
    }
}

void NetworkLabPanel::OnBrowserSessionChanged(const core::BrowserSession& /*session*/) {
    CEF_REQUIRE_UI_THREAD();
    if (filter_active_tab_ && IsVisible()) {
        RebuildView();
    }
}

void NetworkLabPanel::HandleAction(const PanelAction action) {
    CEF_REQUIRE_UI_THREAD();
    switch (action) {
        case PanelAction::ToggleScope:
            filter_active_tab_ = !filter_active_tab_;
            RebuildView();
            break;
        case PanelAction::Clear:
            trace_buffer_.Clear();
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

    const auto filter_tab = filter_active_tab_ ? session_.ActiveTabId() : std::nullopt;
    const auto requests = trace_buffer_.AggregateRequests(filter_tab);

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
    std::string scope_label = filter_active_tab_ ? "[Scope: Active Tab]" : "[Scope: All]";
    auto scope_btn = CefLabelButton::CreateLabelButton(scope_delegate, scope_label);
    toolbar->AddChildView(scope_btn);
    tool_layout->SetFlexForView(scope_btn, 0);

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
        auto method_btn = CefLabelButton::CreateLabelButton(nullptr, method_label);
        row_panel->AddChildView(method_btn);
        row_layout->SetFlexForView(method_btn, 0);

        std::string display_url = req.url;
        if (display_url.size() > 60) {
            display_url = display_url.substr(0, 57) + "...";
        }
        auto url_btn = CefLabelButton::CreateLabelButton(nullptr, display_url);
        row_panel->AddChildView(url_btn);
        row_layout->SetFlexForView(url_btn, 1);

        std::string size_str = std::to_string(req.transferred_bytes) + " B";
        if (req.transferred_bytes >= 1024) {
            size_str = std::to_string(req.transferred_bytes / 1024) + " KB";
        }
        auto size_btn = CefLabelButton::CreateLabelButton(nullptr, size_str);
        row_panel->AddChildView(size_btn);
        row_layout->SetFlexForView(size_btn, 0);

        panel_->AddChildView(row_panel);
        layout_->SetFlexForView(row_panel, 0);
    }

    panel_->Layout();
    auto window = panel_->GetWindow();
    if (window) {
        window->Layout();
    }
}

}  // namespace openbrowser::desktop
