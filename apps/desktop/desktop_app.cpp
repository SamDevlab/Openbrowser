#include "desktop_app.h"

#include "include/cef_command_line.h"
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
        CefRefPtr<CefPanel> browser_host,
        CefRefPtr<CefBrowserEngine> engine)
        : browser_host_(std::move(browser_host)), engine_(std::move(engine)) {}

    DesktopWindowDelegate(const DesktopWindowDelegate&) = delete;
    DesktopWindowDelegate& operator=(const DesktopWindowDelegate&) = delete;

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
        CEF_REQUIRE_UI_THREAD();
        window->SetTitle("Openbrowser");
        window->AddChildView(browser_host_);
        browser_host_->Layout();
        window->Show();
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
        CEF_REQUIRE_UI_THREAD();
        engine_->NotifyWindowDestroyed();
        browser_host_ = nullptr;
        engine_ = nullptr;
    }

    bool CanClose(CefRefPtr<CefWindow> window) override {
        CEF_REQUIRE_UI_THREAD();
        engine_->BeginWindowClose();
        return engine_->CanCloseWindow();
    }

    CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
        return CefSize(1280, 800);
    }

private:
    CefRefPtr<CefPanel> browser_host_;
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

    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    if (command_line && command_line->HasSwitch("network-lab")) {
        network_trace_ = std::make_unique<devtools::network::NetworkTraceBuffer>();
        engine_->SetNetworkObservationSink(network_trace_.get());
    }

    session_ = std::make_unique<core::BrowserSession>(*engine_);
    const bool opened = session_->OpenTab({
        .id = "initial",
        .url = StartupUrl(),
        .title = "New tab",
        .lifecycle = core::TabLifecycle::Active,
        .workspace_id = std::nullopt,
    });

    if (!opened) {
        engine_->BeginWindowClose();
        CefQuitMessageLoop();
        return;
    }

    CefWindow::CreateTopLevelWindow(new DesktopWindowDelegate(browser_host_, engine_));
}

void DesktopApp::ShutdownRuntime() {
    if (engine_) {
        engine_->SetNetworkObservationSink(nullptr);
    }

    session_.reset();
    network_trace_.reset();
    engine_ = nullptr;
    browser_host_ = nullptr;
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
