#include "cef_browser_engine.h"

#include "cef_tab_client.h"
#include "devtools/network/network_trace.h"

#include "include/cef_app.h"
#include "include/cef_frame.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_helpers.h"

#include <filesystem>
#include <functional>
#include <system_error>
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

class ActionTask final : public CefTask {
public:
    ActionTask(
        CefRefPtr<CefBrowserEngine> engine,
        std::string action_id)
        : engine_(std::move(engine)), action_id_(std::move(action_id)) {}

    void Execute() override {
        engine_->PostAction(action_id_);
    }

private:
    CefRefPtr<CefBrowserEngine> engine_;
    std::string action_id_;

    IMPLEMENT_REFCOUNTING(ActionTask);
};

}  // namespace

CefBrowserEngine::CefBrowserEngine(CefRefPtr<CefPanel> browser_host)
    : browser_host_(std::move(browser_host)) {}

void CefBrowserEngine::SetEventSink(engine::BrowserEngineEventSink* sink) noexcept {
    event_sink_ = sink;
}

void CefBrowserEngine::SetActionDispatcher(std::function<void(const std::string&)> dispatcher) {
    action_dispatcher_ = std::move(dispatcher);
}

void CefBrowserEngine::PostAction(const std::string& action_id) {
    if (!action_dispatcher_) {
        return;
    }
    if (CefCurrentlyOn(TID_UI)) {
        action_dispatcher_(action_id);
        return;
    }
    CefPostTask(TID_UI, new ActionTask(this, action_id));
}

void CefBrowserEngine::SetStorageRoot(std::filesystem::path root) {
    storage_root_ = std::move(root);
}

const std::filesystem::path& CefBrowserEngine::StorageRoot() const noexcept {
    return storage_root_;
}

void CefBrowserEngine::SetEphemeralMode(const bool enabled) noexcept {
    is_ephemeral_mode_.store(enabled, std::memory_order_release);
}

bool CefBrowserEngine::IsEphemeralMode() const noexcept {
    return is_ephemeral_mode_.load(std::memory_order_acquire);
}

void CefBrowserEngine::PurgeEphemeralContext() {
    CEF_REQUIRE_UI_THREAD();
    ephemeral_context_ = nullptr;
}

CefRefPtr<CefRequestContext> CefBrowserEngine::GetOrCreateRequestContext(
    const std::optional<core::WorkspaceId>& workspace_id,
    const bool is_ephemeral) {
    CEF_REQUIRE_UI_THREAD();

    if (is_ephemeral || IsEphemeralMode()) {
        if (!ephemeral_context_) {
            CefRequestContextSettings settings{};
            // Empty cache_path forces an isolated, non-persistent in-memory context.
            settings.persist_session_cookies = 0;
            ephemeral_context_ = CefRequestContext::CreateContext(settings, nullptr);
        }
        return ephemeral_context_;
    }

    if (!workspace_id.has_value() || workspace_id->empty() || *workspace_id == "default") {
        return nullptr;
    }

    const auto it = workspace_contexts_.find(*workspace_id);
    if (it != workspace_contexts_.end() && it->second) {
        return it->second;
    }

    CefRequestContextSettings settings{};
    if (!storage_root_.empty()) {
        const auto partition_path = storage_root_ / "workspaces" / *workspace_id;
        std::error_code ec;
        std::filesystem::create_directories(partition_path, ec);
        CefString(&settings.cache_path).FromString(partition_path.string());
        settings.persist_session_cookies = 1;
    }

    CefRefPtr<CefRequestContext> context = CefRequestContext::CreateContext(settings, nullptr);
    workspace_contexts_[*workspace_id] = context;
    return context;
}

