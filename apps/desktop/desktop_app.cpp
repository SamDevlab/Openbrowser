#include "desktop_app.h"

#include "core/session/session_persistence.h"

#include "include/cef_command_line.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_fill_layout.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "include/wrapper/cef_helpers.h"

#include <memory>
#include <string>
#include <utility>

namespace openbrowser::desktop {
namespace {

class DesktopWindowDelegate final : public CefWindowDelegate {
public:
    DesktopWindowDelegate(
        CefRefPtr<CefPanel> tab_strip_panel,
        CefRefPtr<CefPanel> chrome_panel,
        CefRefPtr<CefPanel> focus_sidebar_panel,
        CefRefPtr<CefPanel> browser_host,
        CefRefPtr<CefPanel> network_lab_panel,
        CefRefPtr<CefBrowserEngine> engine)
        : tab_strip_panel_(std::move(tab_strip_panel)),
          chrome_panel_(std::move(chrome_panel)),
          focus_sidebar_panel_(std::move(focus_sidebar_panel)),
          browser_host_(std::move(browser_host)),
          network_lab_panel_(std::move(network_lab_panel)),
          engine_(std::move(engine)) {}

    DesktopWindowDelegate(const DesktopWindowDelegate&) = delete;
    DesktopWindowDelegate& operator=(const DesktopWindowDelegate&) = delete;

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
        CEF_REQUIRE_UI_THREAD();
        window->SetTitle("Openbrowser");

        CefRefPtr<CefPanel> root_panel = CefPanel::CreatePanel(nullptr);
        CefBoxLayoutSettings root_settings{};
        root_settings.horizontal = 0;
        root_settings.between_child_spacing = 0;
        root_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
        CefRefPtr<CefBoxLayout> root_layout = root_panel->SetToBoxLayout(root_settings);

        if (tab_strip_panel_) {
            root_panel->AddChildView(tab_strip_panel_);
            root_layout->SetFlexForView(tab_strip_panel_, 0);
        }
        if (chrome_panel_) {
            root_panel->AddChildView(chrome_panel_);
            root_layout->SetFlexForView(chrome_panel_, 0);
        }

        CefRefPtr<CefPanel> body_panel = CefPanel::CreatePanel(nullptr);
        CefBoxLayoutSettings body_settings{};
        body_settings.horizontal = 1;
        body_settings.between_child_spacing = 0;
        body_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
        CefRefPtr<CefBoxLayout> body_layout = body_panel->SetToBoxLayout(body_settings);

        if (focus_sidebar_panel_) {
            body_panel->AddChildView(focus_sidebar_panel_);
            body_layout->SetFlexForView(focus_sidebar_panel_, 0);
        }
        if (browser_host_) {
            body_panel->AddChildView(browser_host_);
            body_layout->SetFlexForView(browser_host_, 1);
        }

        root_panel->AddChildView(body_panel);
        root_layout->SetFlexForView(body_panel, 1);

        if (network_lab_panel_) {
            root_panel->AddChildView(network_lab_panel_);
            root_layout->SetFlexForView(network_lab_panel_, 0);
        }

        window->AddChildView(root_panel);
        root_panel->Layout();
        window->Show();
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow> /*window*/) override {
        CEF_REQUIRE_UI_THREAD();
        engine_->NotifyWindowDestroyed();
        tab_strip_panel_ = nullptr;
        chrome_panel_ = nullptr;
        focus_sidebar_panel_ = nullptr;
        browser_host_ = nullptr;
        network_lab_panel_ = nullptr;
        engine_ = nullptr;
    }

    bool CanClose(CefRefPtr<CefWindow> /*window*/) override {
        CEF_REQUIRE_UI_THREAD();
        engine_->BeginWindowClose();
        return engine_->CanCloseWindow();
    }

    CefSize GetPreferredSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(1280, 800);
    }

private:
    CefRefPtr<CefPanel> tab_strip_panel_;
    CefRefPtr<CefPanel> chrome_panel_;
    CefRefPtr<CefPanel> focus_sidebar_panel_;
    CefRefPtr<CefPanel> browser_host_;
    CefRefPtr<CefPanel> network_lab_panel_;
    CefRefPtr<CefBrowserEngine> engine_;

    IMPLEMENT_REFCOUNTING(DesktopWindowDelegate);
};

}  // namespace

CefRefPtr<CefBrowserProcessHandler> DesktopApp::GetBrowserProcessHandler() {
    return this;
}

