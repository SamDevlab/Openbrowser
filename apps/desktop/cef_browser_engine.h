#pragma once

#include "core/capabilities/capability_policy.h"
#include "core/capabilities/permission_request.h"
#include "core/workspaces/workspace.h"
#include "devtools/network/network_observation_sink.h"
#include "engine/browser_engine.h"

#include "include/cef_base.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_permission_handler.h"
#include "include/cef_request_context.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_panel.h"

#include <atomic>
#include <cstddef>
#include <filesystem>
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
    void GoBack(const core::TabId& tab_id) override;
    void GoForward(const core::TabId& tab_id) override;
    void Reload(const core::TabId& tab_id) override;
    void Suspend(const core::TabId& tab_id) override;
    void Resume(const core::TabId& tab_id) override;

    void SetStorageRoot(std::filesystem::path root);
    [[nodiscard]] const std::filesystem::path& StorageRoot() const noexcept;
    [[nodiscard]] CefRefPtr<CefRequestContext> GetOrCreateRequestContext(
        const std::optional<core::WorkspaceId>& workspace_id);

    void SetNetworkObservationSink(devtools::network::NetworkObservationSink* sink) noexcept;
    [[nodiscard]] bool NetworkObservationEnabled() const noexcept;

    void SetCapabilityPolicy(core::CapabilityPolicy* policy) noexcept;
    [[nodiscard]] const core::CapabilityPolicy* Policy() const noexcept;
    [[nodiscard]] core::CapabilityPolicy* MutablePolicy() const noexcept;

    void AddPermissionPromptObserver(core::PermissionPromptObserver* observer);
    void RemovePermissionPromptObserver(core::PermissionPromptObserver* observer) noexcept;

    void RegisterPermissionPrompt(
        uint64_t prompt_id,
        core::TabId tab_id,
        std::string origin,
        std::vector<core::Capability> capabilities,
        CefRefPtr<CefPermissionPromptCallback> callback);

    void RegisterMediaAccessPrompt(
        core::TabId tab_id,
        std::string origin,
        uint32_t requested_permissions,
        std::vector<core::Capability> capabilities,
        CefRefPtr<CefMediaAccessCallback> callback);

    void DismissPermissionPrompt(uint64_t prompt_id);

    void RespondToPermission(
        uint64_t prompt_id,
        core::PermissionResponse response,
        bool remember_for_origin);

    [[nodiscard]] std::optional<core::PermissionPrompt> FindPromptForTab(const core::TabId& tab_id) const;

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
    [[nodiscard]] CefRefPtr<CefBrowser> BrowserForCommand(const core::TabId& tab_id);
    void HideActiveSurface();
    void MaybeQuitAfterClose();

    struct PendingPrompt {
        core::PermissionPrompt info;
        CefRefPtr<CefPermissionPromptCallback> permission_callback;
        CefRefPtr<CefMediaAccessCallback> media_callback;
        uint32_t requested_media_permissions{0};
    };

    CefRefPtr<CefPanel> browser_host_;
    SurfaceMap surfaces_;
    std::optional<core::TabId> active_tab_id_;
    engine::BrowserEngineEventSink* event_sink_{nullptr};
    std::atomic<devtools::network::NetworkObservationSink*> network_sink_{nullptr};
    std::atomic<core::CapabilityPolicy*> capability_policy_{nullptr};
    std::unordered_map<uint64_t, PendingPrompt> pending_prompts_;
    std::vector<core::PermissionPromptObserver*> prompt_observers_;
    uint64_t next_media_prompt_id_{0x8000000000000000ULL};
    std::size_t live_browser_count_{0};
    bool window_close_requested_{false};
    bool window_destroyed_{false};
    bool message_loop_quit_requested_{false};

    std::filesystem::path storage_root_;
    std::unordered_map<core::WorkspaceId, CefRefPtr<CefRequestContext>> workspace_contexts_;

    IMPLEMENT_REFCOUNTING(CefBrowserEngine);
};

}  // namespace openbrowser::desktop
