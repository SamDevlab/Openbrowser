#pragma once

#include "devtools/network/network_observation_sink.h"
#include "engine/browser_engine.h"

#include "include/cef_base.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_panel.h"

#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace openbrowser::desktop {

class CefBrowserEngine final : public engine::BrowserEngine, public CefBaseRefCounted {
public:
    explicit CefBrowserEngine(CefRefPtr<CefPanel> browser_host);

    CefBrowserEngine(const CefBrowserEngine&) = delete;
    CefBrowserEngine& operator=(const CefBrowserEngine&) = delete;

    void SetEventSink(engine::BrowserEngineEventSink* sink) noexcept override;
    void CreateTab(const core::Tab& tab) override;
    void CloseTab(const core::TabId& tab_id) override;
    void ActivateTab(const core::TabId& tab_id) override;
    void Navigate(const engine::NavigationRequest& request) override;
    void Suspend(const core::TabId& tab_id) override;
    void Resume(const core::TabId& tab_id) override;

    void SetNetworkObservationSink(devtools::network::NetworkObservationSink* sink) noexcept;
    [[nodiscard]] bool NetworkObservationEnabled() const noexcept;

    void NotifyBrowserCreated(const core::TabId& tab_id, CefRefPtr<CefBrowser> browser);
    void NotifyBrowserBeforeClose(const core::TabId& tab_id);
    void NotifyNavigationStarted(const core::TabId& tab_id, std::string url);
    void NotifyNavigationCommitted(const core::TabId& tab_id, std::string url);
    void NotifyNavigationFailed(
        const core::TabId& tab_id,
        std::string url,
        int error_code,
        std::string error_text);
    void NotifyTitleChanged(const core::TabId& tab_id, std::string title);
    void NotifyRendererCrashed(const core::TabId& tab_id, std::string reason);

    // May be called from CEF's IO thread. Delivery to the Network Lab sink is
    // always serialized onto the CEF UI thread.
    void PostNetworkEvent(devtools::network::NetworkEvent event);
    void DeliverNetworkEventOnUi(devtools::network::NetworkEvent event);

    void BeginWindowClose();
    [[nodiscard]] bool CanCloseWindow();
    void NotifyWindowDestroyed();

private:
    struct Surface {
        CefRefPtr<CefBrowserView> view;
        CefRefPtr<CefClient> client;
        std::optional<std::string> queued_navigation;
        bool browser_created{false};
        bool closing{false};
    };

    using SurfaceMap = std::unordered_map<core::TabId, Surface>;

    [[nodiscard]] SurfaceMap::iterator FindSurface(const core::TabId& tab_id);
    [[nodiscard]] SurfaceMap::const_iterator FindSurface(const core::TabId& tab_id) const;
    void HideActiveSurface();
    void MaybeQuitAfterClose();

    CefRefPtr<CefPanel> browser_host_;
    SurfaceMap surfaces_;
    std::optional<core::TabId> active_tab_id_;
    engine::BrowserEngineEventSink* event_sink_{nullptr};
    std::atomic<devtools::network::NetworkObservationSink*> network_sink_{nullptr};
    std::size_t live_browser_count_{0};
    bool window_close_requested_{false};
    bool window_destroyed_{false};
    bool message_loop_quit_requested_{false};

    IMPLEMENT_REFCOUNTING(CefBrowserEngine);
};

}  // namespace openbrowser::desktop
