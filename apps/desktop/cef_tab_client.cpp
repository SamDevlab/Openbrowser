#include "cef_tab_client.h"

#include "cef_browser_engine.h"
#include "devtools/network/network_trace.h"

#include "include/wrapper/cef_helpers.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace openbrowser::desktop {
namespace {

template <typename HeaderMap>
std::vector<devtools::network::Header> ConvertHeaders(const HeaderMap& source) {
    std::vector<devtools::network::Header> headers;
    headers.reserve(source.size());
    for (const auto& [name, value] : source) {
        headers.push_back({.name = name.ToString(), .value = value.ToString()});
    }
    return headers;
}

std::vector<devtools::network::Header> RequestHeaders(CefRefPtr<CefRequest> request) {
    CefRequest::HeaderMap headers;
    request->GetHeaderMap(headers);
    return ConvertHeaders(headers);
}

std::vector<devtools::network::Header> ResponseHeaders(CefRefPtr<CefResponse> response) {
    CefResponse::HeaderMap headers;
    response->GetHeaderMap(headers);
    return ConvertHeaders(headers);
}

std::string RequestBodyPreview(CefRefPtr<CefRequest> request) {
    if (!request) {
        return {};
    }
    auto post_data = request->GetPostData();
    if (!post_data) {
        return {};
    }
    CefPostData::ElementVector elements;
    post_data->GetElements(elements);
    std::string preview;
    for (const auto& el : elements) {
        if (!el || el->GetType() != PDE_TYPE_BYTES) {
            continue;
        }
        const std::size_t bytes = el->GetBytesCount();
        if (bytes == 0) {
            continue;
        }
        const std::size_t read_bytes = std::min(bytes, static_cast<std::size_t>(512));
        std::vector<char> buf(read_bytes);
        el->GetBytes(read_bytes, buf.data());
        preview.append(buf.data(), read_bytes);
        if (preview.size() >= 512) {
            break;
        }
    }
    return preview;
}

std::size_t ToTransferredBytes(const int64_t length) noexcept {
    if (length <= 0) {
        return 0;
    }

    const auto unsigned_length = static_cast<std::uint64_t>(length);
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
    return static_cast<std::size_t>(std::min(unsigned_length, maximum));
}

std::string TerminationReason(
    const CefRequestHandler::TerminationStatus status,
    const int error_code,
    const CefString& error_string) {
    std::string reason = "renderer terminated (status=" + std::to_string(static_cast<int>(status));
    reason += ", error_code=" + std::to_string(error_code) + ")";
    if (!error_string.empty()) {
        reason += ": " + error_string.ToString();
    }
    return reason;
}

}  // namespace

CefTabClient::CefTabClient(core::TabId tab_id, CefRefPtr<CefBrowserEngine> engine)
    : tab_id_(std::move(tab_id)), engine_(std::move(engine)) {}

CefRefPtr<CefDisplayHandler> CefTabClient::GetDisplayHandler() {
    return this;
}

CefRefPtr<CefLifeSpanHandler> CefTabClient::GetLifeSpanHandler() {
    return this;
}

CefRefPtr<CefLoadHandler> CefTabClient::GetLoadHandler() {
    return this;
}

CefRefPtr<CefPermissionHandler> CefTabClient::GetPermissionHandler() {
    return this;
}

CefRefPtr<CefRequestHandler> CefTabClient::GetRequestHandler() {
    return this;
}

bool CefTabClient::OnRequestMediaAccessPermission(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    const CefString& requesting_origin,
    const uint32_t requested_permissions,
    CefRefPtr<CefMediaAccessCallback> callback) {
    CEF_REQUIRE_UI_THREAD();

    const std::string origin = requesting_origin.ToString();
    const auto* policy = engine_->Policy();

    std::vector<core::Capability> caps;
    if ((requested_permissions & CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE) != 0) {
        caps.push_back(core::Capability::Microphone);
    }
    if ((requested_permissions & CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE) != 0) {
        caps.push_back(core::Capability::Camera);
    }

    if (caps.empty() || policy == nullptr) {
        if (callback) {
            callback->Cancel();
        }
        return true;
    }

    bool all_allow = true;
    bool any_deny = false;
    for (const auto cap : caps) {
        const auto decision = policy->Resolve(cap, {.origin = origin});
        if (decision == core::CapabilityDecision::Deny) {
            any_deny = true;
            all_allow = false;
            break;
        }
        if (decision != core::CapabilityDecision::Allow) {
            all_allow = false;
        }
    }

    if (any_deny) {
        if (callback) {
            callback->Cancel();
        }
        return true;
    }

    if (all_allow) {
        if (callback) {
            callback->Continue(requested_permissions);
        }
        return true;
    }

    engine_->RegisterMediaAccessPrompt(
        tab_id_,
        origin,
        requested_permissions,
        std::move(caps),
        callback);
    return true;
}

