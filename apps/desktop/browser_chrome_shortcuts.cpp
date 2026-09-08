#include "include/views/cef_textfield.h"

#include "browser_chrome.h"

#include "include/wrapper/cef_helpers.h"

namespace openbrowser::desktop {

void BrowserChrome::RefreshZoomPresentation() {
    CEF_REQUIRE_UI_THREAD();
    UpdateZoomPresentation();
}

}  // namespace openbrowser::desktop
