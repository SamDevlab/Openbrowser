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
  <meta name="color-scheme" content="dark">
  <title>New Tab</title>
  <style>
    :root {
      color-scheme: dark;
      font-family: "Segoe UI Variable", "Segoe UI", system-ui, sans-serif;
      --aura-bg: #111218;
      --aura-surface: rgba(31, 32, 43, 0.72);
      --aura-border: rgba(255, 255, 255, 0.10);
      --aura-text: #f5f5f7;
      --aura-muted: #a6a7b2;
      --aura-accent: #7567ff;
      --aura-accent-2: #b35cff;
    }

    * { box-sizing: border-box; }

    html, body { min-height: 100%; }

    body {
      margin: 0;
      min-height: 100vh;
      overflow: hidden;
      display: grid;
      place-items: center;
      background:
        radial-gradient(circle at 18% 18%, rgba(117, 103, 255, 0.20), transparent 33%),
        radial-gradient(circle at 82% 76%, rgba(179, 92, 255, 0.12), transparent 31%),
        linear-gradient(145deg, #151722 0%, var(--aura-bg) 58%, #0d0e13 100%);
      color: var(--aura-text);
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      pointer-events: none;
      background-image:
        linear-gradient(rgba(255,255,255,0.018) 1px, transparent 1px),
        linear-gradient(90deg, rgba(255,255,255,0.018) 1px, transparent 1px);
      background-size: 42px 42px;
      mask-image: linear-gradient(to bottom, rgba(0,0,0,0.44), transparent 76%);
    }

    main {
      position: relative;
      width: min(720px, calc(100vw - 48px));
      padding: 44px 48px 40px;
      text-align: center;
      border: 1px solid var(--aura-border);
      border-radius: 24px;
      background: var(--aura-surface);
      box-shadow: 0 28px 80px rgba(0, 0, 0, 0.28);
      backdrop-filter: blur(18px);
      transform: translateY(-3vh);
    }

    .mark {
      position: relative;
      width: 72px;
      height: 72px;
      margin: 0 auto 24px;
      border-radius: 50%;
      background: conic-gradient(
        from 220deg,
        #4ec7ff,
        var(--aura-accent),
        var(--aura-accent-2),
        #4ec7ff
      );
      box-shadow: 0 14px 36px rgba(117, 103, 255, 0.26);
    }

    .mark::after {
      content: "";
      position: absolute;
      inset: 13px;
      border-radius: 50%;
      background: #171821;
      box-shadow: inset 0 0 0 1px rgba(255,255,255,0.07);
    }

    h1 {
      margin: 0;
      font-size: clamp(34px, 5vw, 48px);
      font-weight: 650;
      letter-spacing: -0.045em;
    }

    .tagline {
      margin: 10px 0 0;
      color: var(--aura-muted);
      font-size: 16px;
      line-height: 1.55;
    }

    .hint {
      width: min(470px, 100%);
      margin: 30px auto 0;
      padding: 14px 16px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 18px;
      border: 1px solid var(--aura-border);
      border-radius: 14px;
      background: rgba(8, 9, 14, 0.38);
      color: #c9cad2;
      font-size: 14px;
      text-align: left;
    }

    .hint strong {
      color: var(--aura-text);
      font-weight: 550;
    }

    kbd {
      flex: none;
      padding: 5px 9px;
      border: 1px solid rgba(255,255,255,0.13);
      border-bottom-color: rgba(255,255,255,0.22);
      border-radius: 7px;
      background: rgba(255,255,255,0.06);
      color: #dfe0e6;
      font: 12px "Segoe UI Variable", "Segoe UI", system-ui, sans-serif;
      white-space: nowrap;
    }

    .principles {
      margin-top: 26px;
      display: flex;
      justify-content: center;
      gap: 10px;
      color: #81838f;
      font-size: 11px;
      letter-spacing: 0.12em;
      text-transform: uppercase;
    }

    .principles span + span::before {
      content: "•";
      margin-right: 10px;
      color: #555765;
    }

    @media (max-width: 560px) {
      main { padding: 36px 24px 32px; border-radius: 20px; }
      .hint { align-items: flex-start; }
      .principles { flex-wrap: wrap; }
    }
  </style>
</head>
<body>
  <main>
    <div class="mark" aria-hidden="true"></div>
    <h1>Openbrowser</h1>
    <p class="tagline">Your browser, your space.</p>

    <div class="hint" aria-label="Address bar keyboard hint">
      <span><strong>Search or enter an address</strong> in the address bar above.</span>
      <kbd>Ctrl + L</kbd>
    </div>

    <div class="principles" aria-hidden="true">
      <span>Personal</span>
      <span>Focused</span>
      <span>Elegant</span>
    </div>
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

        // CefResponse::SetMimeType expects only the media type. Supplying a
        // charset parameter here can cause Chromium to treat the custom
        // response as plain text. UTF-8 is declared by the document itself.
        return new CefStreamResourceHandler(
            200,
            "OK",
            "text/html",
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
