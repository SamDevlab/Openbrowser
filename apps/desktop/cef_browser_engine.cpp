#include "cef_browser_engine.h"

#include "cef_tab_client.h"
#include "devtools/network/network_trace.h"

#include "include/cef_app.h"
#include "include/cef_frame.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_helpers.h"

#include <utility>
#include <vector>

namespace openbrowser::desktop {
namespace {

class NetworkEventTask final : public CefTask {
public:
    NetworkEventTask(
        CefRefPtr<CefBrowserEngine> engine,
        devtools::network::NetworkEvent event)
        : engine_(std::move(engine)), event_(std::move(event)) {}

    void Execute() override {
        engine_->DeliverNetworkEventOnUi(std::move(event_));
    }

private:
    CefRefPtr<CefBrowserEngine> engine_;
    devtools::network::NetworkEvent event_;

    IMPLEMENT_REFCOUNTING(NetworkEventTask);
};

}  // namespace

CefBrowserEngine::CefBrowserEngine(CefRefPtr<CefPanel> browser_host)
    : browser_host_(std::move(browser_host)) {}

void CefBrowserEngine::SetEventSink(engine::BrowserEngineEventSink* sink) noexcept {
    event_sink_ = sink;
}

void CefBrowserEngine::CreateTab(const core::Tab& tab) {
    CEF_REQUIRE_UI_THREAD();

    if (!browser_host_ || tab.id.empty() || tab.url.empty() || FindSurface(tab.id) != surfaces_.end()) {
        return;
    }

    CefRefPtr<CefBrowserEngine> self(this);
    CefRefPtr<CefTabClient> client(new CefTabClient(tab.id, self));
    CefBrowserSettings browser_settings;
    CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
        client,
        tab.url,
        browser_settings,
        nullptr,
        nullptr,
        nullptr);

    if (!view) {
        NotifyNavigationFailed(tab.id, tab.url, -1, "CEF failed to create BrowserView");
        return;
    }

    view->SetVisible(false);

    auto [surface_it, inserted] = surfaces_.emplace(
        tab.id,
        Surface{
            .view = view,
            .client = client,
            .queued_navigation = std::nullopt,
            .browser_created = false,
            .closing = false,
        });

    if (!inserted) {
        return;
    }

    browser_host_->AddChildView(surface_it->second.view);
    browser_host_->Layout();
}

void CefBrowserEngine::CloseTab(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();

    const auto surface = FindSurface(tab_id);
    if (surface == surfaces_.end() || surface->second.closing) {
        return;
    }

    surface->second.closing = true;
    surface->second.view->SetVisible(false);

    if (active_tab_id_.has_value() && *active_tab_id_ == tab_id) {
        active_tab_id_.reset();
    }

    CefRefPtr<CefBrowser> browser = surface->second.view->GetBrowser();
    if (browser) {
        browser->GetHost()->CloseBrowser(false);
        return;
    }

    browser_host_->RemoveChildView(surface->second.view);
    surfaces_.erase(surface);
    browser_host_->Layout();
}

void CefBrowserEngine::ActivateTab(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();

    const auto surface = FindSurface(tab_id);
    if (surface == surfaces_.end() || surface->second.closing) {
        return;
    }

    HideActiveSurface();
    surface->second.view->SetVisible(true);
    browser_host_->Layout();
    surface->second.view->RequestFocus();
    active_tab_id_ = tab_id;
}

void CefBrowserEngine::Navigate(const engine::NavigationRequest& request) {
    CEF_REQUIRE_UI_THREAD();

    const auto surface = FindSurface(request.tab_id);
    if (surface == surfaces_.end() || surface->second.closing || request.url.empty()) {
        return;
    }

    CefRefPtr<CefBrowser> browser = surface->second.view->GetBrowser();
    if (!browser) {
        surface->second.queued_navigation = request.url;
        return;
    }

    CefRefPtr<CefFrame> frame = browser->GetMainFrame();
    if (frame) {
        frame->LoadURL(request.url);
    }
}