bool CefTabClient::OnShowPermissionPrompt(
    CefRefPtr<CefBrowser> /*browser*/,
    const uint64_t prompt_id,
    const CefString& requesting_origin,
    const uint32_t requested_permissions,
    CefRefPtr<CefPermissionPromptCallback> callback) {
    CEF_REQUIRE_UI_THREAD();

    const std::string origin = requesting_origin.ToString();
    const auto* policy = engine_->Policy();

    std::vector<core::Capability> caps;
    if ((requested_permissions & CEF_PERMISSION_TYPE_CAMERA_STREAM) != 0) {
        caps.push_back(core::Capability::Camera);
    }
    if ((requested_permissions & CEF_PERMISSION_TYPE_MIC_STREAM) != 0) {
        caps.push_back(core::Capability::Microphone);
    }
    if ((requested_permissions & CEF_PERMISSION_TYPE_GEOLOCATION) != 0) {
        caps.push_back(core::Capability::Geolocation);
    }
    if ((requested_permissions & CEF_PERMISSION_TYPE_NOTIFICATIONS) != 0) {
        caps.push_back(core::Capability::Notifications);
    }
    if ((requested_permissions & CEF_PERMISSION_TYPE_CLIPBOARD) != 0) {
        caps.push_back(core::Capability::ClipboardRead);
    }

    if (caps.empty() || policy == nullptr) {
        if (callback) {
            callback->Continue(CEF_PERMISSION_RESULT_DENY);
        }
        return true;
    }

    bool all_allow = true;
    bool any_deny = false;
    for (const auto cap : caps) {
        const auto decision = policy->Resolve(cap, {.origin = origin});
        if (decision == core::CapabilityDecision::Deny) {
            any_deny = true;
            all_allow = false;
            break;
        }
        if (decision != core::CapabilityDecision::Allow) {
            all_allow = false;
        }
    }

    if (any_deny) {
        if (callback) {
            callback->Continue(CEF_PERMISSION_RESULT_DENY);
        }
        return true;
    }

    if (all_allow) {
        if (callback) {
            callback->Continue(CEF_PERMISSION_RESULT_ACCEPT);
        }
        return true;
    }

    engine_->RegisterPermissionPrompt(
        prompt_id,
        tab_id_,
        origin,
        std::move(caps),
        callback);
    return true;
}

void CefTabClient::OnDismissPermissionPrompt(
    CefRefPtr<CefBrowser> /*browser*/,
    const uint64_t prompt_id,
    const cef_permission_request_result_t /*result*/) {
    CEF_REQUIRE_UI_THREAD();
    engine_->DismissPermissionPrompt(prompt_id);
}

void CefTabClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    engine_->NotifyBrowserCreated(tab_id_, browser);
}

void CefTabClient::OnBeforeClose(CefRefPtr<CefBrowser> /*browser*/) {
    CEF_REQUIRE_UI_THREAD();
    engine_->NotifyBrowserBeforeClose(tab_id_);
}

bool CefTabClient::OnBeforeBrowse(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    const bool /*user_gesture*/,
    const bool /*is_redirect*/) {
    CEF_REQUIRE_UI_THREAD();

    const std::string url = request ? request->GetURL().ToString() : std::string{};
    const auto* policy = engine_->Policy();
    if (policy != nullptr && !policy->CanNavigate(url)) {
        if (frame && frame->IsMain()) {
            engine_->NotifyNavigationFailed(
                tab_id_,
                url,
                -102,
                "Blocked by Openbrowser capability policy");
        }
        return true;
    }

    if (frame && frame->IsMain()) {
        engine_->NotifyNavigationStarted(tab_id_, url);
    }
    return false;
}

void CefTabClient::OnLoadStart(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> frame,
    const TransitionType /*transition_type*/) {
    CEF_REQUIRE_UI_THREAD();
    if (frame && frame->IsMain()) {
        engine_->NotifyNavigationCommitted(tab_id_, frame->GetURL().ToString());
    }
}

void CefTabClient::OnLoadError(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> frame,
    const ErrorCode error_code,
    const CefString& error_text,
    const CefString& failed_url) {
    CEF_REQUIRE_UI_THREAD();
    if (frame && frame->IsMain()) {
        engine_->NotifyNavigationFailed(
            tab_id_,
            failed_url.ToString(),
            static_cast<int>(error_code),
            error_text.ToString());
    }
}

void CefTabClient::OnTitleChange(CefRefPtr<CefBrowser> /*browser*/, const CefString& title) {
    CEF_REQUIRE_UI_THREAD();
    engine_->NotifyTitleChanged(tab_id_, title.ToString());
}

void CefTabClient::OnRenderProcessTerminated(
    CefRefPtr<CefBrowser> /*browser*/,
    const TerminationStatus status,
    const int error_code,
    const CefString& error_string) {
    CEF_REQUIRE_UI_THREAD();
    engine_->NotifyRendererCrashed(tab_id_, TerminationReason(status, error_code, error_string));
}

