#include "internal_pages.h"

#include "core/navigation/internal_urls.h"

#include "include/cef_request.h"
#include "include/cef_response.h"
#include "include/cef_scheme.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_stream_resource_handler.h"

#include <string>
#include <utility>

namespace openbrowser::desktop {
namespace {

constexpr char kNewTabHost[] = "newtab.openbrowser.invalid";

// The backing memory must outlive every CefStreamReader created from it.
char kNewTabHtml[] = R"html(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>New Tab</title>
  <style>
    :root { color-scheme: dark; font-family: "Segoe UI", system-ui, sans-serif; }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      background: #1f1f1f;
      color: #f3f3f3;
    }
    main {
      width: min(560px, calc(100vw - 48px));
      text-align: center;
      transform: translateY(-5vh);
    }
    .mark {
      width: 58px;
      height: 58px;
      margin: 0 auto 22px;
      display: grid;
      place-items: center;
      border: 1px solid #505050;
      border-radius: 50%;
      background: #292929;
      font-size: 24px;
      font-weight: 650;
    }
    h1 { margin: 0; font-size: 30px; font-weight: 600; letter-spacing: -0.02em; }
    p { margin: 12px 0 0; color: #b8b8b8; font-size: 15px; line-height: 1.55; }
    kbd {
      display: inline-block;
      margin-top: 20px;
      padding: 5px 9px;
      border: 1px solid #515151;
      border-bottom-color: #666;
      border-radius: 6px;
      background: #282828;
      color: #d9d9d9;
      font: 12px "Segoe UI", system-ui, sans-serif;
    }
  </style>
</head>
<body>
  <main>
    <div class="mark" aria-hidden="true">O</div>
    <h1>Openbrowser</h1>
    <p>Search or enter an address in the bar above.</p>
    <kbd>Ctrl + L</kbd>
  </main>
</body>
</html>
)html";

class NewTabPageFactory final : public CefSchemeHandlerFactory {
public:
    CefRefPtr<CefResourceHandler> Create(
        CefRefPtr<CefBrowser> /*browser*/,
        CefRefPtr<CefFrame> /*frame*/,
        const CefString& /*scheme_name*/,
        CefRefPtr<CefRequest> request) override {
        if (!request || request->GetURL().ToString() != std::string(core::navigation::kNewTabUrl)) {
            return nullptr;
        }

        auto stream = CefStreamReader::CreateForData(kNewTabHtml, sizeof(kNewTabHtml) - 1);
        if (!stream) {
            return nullptr;
        }

        CefResponse::HeaderMap headers;
        headers.emplace("Cache-Control", "no-store");
        headers.emplace(
            "Content-Security-Policy",
            "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'");
        headers.emplace("X-Content-Type-Options", "nosniff");

        return new CefStreamResourceHandler(
            200,
            "OK",
            "text/html; charset=utf-8",
            std::move(headers),
            stream);
    }

private:
    IMPLEMENT_REFCOUNTING(NewTabPageFactory);
};

CefRefPtr<CefSchemeHandlerFactory> CreateNewTabFactory() {
    return new NewTabPageFactory();
}

}  // namespace

InternalPageRegistry::InternalPageRegistry() {
    registered_ = CefRegisterSchemeHandlerFactory(
        "https",
        kNewTabHost,
        CreateNewTabFactory());
}

InternalPageRegistry::~InternalPageRegistry() {
    if (registered_) {
        static_cast<void>(CefRegisterSchemeHandlerFactory("https", kNewTabHost, nullptr));
    }
}

bool InternalPageRegistry::RegisterForContext(CefRefPtr<CefRequestContext> context) const {
    if (!context) {
        return false;
    }

    return context->RegisterSchemeHandlerFactory(
        "https",
        kNewTabHost,
        CreateNewTabFactory());
}

}  // namespace openbrowser::desktop