void CefBrowserEngine::GoBack(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();
    CefRefPtr<CefBrowser> browser = BrowserForCommand(tab_id);
    if (browser && browser->CanGoBack()) {
        browser->GoBack();
    }
}

void CefBrowserEngine::GoForward(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();
    CefRefPtr<CefBrowser> browser = BrowserForCommand(tab_id);
    if (browser && browser->CanGoForward()) {
        browser->GoForward();
    }
}

void CefBrowserEngine::Reload(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();
    CefRefPtr<CefBrowser> browser = BrowserForCommand(tab_id);
    if (browser) {
        browser->Reload();
    }
}

void CefBrowserEngine::Suspend(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();

    const auto surface = FindSurface(tab_id);
    if (surface == surfaces_.end() || surface->second.closing) {
        return;
    }

    surface->second.view->SetVisible(false);
}

void CefBrowserEngine::Resume(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();

    const auto surface = FindSurface(tab_id);
    if (surface == surfaces_.end() || surface->second.closing) {
        return;
    }

    if (active_tab_id_.has_value() && *active_tab_id_ == tab_id) {
        surface->second.view->SetVisible(true);
        surface->second.view->RequestFocus();
    }
}

void CefBrowserEngine::SetNetworkObservationSink(
    devtools::network::NetworkObservationSink* sink) noexcept {
    network_sink_.store(sink, std::memory_order_release);
}

bool CefBrowserEngine::NetworkObservationEnabled() const noexcept {
    return network_sink_.load(std::memory_order_acquire) != nullptr;
}

void CefBrowserEngine::SetCapabilityPolicy(
    const core::CapabilityPolicy* policy) noexcept {
    capability_policy_.store(policy, std::memory_order_release);
}

const core::CapabilityPolicy* CefBrowserEngine::Policy() const noexcept {
    return capability_policy_.load(std::memory_order_acquire);
}

void CefBrowserEngine::NotifyBrowserCreated(
    const core::TabId& tab_id,
    CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    const auto surface = FindSurface(tab_id);
    if (surface == surfaces_.end() || surface->second.browser_created) {
        return;
    }

    surface->second.browser_created = true;
    ++live_browser_count_;

    if (surface->second.queued_navigation.has_value() && browser) {
        CefRefPtr<CefFrame> frame = browser->GetMainFrame();
        if (frame) {
            frame->LoadURL(*surface->second.queued_navigation);
        }
        surface->second.queued_navigation.reset();
    }
}

void CefBrowserEngine::NotifyBrowserBeforeClose(const core::TabId& tab_id) {
    CEF_REQUIRE_UI_THREAD();

    const auto surface = FindSurface(tab_id);
    if (surface != surfaces_.end()) {
        if (surface->second.browser_created && live_browser_count_ > 0) {
            --live_browser_count_;
        }

        if (browser_host_ && surface->second.view) {
            browser_host_->RemoveChildView(surface->second.view);
        }
        surfaces_.erase(surface);

        if (active_tab_id_.has_value() && *active_tab_id_ == tab_id) {
            active_tab_id_.reset();
        }

        if (browser_host_) {
            browser_host_->Layout();
        }
    }

    MaybeQuitAfterClose();
}

void CefBrowserEngine::NotifyNavigationStarted(const core::TabId& tab_id, std::string url) {
    CEF_REQUIRE_UI_THREAD();
    if (event_sink_ != nullptr) {
        event_sink_->OnNavigationStarted({.tab_id = tab_id, .url = std::move(url)});
    }
}

void CefBrowserEngine::NotifyNavigationCommitted(const core::TabId& tab_id, std::string url) {
    CEF_REQUIRE_UI_THREAD();
    if (event_sink_ != nullptr) {
        event_sink_->OnNavigationCommitted({.tab_id = tab_id, .url = std::move(url)});
    }
}

void CefBrowserEngine::NotifyNavigationFailed(
    const core::TabId& tab_id,
    std::string url,
    const int error_code,
    std::string error_text) {
    CEF_REQUIRE_UI_THREAD();
    if (event_sink_ != nullptr) {
        event_sink_->OnNavigationFailed({
            .tab_id = tab_id,
            .url = std::move(url),
            .error_code = error_code,
            .error_text = std::move(error_text),
        });
    }
}