CefRefPtr<CefResourceRequestHandler> CefTabClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    CefRefPtr<CefRequest> /*request*/,
    const bool /*is_navigation*/,
    const bool /*is_download*/,
    const CefString& /*request_initiator*/,
    bool& disable_default_handling) {
    CEF_REQUIRE_IO_THREAD();
    disable_default_handling = false;
    if (!engine_->NetworkObservationEnabled() && engine_->Policy() == nullptr) {
        return nullptr;
    }
    return this;
}

CefResourceRequestHandler::ReturnValue CefTabClient::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> /*callback*/) {
    CEF_REQUIRE_IO_THREAD();

    const std::string url = request ? request->GetURL().ToString() : std::string{};
    const std::string initiator = request && request->GetReferrerURL().length() > 0
        ? request->GetReferrerURL().ToString()
        : std::string{};

    const auto* policy = engine_->Policy();
    if (policy != nullptr && !policy->CanLoadResource(url, initiator)) {
        const std::string request_id = TraceRequestId(request);
        const std::string method = request ? request->GetMethod().ToString() : "GET";
        engine_->PostNetworkEvent({
            .request_id = request_id,
            .tab_id = tab_id_,
            .type = devtools::network::NetworkEventType::RequestStarted,
            .url = url,
            .method = method,
            .headers = request ? RequestHeaders(request) : std::vector<devtools::network::Header>{},
        });
        engine_->PostNetworkEvent({
            .request_id = request_id,
            .tab_id = tab_id_,
            .type = devtools::network::NetworkEventType::RequestFailed,
            .url = url,
            .method = method,
            .error = "BLOCKED_BY_CAPABILITY_POLICY",
        });
        ForgetTraceRequest(request);
        return RV_CANCEL;
    }

    engine_->PostNetworkEvent({
        .request_id = TraceRequestId(request),
        .tab_id = tab_id_,
        .type = devtools::network::NetworkEventType::RequestStarted,
        .url = url,
        .method = request ? request->GetMethod().ToString() : "GET",
        .headers = request ? RequestHeaders(request) : std::vector<devtools::network::Header>{},
        .body_preview = RequestBodyPreview(request),
    });

    return RV_CONTINUE;
}

void CefTabClient::OnResourceRedirect(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response,
    CefString& new_url) {
    CEF_REQUIRE_IO_THREAD();

    engine_->PostNetworkEvent({
        .request_id = TraceRequestId(request),
        .tab_id = tab_id_,
        .type = devtools::network::NetworkEventType::Redirect,
        .url = new_url.ToString(),
        .method = request->GetMethod().ToString(),
        .status = response ? std::optional<int>{response->GetStatus()} : std::nullopt,
        .headers = response ? ResponseHeaders(response) : std::vector<devtools::network::Header>{},
    });
}

bool CefTabClient::OnResourceResponse(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response) {
    CEF_REQUIRE_IO_THREAD();

    engine_->PostNetworkEvent({
        .request_id = TraceRequestId(request),
        .tab_id = tab_id_,
        .type = devtools::network::NetworkEventType::ResponseReceived,
        .url = request->GetURL().ToString(),
        .method = request->GetMethod().ToString(),
        .status = response ? std::optional<int>{response->GetStatus()} : std::nullopt,
        .headers = response ? ResponseHeaders(response) : std::vector<devtools::network::Header>{},
    });

    return false;
}

void CefTabClient::OnResourceLoadComplete(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response,
    const URLRequestStatus status,
    const int64_t received_content_length) {
    CEF_REQUIRE_IO_THREAD();

    const bool succeeded = status == UR_SUCCESS;
    engine_->PostNetworkEvent({
        .request_id = TraceRequestId(request),
        .tab_id = tab_id_,
        .type = succeeded ? devtools::network::NetworkEventType::RequestFinished
                          : devtools::network::NetworkEventType::RequestFailed,
        .url = request->GetURL().ToString(),
        .method = request->GetMethod().ToString(),
        .status = response ? std::optional<int>{response->GetStatus()} : std::nullopt,
        .transferred_bytes = ToTransferredBytes(received_content_length),
        .error = succeeded ? std::string{}
                           : "cef_urlrequest_status=" + std::to_string(static_cast<int>(status)),
    });

    ForgetTraceRequest(request);
}

std::uint64_t CefTabClient::RequestKey(CefRefPtr<CefRequest> request) const noexcept {
    const auto identifier = request->GetIdentifier();
    if (identifier != 0) {
        return identifier;
    }

    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(request.get()));
}

std::string CefTabClient::TraceRequestId(CefRefPtr<CefRequest> request) {
    const auto key = RequestKey(request);
    const auto existing = trace_request_ids_.find(key);
    if (existing != trace_request_ids_.end()) {
        return existing->second;
    }

    auto id = tab_id_ + ":request-" + std::to_string(next_trace_request_id_++);
    trace_request_ids_.emplace(key, id);
    return id;
}

void CefTabClient::ForgetTraceRequest(CefRefPtr<CefRequest> request) {
    trace_request_ids_.erase(RequestKey(request));
}

}  // namespace openbrowser::desktop
