#pragma once

#include "browser_chrome.h"
#include "cef_browser_engine.h"
#include "core/session/browser_session.h"
#include "devtools/network/network_trace.h"

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"

#include <memory>
#include <string>

namespace openbrowser::desktop {

class DesktopApp final : public CefApp, public CefBrowserProcessHandler {
public:
    DesktopApp() = default;
    DesktopApp(const DesktopApp&) = delete;
    DesktopApp& operator=(const DesktopApp&) = delete;

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
    void OnContextInitialized() override;

    // Must run after CefRunMessageLoop returns and before CefShutdown.
    void ShutdownRuntime();

private:
    [[nodiscard]] std::string StartupUrl() const;

    CefRefPtr<CefPanel> browser_host_;
    CefRefPtr<CefBrowserEngine> engine_;
    std::unique_ptr<core::BrowserSession> session_;
    std::unique_ptr<BrowserChrome> chrome_;
    std::unique_ptr<devtools::network::NetworkTraceBuffer> network_trace_;

    IMPLEMENT_REFCOUNTING(DesktopApp);
};

}  // namespace openbrowser::desktop
