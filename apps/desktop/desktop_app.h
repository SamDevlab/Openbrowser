#pragma once

#include "browser_chrome.h"
#include "cef_browser_engine.h"
#include "core/capabilities/capability_policy.h"
#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session.h"
#include "core/session/browser_session_observer.h"
#include "devtools/network/network_trace.h"
#include "focus_sidebar.h"
#include "network_lab_panel.h"
#include "tab_strip.h"

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"

#include <filesystem>
#include <memory>
#include <string>

namespace openbrowser::desktop {

class DesktopApp final : public CefApp,
                         public CefBrowserProcessHandler,
                         public core::BrowserSessionObserver {
public:
    DesktopApp() = default;
    DesktopApp(const DesktopApp&) = delete;
    DesktopApp& operator=(const DesktopApp&) = delete;

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
    void OnContextInitialized() override;

    // Must run after CefRunMessageLoop returns and before CefShutdown.
    void ShutdownRuntime();

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    [[nodiscard]] std::string StartupUrl() const;
    [[nodiscard]] std::filesystem::path SessionFilePath() const;
    void SaveCurrentSession(bool clean_shutdown);
    void ToggleNetworkLab();

    CefRefPtr<CefPanel> browser_host_;
    CefRefPtr<CefBrowserEngine> engine_;
    std::unique_ptr<core::FocusQueue> focus_queue_;
    std::unique_ptr<core::BrowserSession> session_;
    std::unique_ptr<TabStrip> tab_strip_;
    std::unique_ptr<BrowserChrome> chrome_;
    std::unique_ptr<FocusSidebar> focus_sidebar_;
    std::unique_ptr<devtools::network::NetworkTraceBuffer> network_trace_;
    std::unique_ptr<NetworkLabPanel> network_lab_panel_;
    std::unique_ptr<core::CapabilityPolicy> capability_policy_;
    std::filesystem::path session_file_path_;

    IMPLEMENT_REFCOUNTING(DesktopApp);
};

}  // namespace openbrowser::desktop