void CefBrowserEngine::NotifyTitleChanged(const core::TabId& tab_id, std::string title) {
    CEF_REQUIRE_UI_THREAD();
    if (event_sink_ != nullptr) {
        event_sink_->OnTitleChanged({.tab_id = tab_id, .title = std::move(title)});
    }
}

void CefBrowserEngine::NotifyRendererCrashed(const core::TabId& tab_id, std::string reason) {
    CEF_REQUIRE_UI_THREAD();
    if (event_sink_ != nullptr) {
        event_sink_->OnRendererCrashed({.tab_id = tab_id, .reason = std::move(reason)});
    }
}

void CefBrowserEngine::PostNetworkEvent(devtools::network::NetworkEvent event) {
    if (!NetworkObservationEnabled()) {
        return;
    }

    if (CefCurrentlyOn(TID_UI)) {
        DeliverNetworkEventOnUi(std::move(event));
        return;
    }

    CefRefPtr<CefBrowserEngine> self(this);
    CefPostTask(TID_UI, new NetworkEventTask(std::move(self), std::move(event)));
}

void CefBrowserEngine::DeliverNetworkEventOnUi(devtools::network::NetworkEvent event) {
    CEF_REQUIRE_UI_THREAD();
    auto* sink = network_sink_.load(std::memory_order_acquire);
    if (sink != nullptr) {
        sink->OnNetworkEvent(std::move(event));
    }
}

void CefBrowserEngine::BeginWindowClose() {
    CEF_REQUIRE_UI_THREAD();
    window_close_requested_ = true;
}

bool CefBrowserEngine::CanCloseWindow() {
    CEF_REQUIRE_UI_THREAD();

    std::vector<core::TabId> uncreated_surfaces;
    bool can_close = true;

    for (auto& [tab_id, surface] : surfaces_) {
        CefRefPtr<CefBrowser> browser = surface.view->GetBrowser();
        if (!browser) {
            uncreated_surfaces.push_back(tab_id);
            continue;
        }

        if (!browser->GetHost()->TryCloseBrowser()) {
            can_close = false;
        }
    }

    for (const auto& tab_id : uncreated_surfaces) {
        const auto surface = FindSurface(tab_id);
        if (surface != surfaces_.end()) {
            browser_host_->RemoveChildView(surface->second.view);
            surfaces_.erase(surface);
        }
    }

    browser_host_->Layout();
    MaybeQuitAfterClose();
    return can_close;
}

void CefBrowserEngine::NotifyWindowDestroyed() {
    CEF_REQUIRE_UI_THREAD();
    window_destroyed_ = true;
    MaybeQuitAfterClose();
}

CefBrowserEngine::SurfaceMap::iterator CefBrowserEngine::FindSurface(const core::TabId& tab_id) {
    return surfaces_.find(tab_id);
}

CefBrowserEngine::SurfaceMap::const_iterator CefBrowserEngine::FindSurface(
    const core::TabId& tab_id) const {
    return surfaces_.find(tab_id);
}

CefRefPtr<CefBrowser> CefBrowserEngine::BrowserForCommand(const core::TabId& tab_id) {
    const auto surface = FindSurface(tab_id);
    if (surface == surfaces_.end() || surface->second.closing || !surface->second.view) {
        return nullptr;
    }
    return surface->second.view->GetBrowser();
}

void CefBrowserEngine::HideActiveSurface() {
    if (!active_tab_id_.has_value()) {
        return;
    }

    const auto active = FindSurface(*active_tab_id_);
    if (active != surfaces_.end() && active->second.view) {
        active->second.view->SetVisible(false);
    }
}

void CefBrowserEngine::MaybeQuitAfterClose() {
    if (!window_close_requested_ || !window_destroyed_ || live_browser_count_ != 0 ||
        message_loop_quit_requested_) {
        return;
    }

    message_loop_quit_requested_ = true;
    CefQuitMessageLoop();
}

}  // namespace openbrowser::desktop