void CefBrowserEngine::CreateTab(const core::Tab& tab) {
    CEF_REQUIRE_UI_THREAD();

    if (!browser_host_ || tab.id.empty() || tab.url.empty() || FindSurface(tab.id) != surfaces_.end()) {
        return;
    }

    CefRefPtr<CefBrowserEngine> self(this);
    CefRefPtr<CefTabClient> client(new CefTabClient(tab.id, self));
    CefBrowserSettings browser_settings;
    CefRefPtr<CefRequestContext> request_context = GetOrCreateRequestContext(
        tab.workspace_id, tab.is_ephemeral);
    CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
        client,
        tab.url,
        browser_settings,
        nullptr,
        request_context,
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

    std::vector<uint64_t> prompts_to_dismiss;
    for (const auto& [id, prompt] : pending_prompts_) {
        if (prompt.info.tab_id == tab_id) {
            prompts_to_dismiss.push_back(id);
        }
    }
    for (const auto id : prompts_to_dismiss) {
        DismissPermissionPrompt(id);
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
    core::CapabilityPolicy* policy) noexcept {
    capability_policy_.store(policy, std::memory_order_release);
}

const core::CapabilityPolicy* CefBrowserEngine::Policy() const noexcept {
    return capability_policy_.load(std::memory_order_acquire);
}

core::CapabilityPolicy* CefBrowserEngine::MutablePolicy() const noexcept {
    return capability_policy_.load(std::memory_order_acquire);
}

void CefBrowserEngine::SetTransferBroker(core::TransferBroker* broker) noexcept {
    transfer_broker_.store(broker, std::memory_order_release);
}

core::TransferBroker* CefBrowserEngine::TransferBroker() const noexcept {
    return transfer_broker_.load(std::memory_order_acquire);
}

void CefBrowserEngine::SetContentFilter(core::ContentFilter* filter) noexcept {
    content_filter_.store(filter, std::memory_order_release);
}

core::ContentFilter* CefBrowserEngine::ContentFilter() const noexcept {
    return content_filter_.load(std::memory_order_acquire);
}

void CefBrowserEngine::SetConnectionRegistry(devtools::network::ConnectionRegistry* registry) noexcept {
    connection_registry_.store(registry, std::memory_order_release);
}

devtools::network::ConnectionRegistry* CefBrowserEngine::ConnectionRegistry() const noexcept {
    return connection_registry_.load(std::memory_order_acquire);
}

void CefBrowserEngine::SetUserAgentPolicyEngine(core::UserAgentPolicyEngine* ua_engine) noexcept {
    ua_engine_.store(ua_engine, std::memory_order_release);
}

const core::UserAgentPolicyEngine* CefBrowserEngine::UserAgentEngine() const noexcept {
    return ua_engine_.load(std::memory_order_acquire);
}

void CefBrowserEngine::SetMitigationRegistry(core::CompatibilityMitigationRegistry* mitigations) noexcept {
    mitigation_registry_.store(mitigations, std::memory_order_release);
}

const core::CompatibilityMitigationRegistry* CefBrowserEngine::MitigationRegistry() const noexcept {
    return mitigation_registry_.load(std::memory_order_acquire);
}

void CefBrowserEngine::SetDecisionLog(core::FilterDecisionLog* log) noexcept {
    filter_decision_log_.store(log, std::memory_order_release);
}

core::FilterDecisionLog* CefBrowserEngine::DecisionLog() const noexcept {
    return filter_decision_log_.load(std::memory_order_acquire);
}

void CefBrowserEngine::AddPermissionPromptObserver(
    core::PermissionPromptObserver* observer) {
    CEF_REQUIRE_UI_THREAD();
    if (observer != nullptr &&
        std::find(prompt_observers_.begin(), prompt_observers_.end(), observer) == prompt_observers_.end()) {
        prompt_observers_.push_back(observer);
    }
}

void CefBrowserEngine::RemovePermissionPromptObserver(
    core::PermissionPromptObserver* observer) noexcept {
    CEF_REQUIRE_UI_THREAD();
    const auto it = std::find(prompt_observers_.begin(), prompt_observers_.end(), observer);
    if (it != prompt_observers_.end()) {
        prompt_observers_.erase(it);
    }
}

void CefBrowserEngine::RegisterPermissionPrompt(
    const uint64_t prompt_id,
    core::TabId tab_id,
    std::string origin,
    std::vector<core::Capability> capabilities,
    CefRefPtr<CefPermissionPromptCallback> callback) {
    CEF_REQUIRE_UI_THREAD();

    core::PermissionPrompt prompt_info{
        .prompt_id = prompt_id,
        .tab_id = std::move(tab_id),
        .origin = std::move(origin),
        .capabilities = std::move(capabilities),
    };

    pending_prompts_[prompt_id] = {
        .info = prompt_info,
        .permission_callback = std::move(callback),
        .media_callback = nullptr,
        .requested_media_permissions = 0,
    };

    for (auto* obs : prompt_observers_) {
        if (obs != nullptr) {
            obs->OnPermissionPromptRequested(prompt_info);
        }
    }
}

void CefBrowserEngine::RegisterMediaAccessPrompt(
    core::TabId tab_id,
    std::string origin,
    const uint32_t requested_permissions,
    std::vector<core::Capability> capabilities,
    CefRefPtr<CefMediaAccessCallback> callback) {
    CEF_REQUIRE_UI_THREAD();

    const uint64_t prompt_id = ++next_media_prompt_id_;

    core::PermissionPrompt prompt_info{
        .prompt_id = prompt_id,
        .tab_id = std::move(tab_id),
        .origin = std::move(origin),
        .capabilities = std::move(capabilities),
    };

    pending_prompts_[prompt_id] = {
        .info = prompt_info,
        .permission_callback = nullptr,
        .media_callback = std::move(callback),
        .requested_media_permissions = requested_permissions,
    };

    for (auto* obs : prompt_observers_) {
        if (obs != nullptr) {
            obs->OnPermissionPromptRequested(prompt_info);
        }
    }
}

void CefBrowserEngine::DismissPermissionPrompt(const uint64_t prompt_id) {
    CEF_REQUIRE_UI_THREAD();
    const auto it = pending_prompts_.find(prompt_id);
    if (it == pending_prompts_.end()) {
        return;
    }
    pending_prompts_.erase(it);
    for (auto* obs : prompt_observers_) {
        if (obs != nullptr) {
            obs->OnPermissionPromptDismissed(prompt_id);
        }
    }
}

void CefBrowserEngine::RespondToPermission(
    const uint64_t prompt_id,
    const core::PermissionResponse response,
    const bool remember_for_origin) {
    CEF_REQUIRE_UI_THREAD();

    const auto it = pending_prompts_.find(prompt_id);
    if (it == pending_prompts_.end()) {
        return;
    }

    auto prompt = std::move(it->second);
    pending_prompts_.erase(it);

    if (remember_for_origin) {
        auto* policy = MutablePolicy();
        if (policy != nullptr) {
            const auto decision = (response == core::PermissionResponse::Allow)
                ? core::CapabilityDecision::Allow
                : core::CapabilityDecision::Deny;
            for (const auto cap : prompt.info.capabilities) {
                policy->SetOrigin(prompt.info.origin, cap, decision);
            }
        }
    }

    if (prompt.permission_callback) {
        cef_permission_request_result_t result = CEF_PERMISSION_RESULT_DISMISS;
        if (response == core::PermissionResponse::Allow) {
            result = CEF_PERMISSION_RESULT_ACCEPT;
        } else if (response == core::PermissionResponse::Block) {
            result = CEF_PERMISSION_RESULT_DENY;
        }
        prompt.permission_callback->Continue(result);
    } else if (prompt.media_callback) {
        if (response == core::PermissionResponse::Allow) {
            prompt.media_callback->Continue(prompt.requested_media_permissions);
        } else {
            prompt.media_callback->Cancel();
        }
    }

    for (auto* obs : prompt_observers_) {
        if (obs != nullptr) {
            obs->OnPermissionPromptDismissed(prompt_id);
        }
    }
}

std::optional<core::PermissionPrompt> CefBrowserEngine::FindPromptForTab(
    const core::TabId& tab_id) const {
    for (const auto& [id, prompt] : pending_prompts_) {
        if (prompt.info.tab_id == tab_id) {
            return prompt.info;
        }
    }
    return std::nullopt;
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

        // Once the top-level CefWindow is destroyed its Views hierarchy must no
        // longer be mutated. OnBeforeClose can arrive after OnWindowDestroyed on
        // Windows, so only detach/layout while the window still owns the panel.
        if (!window_destroyed_ && browser_host_ && surface->second.view) {
            browser_host_->RemoveChildView(surface->second.view);
        }
        surfaces_.erase(surface);

        if (active_tab_id_.has_value() && *active_tab_id_ == tab_id) {
            active_tab_id_.reset();
        }

        if (!window_destroyed_ && browser_host_) {
            browser_host_->Layout();
        }
    }

    // CEF's lifecycle contract allows the app message loop to exit after the
    // final OnBeforeClose callback. Do not wait for a second Views ordering
    // condition once all browser objects are gone.
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

    bool can_close = true;

    for (auto& [tab_id, surface] : surfaces_) {
        CefRefPtr<CefBrowser> browser = surface.view->GetBrowser();
        if (!browser) {
            continue;
        }

        if (!browser->GetHost()->TryCloseBrowser()) {
            can_close = false;
        }
    }

    return can_close;
}

void CefBrowserEngine::NotifyWindowDestroyed() {
    CEF_REQUIRE_UI_THREAD();
    window_destroyed_ = true;

    // The top-level Views hierarchy no longer exists after this callback. Drop
    // the engine's panel reference so later OnBeforeClose callbacks cannot
    // accidentally mutate a destroyed CefView tree.
    browser_host_ = nullptr;
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
    if (!window_close_requested_ || live_browser_count_ != 0 ||
        message_loop_quit_requested_) {
        return;
    }

    message_loop_quit_requested_ = true;
    CefQuitMessageLoop();
}

}  // namespace openbrowser::desktop
