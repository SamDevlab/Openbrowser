#pragma once

#include "core/tabs/tab.h"

#include "include/cef_client.h"
#include "include/cef_display_handler.h"
#include "include/cef_download_handler.h"
#include "include/cef_find_handler.h"
#include "include/cef_keyboard_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_permission_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_resource_request_handler.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace openbrowser::desktop {

class CefBrowserEngine;

class CefTabClient final : public CefClient,
                           public CefDisplayHandler,
                           public CefDownloadHandler,
                           public CefFindHandler,
                           public CefKeyboardHandler,
                           public CefLifeSpanHandler,
                           public CefLoadHandler,
                           public CefPermissionHandler,
                           public CefRequestHandler,
                           public CefResourceRequestHandler {
public:
    CefTabClient(core::TabId tab_id, CefRefPtr<CefBrowserEngine> engine);

    CefTabClient(const CefTabClient&) = delete;
    CefTabClient& operator=(const CefTabClient&) = delete;

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override;
    CefRefPtr<CefFindHandler> GetFindHandler() override;
    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override;
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
    CefRefPtr<CefLoadHandler> GetLoadHandler() override;
    CefRefPtr<CefPermissionHandler> GetPermissionHandler() override;
    CefRefPtr<CefRequestHandler> GetRequestHandler() override;

    bool OnPreKeyEvent(
        CefRefPtr<CefBrowser> browser,
        const CefKeyEvent& event,
        CefEventHandle os_event,
        bool* is_keyboard_shortcut) override;

    void OnFindResult(
        CefRefPtr<CefBrowser> browser,
        int identifier,
        int count,
        const CefRect& selection_rect,
        int active_match_ordinal,
        bool final_update) override;

    bool CanDownload(
        CefRefPtr<CefBrowser> browser,
        const CefString& url,
        const CefString& request_method) override;

    bool OnBeforeDownload(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefDownloadItem> download_item,
        const CefString& suggested_name,
        CefRefPtr<CefBeforeDownloadCallback> callback) override;

    void OnDownloadUpdated(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefDownloadItem> download_item,
        CefRefPtr<CefDownloadItemCallback> callback) override;

    bool OnRequestMediaAccessPermission(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& requesting_origin,
        uint32_t requested_permissions,
        CefRefPtr<CefMediaAccessCallback> callback) override;

    bool OnShowPermissionPrompt(
        CefRefPtr<CefBrowser> browser,
        uint64_t prompt_id,
        const CefString& requesting_origin,
        uint32_t requested_permissions,
        CefRefPtr<CefPermissionPromptCallback> callback) override;

    void OnDismissPermissionPrompt(
        CefRefPtr<CefBrowser> browser,
        uint64_t prompt_id,
        cef_permission_request_result_t result) override;

    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    bool OnBeforeBrowse(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool user_gesture,
        bool is_redirect) override;

    void OnLoadStart(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        TransitionType transition_type) override;

    void OnLoadError(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        ErrorCode error_code,
        const CefString& error_text,
        const CefString& failed_url) override;

    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

    void OnRenderProcessTerminated(
        CefRefPtr<CefBrowser> browser,
        TerminationStatus status,
        int error_code,
        const CefString& error_string) override;

    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool is_navigation,
        bool is_download,
        const CefString& request_initiator,
        bool& disable_default_handling) override;

    ReturnValue OnBeforeResourceLoad(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefCallback> callback) override;

    void OnResourceRedirect(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response,
        CefString& new_url) override;

    bool OnResourceResponse(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response) override;

    void OnResourceLoadComplete(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response,
        URLRequestStatus status,
        int64_t received_content_length) override;

private:
    [[nodiscard]] std::uint64_t RequestKey(CefRefPtr<CefRequest> request) const noexcept;
    [[nodiscard]] std::string TraceRequestId(CefRefPtr<CefRequest> request);
    void ForgetTraceRequest(CefRefPtr<CefRequest> request);

    core::TabId tab_id_;
    CefRefPtr<CefBrowserEngine> engine_;

    // Resource callbacks are serialized on CEF's IO thread. These IDs never
    // leave the adapter; Network Lab receives Openbrowser-owned request IDs.
    std::unordered_map<std::uint64_t, std::string> trace_request_ids_;
    std::uint64_t next_trace_request_id_{1};

    IMPLEMENT_REFCOUNTING(CefTabClient);
};

}  // namespace openbrowser::desktop