void DesktopApp::OnContextInitialized() {
    CEF_REQUIRE_UI_THREAD();

    browser_host_ = CefPanel::CreatePanel(nullptr);
    browser_host_->SetToFillLayout();

    engine_ = new CefBrowserEngine(browser_host_);
    engine_->SetStorageRoot(StorageDirectory());

    capability_policy_ = std::make_unique<core::CapabilityPolicy>(
        core::CapabilityPolicy::CreateDefault());
    engine_->SetCapabilityPolicy(capability_policy_.get());

    transfer_broker_ = std::make_unique<core::TransferBroker>();
    engine_->SetTransferBroker(transfer_broker_.get());

    content_filter_ = std::make_unique<core::ContentFilter>();
    engine_->SetContentFilter(content_filter_.get());

    history_manager_ = std::make_unique<core::HistoryManager>();
    bookmark_manager_ = std::make_unique<core::BookmarkManager>();

    network_trace_ = std::make_unique<devtools::network::NetworkTraceBuffer>();
    engine_->SetNetworkObservationSink(network_trace_.get());

    workspace_manager_ = std::make_unique<core::WorkspaceManager>();
    focus_queue_ = std::make_unique<core::FocusQueue>();
    session_ = std::make_unique<core::BrowserSession>(*engine_);
    session_file_path_ = SessionFilePath();

    bool restored = false;
    const auto snapshot = core::SessionPersistence::LoadFromFile(session_file_path_);
    if (snapshot.has_value() && !snapshot->tabs.empty()) {
        restored = core::SessionPersistence::RestoreSession(*session_, *focus_queue_, *snapshot);
    }

    if (!restored || session_->Tabs().empty()) {
        core::Tab initial_tab;
        initial_tab.id = "initial";
        initial_tab.url = StartupUrl();
        initial_tab.title = "New tab";
        initial_tab.lifecycle = core::TabLifecycle::Active;
        initial_tab.workspace_id = std::nullopt;

        const bool opened = session_->OpenTab(std::move(initial_tab));

        if (!opened) {
            engine_->BeginWindowClose();
            CefQuitMessageLoop();
            return;
        }
    }

    tab_strip_ = std::make_unique<TabStrip>(*session_, workspace_manager_.get());
    chrome_ = std::make_unique<BrowserChrome>(*session_, engine_, [this]() {
        ToggleNetworkLab();
    });
    focus_sidebar_ = std::make_unique<FocusSidebar>(*session_, *focus_queue_);
    network_lab_panel_ = std::make_unique<NetworkLabPanel>(*network_trace_, *session_);

    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    const bool show_network_lab = command_line && command_line->HasSwitch("network-lab");
    network_lab_panel_->SetVisible(show_network_lab);

    session_->AddObserver(this);

    // Persist running session with clean_shutdown = false for crash detection
    SaveCurrentSession(false);

    CefWindow::CreateTopLevelWindow(
        new DesktopWindowDelegate(
            tab_strip_->View(),
            chrome_->View(),
            focus_sidebar_->View(),
            browser_host_,
            network_lab_panel_->View(),
            engine_));
}

void DesktopApp::ShutdownRuntime() {
    if (session_) {
        session_->RemoveObserver(this);
    }

    SaveCurrentSession(true);

    if (engine_) {
        engine_->SetTransferBroker(nullptr);
        engine_->SetContentFilter(nullptr);
        engine_->SetCapabilityPolicy(nullptr);
        engine_->SetNetworkObservationSink(nullptr);
    }

    bookmark_manager_.reset();
    history_manager_.reset();
    content_filter_.reset();
    transfer_broker_.reset();
    network_lab_panel_.reset();
    focus_sidebar_.reset();
    chrome_.reset();
    tab_strip_.reset();
    session_.reset();
    focus_queue_.reset();
    workspace_manager_.reset();
    network_trace_.reset();
    capability_policy_.reset();
    engine_ = nullptr;
    browser_host_ = nullptr;
}

void DesktopApp::OnBrowserSessionChanged(const core::BrowserSession& session) {
    CEF_REQUIRE_UI_THREAD();
    if (history_manager_ && session.ActiveTabId().has_value()) {
        const auto* active_tab = session.FindTab(*session.ActiveTabId());
        if (active_tab && !active_tab->url.empty() && active_tab->url != "about:blank") {
            history_manager_->RecordVisit(active_tab->url, active_tab->title);
        }
    }
    SaveCurrentSession(false);
}

void DesktopApp::ToggleNetworkLab() {
    CEF_REQUIRE_UI_THREAD();
    if (network_lab_panel_) {
        network_lab_panel_->ToggleVisibility();
    }
}

std::filesystem::path DesktopApp::StorageDirectory() const {
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    if (command_line && command_line->HasSwitch("storage-dir")) {
        const auto configured = command_line->GetSwitchValue("storage-dir").ToString();
        if (!configured.empty()) {
            return configured;
        }
    }

    return "openbrowser_storage";
}

std::filesystem::path DesktopApp::SessionFilePath() const {
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    if (command_line && command_line->HasSwitch("session-file")) {
        const auto configured = command_line->GetSwitchValue("session-file").ToString();
        if (!configured.empty()) {
            return configured;
        }
    }

    return "openbrowser_session.json";
}

void DesktopApp::SaveCurrentSession(const bool clean_shutdown) {
    if (!session_ || !focus_queue_ || session_file_path_.empty()) {
        return;
    }

    const auto snapshot = core::SessionPersistence::CaptureSnapshot(
        *session_, *focus_queue_, clean_shutdown);
    static_cast<void>(core::SessionPersistence::SaveToFile(session_file_path_, snapshot));
}

std::string DesktopApp::StartupUrl() const {
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    if (command_line) {
        const auto configured = command_line->GetSwitchValue("url").ToString();
        if (!configured.empty()) {
            return configured;
        }
    }

    return "https://example.com/";
}

}  // namespace openbrowser::desktop
