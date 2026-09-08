#include "cef_tab_client.h"

#include "cef_browser_engine.h"

#include "include/wrapper/cef_helpers.h"

namespace openbrowser::desktop {

CefRefPtr<CefFindHandler> CefTabClient::GetFindHandler() {
    return this;
}

void CefTabClient::OnFindResult(
    CefRefPtr<CefBrowser> /*browser*/,
    const int /*identifier*/,
    const int count,
    const CefRect& /*selection_rect*/,
    const int active_match_ordinal,
    const bool final_update) {
    CEF_REQUIRE_UI_THREAD();
    if (engine_) {
        engine_->NotifyFindResult(tab_id_, count, active_match_ordinal, final_update);
    }
}

}  // namespace openbrowser::desktop
