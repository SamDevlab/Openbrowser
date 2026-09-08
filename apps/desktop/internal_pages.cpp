#include "internal_pages.h"

#include "core/navigation/internal_urls.h"

#include "include/cef_request.h"
#include "include/cef_response.h"
#include "include/cef_scheme.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_stream_resource_handler.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace openbrowser::desktop {

struct NewTabAppearanceState {
    std::mutex mutex;
    NewTabAppearance appearance;
};

namespace {

constexpr char kNewTabHost[] = "newtab.openbrowser.invalid";

constexpr char kNewTabHtmlTemplate[] = R"html(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="color-scheme" content="light dark">
  <title>New Tab</title>
  <style>
    :root {
      color-scheme: dark;
      font-family: "Segoe UI Variable", "Segoe UI", system-ui, sans-serif;
      --aura-bg: #0f1016;
      --aura-surface: rgba(27, 28, 38, 0.78);
      --aura-surface-strong: rgba(18, 19, 27, 0.90);
      --aura-border: rgba(255, 255, 255, 0.10);
      --aura-border-strong: rgba(255, 255, 255, 0.16);
      --aura-text: #f5f5f7;
      --aura-muted: #a8a9b4;
      --aura-soft: #777987;
      --aura-accent: #7567ff;
      --aura-accent-2: #b35cff;
      --aura-blue: #4ec7ff;
    }

    * { box-sizing: border-box; }

    html, body { min-height: 100%; }

    body {
      margin: 0;
      min-height: 100vh;
      overflow: auto;
      background: var(--aura-bg);
      color: var(--aura-text);
    }

    .preference-control {
      position: fixed;
      width: 1px;
      height: 1px;
      opacity: 0;
      pointer-events: none;
    }

    .wallpaper {
      position: fixed;
      inset: 0;
      z-index: -2;
      opacity: 0;
      transition: opacity 160ms ease;
      pointer-events: none;
    }

    .wallpaper::after {
      content: "";
      position: absolute;
      inset: 0;
      background-image:
        linear-gradient(rgba(255,255,255,0.016) 1px, transparent 1px),
        linear-gradient(90deg, rgba(255,255,255,0.016) 1px, transparent 1px);
      background-size: 44px 44px;
      mask-image: linear-gradient(to bottom, rgba(0,0,0,0.48), transparent 78%);
    }

    .wallpaper-aura {
      background:
        radial-gradient(circle at 18% 16%, rgba(117, 103, 255, 0.23), transparent 32%),
        radial-gradient(circle at 82% 78%, rgba(179, 92, 255, 0.14), transparent 30%),
        linear-gradient(145deg, #171923 0%, #111218 58%, #0c0d12 100%);
    }

    .wallpaper-midnight {
      background:
        radial-gradient(circle at 76% 18%, rgba(78, 199, 255, 0.12), transparent 30%),
        linear-gradient(150deg, #111722 0%, #0b0e14 54%, #090a0f 100%);
    }

    .wallpaper-soft {
      background:
        radial-gradient(circle at 22% 22%, rgba(93, 121, 255, 0.17), transparent 34%),
        radial-gradient(circle at 74% 72%, rgba(105, 79, 173, 0.15), transparent 34%),
        linear-gradient(145deg, #1a1d2a 0%, #151621 62%, #101117 100%);
    }

    #wallpaper-aura:checked ~ .wallpaper-aura,
    #wallpaper-midnight:checked ~ .wallpaper-midnight,
    #wallpaper-soft:checked ~ .wallpaper-soft {
      opacity: 1;
    }

    #show-shortcuts:not(:checked) ~ main .shortcuts-section,
    #show-context:not(:checked) ~ main .context-section {
      display: none;
    }

    main {
      width: min(920px, calc(100vw - 48px));
      min-height: 100vh;
      margin: 0 auto;
      padding: clamp(46px, 8vh, 84px) 0 34px;
      display: flex;
      flex-direction: column;
      justify-content: center;
      gap: 24px;
    }

    .hero {
      text-align: center;
    }

    .mark {
      position: relative;
      width: 58px;
      height: 58px;
      margin: 0 auto 18px;
      border-radius: 50%;
      background: conic-gradient(
        from 220deg,
        var(--aura-blue),
        var(--aura-accent),
        var(--aura-accent-2),
        var(--aura-blue)
      );
      box-shadow: 0 14px 34px rgba(117, 103, 255, 0.24);
    }

    .mark::after {
      content: "";
      position: absolute;
      inset: 11px;
      border-radius: 50%;
      background: #171821;
      box-shadow: inset 0 0 0 1px rgba(255,255,255,0.07);
    }

    h1 {
      margin: 0;
      font-size: clamp(32px, 4.5vw, 44px);
      font-weight: 650;
      letter-spacing: -0.045em;
    }

    .tagline {
      margin: 8px 0 0;
      color: var(--aura-muted);
      font-size: 15px;
      line-height: 1.5;
    }

    .search-cue {
      width: min(650px, 100%);
      margin: 26px auto 0;
      min-height: 68px;
      padding: 14px 16px 14px 18px;
      display: flex;
      align-items: center;
      gap: 14px;
      border: 1px solid var(--aura-border-strong);
      border-radius: 18px;
      background: rgba(14, 15, 22, 0.68);
      box-shadow: 0 18px 48px rgba(0, 0, 0, 0.20);
      text-align: left;
    }

    .search-icon {
      width: 36px;
      height: 36px;
      display: grid;
      place-items: center;
      flex: none;
      border-radius: 11px;
      background: rgba(117, 103, 255, 0.12);
      color: #cbc6ff;
      font-size: 18px;
    }

    .search-copy {
      min-width: 0;
      flex: 1;
      display: grid;
      gap: 2px;
    }

    .search-copy strong {
      color: var(--aura-text);
      font-size: 15px;
      font-weight: 590;
    }

    .search-copy span {
      color: var(--aura-muted);
      font-size: 12px;
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

    .content-grid {
      display: grid;
      grid-template-columns: minmax(0, 1.4fr) minmax(240px, 0.8fr);
      gap: 16px;
      align-items: start;
    }

    .card {
      border: 1px solid var(--aura-border);
      border-radius: 18px;
      background: var(--aura-surface);
      box-shadow: 0 18px 48px rgba(0, 0, 0, 0.16);
      backdrop-filter: blur(16px);
    }

    .section-head {
      padding: 16px 18px 12px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
    }

    .section-head h2 {
      margin: 0;
      font-size: 13px;
      font-weight: 600;
      letter-spacing: 0.01em;
    }

    .section-head span {
      color: var(--aura-soft);
      font-size: 11px;
    }

    .shortcuts {
      padding: 4px 14px 16px;
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 10px;
    }

    .shortcut {
      min-width: 0;
      padding: 14px 10px 12px;
      display: grid;
      justify-items: center;
      gap: 8px;
      border: 1px solid transparent;
      border-radius: 14px;
      color: #d9dae1;
      text-decoration: none;
      transition: background 140ms ease, border-color 140ms ease, transform 140ms ease;
    }

    .shortcut:hover,
    .shortcut:focus-visible {
      border-color: var(--aura-border);
      background: rgba(255,255,255,0.055);
      outline: none;
      transform: translateY(-1px);
    }

    .shortcut-badge {
      width: 38px;
      height: 38px;
      display: grid;
      place-items: center;
      border-radius: 12px;
      background: rgba(255,255,255,0.07);
      box-shadow: inset 0 0 0 1px rgba(255,255,255,0.05);
      color: #f3f3f6;
      font-size: 14px;
      font-weight: 650;
    }

    .shortcut span:last-child {
      max-width: 100%;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      font-size: 12px;
    }

    .context-body {
      padding: 3px 16px 16px;
      display: grid;
      gap: 9px;
    }

    .context-row {
      padding: 11px 12px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      border-radius: 12px;
      background: rgba(7, 8, 12, 0.24);
      color: var(--aura-muted);
      font-size: 12px;
    }

    .context-row strong {
      color: #dedee4;
      font-weight: 560;
    }

    .customize {
      width: min(650px, 100%);
      margin: 0 auto;
      border: 1px solid rgba(255,255,255,0.075);
      border-radius: 15px;
      background: rgba(12, 13, 19, 0.44);
      color: var(--aura-muted);
    }

    .customize summary {
      padding: 12px 15px;
      cursor: pointer;
      list-style: none;
      user-select: none;
      color: #b9bac4;
      font-size: 12px;
    }

    .customize summary::-webkit-details-marker { display: none; }

    .customize summary::after {
      content: "+";
      float: right;
      color: #858796;
    }

    .customize[open] summary::after { content: "−"; }

    .customize-body {
      padding: 2px 15px 15px;
      display: grid;
      gap: 14px;
    }

    .customize-group {
      display: grid;
      gap: 8px;
    }

    .customize-title {
      color: #858796;
      font-size: 10px;
      letter-spacing: 0.10em;
      text-transform: uppercase;
    }

    .options {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
    }

    .option {
      padding: 8px 10px;
      display: inline-flex;
      align-items: center;
      gap: 7px;
      border: 1px solid var(--aura-border);
      border-radius: 10px;
      background: rgba(255,255,255,0.035);
      color: #c5c6cf;
      cursor: pointer;
      font-size: 11px;
    }

    .option:hover { background: rgba(255,255,255,0.065); }

    .swatch {
      width: 14px;
      height: 14px;
      border-radius: 50%;
      box-shadow: inset 0 0 0 1px rgba(255,255,255,0.18);
    }

    .swatch-aura { background: linear-gradient(135deg, #7567ff, #b35cff); }
    .swatch-midnight { background: linear-gradient(135deg, #122035, #4ec7ff); }
    .swatch-soft { background: linear-gradient(135deg, #5d79ff, #6950ad); }

    #wallpaper-aura:checked ~ main label[for="wallpaper-aura"],
    #wallpaper-midnight:checked ~ main label[for="wallpaper-midnight"],
    #wallpaper-soft:checked ~ main label[for="wallpaper-soft"],
    #show-shortcuts:checked ~ main label[for="show-shortcuts"],
    #show-context:checked ~ main label[for="show-context"] {
      border-color: rgba(117, 103, 255, 0.52);
      background: rgba(117, 103, 255, 0.11);
      color: #e1deff;
    }

    .preview-note {
      margin: 0;
      color: #6f7180;
      font-size: 10px;
      line-height: 1.45;
    }

    .principles {
      display: flex;
      justify-content: center;
      gap: 10px;
      color: #6f7180;
      font-size: 10px;
      letter-spacing: 0.11em;
      text-transform: uppercase;
    }

    .principles span + span::before {
      content: "•";
      margin-right: 10px;
      color: #4f515d;
    }

    {{APPEARANCE_CSS}}

    @media (max-width: 760px) {
      main {
        width: min(620px, calc(100vw - 32px));
        padding-top: 38px;
      }

      .content-grid { grid-template-columns: 1fr; }
      .shortcuts { grid-template-columns: repeat(4, minmax(70px, 1fr)); }
    }

    @media (max-width: 520px) {
      main {
        width: calc(100vw - 24px);
        padding-top: 28px;
        gap: 16px;
      }

      .mark { width: 50px; height: 50px; margin-bottom: 14px; }
      .mark::after { inset: 10px; }
      .tagline { font-size: 14px; }
      .search-cue { min-height: 62px; border-radius: 15px; }
      .search-copy span { display: none; }
      .shortcuts { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .principles { flex-wrap: wrap; }
    }

    @media (max-height: 660px) and (min-width: 761px) {
      main { padding-top: 28px; padding-bottom: 24px; gap: 15px; }
      .mark { width: 46px; height: 46px; margin-bottom: 12px; }
      .mark::after { inset: 9px; }
      .tagline { display: none; }
      .search-cue { margin-top: 18px; min-height: 60px; }
      .section-head { padding-top: 12px; }
      .shortcuts { padding-bottom: 12px; }
      .principles { display: none; }
    }

    @media (prefers-reduced-motion: reduce) {
      .wallpaper,
      .shortcut {
        transition: none;
      }
    }
  </style>
</head>
<body>
  <input class="preference-control" type="radio" name="wallpaper" id="wallpaper-aura" {{WALLPAPER_AURA_CHECKED}}>
  <input class="preference-control" type="radio" name="wallpaper" id="wallpaper-midnight" {{WALLPAPER_MIDNIGHT_CHECKED}}>
  <input class="preference-control" type="radio" name="wallpaper" id="wallpaper-soft" {{WALLPAPER_SOFT_CHECKED}}>
  <input class="preference-control" type="checkbox" id="show-shortcuts" {{SHOW_SHORTCUTS_CHECKED}}>
  <input class="preference-control" type="checkbox" id="show-context" {{SHOW_CONTEXT_CHECKED}}>

  <div class="wallpaper wallpaper-aura" aria-hidden="true"></div>
  <div class="wallpaper wallpaper-midnight" aria-hidden="true"></div>
  <div class="wallpaper wallpaper-soft" aria-hidden="true"></div>

  <main>
    <header class="hero">
      <div class="mark" aria-hidden="true"></div>
      <h1>Openbrowser</h1>
      <p class="tagline">Your browser, your space.</p>

      <div class="search-cue" role="note" aria-label="Search from the address bar">
        <span class="search-icon" aria-hidden="true">⌕</span>
        <span class="search-copy">
          <strong>Search or enter an address</strong>
          <span>Start typing — the address bar is already focused.</span>
        </span>
        <kbd>Ctrl + L</kbd>
      </div>
    </header>

    <div class="content-grid">
      <section class="card shortcuts-section" aria-label="Quick access">
        <div class="section-head">
          <h2>Quick access</h2>
          <span>Open in this tab</span>
        </div>
        <div class="shortcuts">
          <a class="shortcut" href="https://github.com/" rel="noreferrer" referrerpolicy="no-referrer">
            <span class="shortcut-badge" aria-hidden="true">G</span>
            <span>GitHub</span>
          </a>
          <a class="shortcut" href="https://www.wikipedia.org/" rel="noreferrer" referrerpolicy="no-referrer">
            <span class="shortcut-badge" aria-hidden="true">W</span>
            <span>Wikipedia</span>
          </a>
          <a class="shortcut" href="https://www.youtube.com/" rel="noreferrer" referrerpolicy="no-referrer">
            <span class="shortcut-badge" aria-hidden="true">Y</span>
            <span>YouTube</span>
          </a>
          <a class="shortcut" href="https://github.com/SamDevlab/Openbrowser" rel="noreferrer" referrerpolicy="no-referrer">
            <span class="shortcut-badge" aria-hidden="true">O</span>
            <span>Openbrowser</span>
          </a>
        </div>
      </section>

      <section class="card context-section" aria-label="Browser context">
        <div class="section-head">
          <h2>Keep the web in front</h2>
          <span>Aura</span>
        </div>
        <div class="context-body">
          <div class="context-row">
            <strong>Aura sidebar</strong>
            <kbd>Ctrl + Shift + \</kbd>
          </div>
          <div class="context-row">
            <strong>Command center</strong>
            <kbd>Ctrl + K</kbd>
          </div>
        </div>
      </section>
    </div>

    <details class="customize">
      <summary>Customize this tab</summary>
      <div class="customize-body">
        <div class="customize-group">
          <span class="customize-title">Wallpaper</span>
          <div class="options">
            <label class="option" for="wallpaper-aura"><span class="swatch swatch-aura"></span>Aura</label>
            <label class="option" for="wallpaper-midnight"><span class="swatch swatch-midnight"></span>Midnight</label>
            <label class="option" for="wallpaper-soft"><span class="swatch swatch-soft"></span>Soft</label>
          </div>
        </div>

        <div class="customize-group">
          <span class="customize-title">Modules</span>
          <div class="options">
            <label class="option" for="show-shortcuts">Quick access</label>
            <label class="option" for="show-context">Browser hints</label>
          </div>
        </div>

        <p class="preview-note">Persistent defaults are controlled in Settings · Personalization. Changes here remain local to this tab.</p>
      </div>
    </details>

    <div class="principles" aria-hidden="true">
      <span>Personal</span>
      <span>Focused</span>
      <span>Elegant</span>
    </div>
  </main>
</body>
</html>
)html";

void ReplaceAll(std::string& text, const std::string_view from, const std::string_view to) {
    std::size_t start = 0;
    while ((start = text.find(from, start)) != std::string::npos) {
        text.replace(start, from.size(), to);
        start += to.size();
    }
}

std::string LightThemeCss() {
    return R"css(
      :root {
        color-scheme: light;
        --aura-bg: #f4f5f9;
        --aura-surface: rgba(255, 255, 255, 0.82);
        --aura-surface-strong: rgba(255, 255, 255, 0.94);
        --aura-border: rgba(31, 34, 48, 0.11);
        --aura-border-strong: rgba(31, 34, 48, 0.18);
        --aura-text: #20212a;
        --aura-muted: #686b78;
        --aura-soft: #878a96;
      }
      .wallpaper-aura {
        background:
          radial-gradient(circle at 18% 16%, rgba(117, 103, 255, 0.18), transparent 32%),
          radial-gradient(circle at 82% 78%, rgba(179, 92, 255, 0.10), transparent 30%),
          linear-gradient(145deg, #f8f7ff 0%, #f1f2f8 58%, #e9ebf2 100%);
      }
      .wallpaper-midnight {
        background:
          radial-gradient(circle at 76% 18%, rgba(78, 199, 255, 0.15), transparent 30%),
          linear-gradient(150deg, #eef6ff 0%, #eef1f7 54%, #e7eaf1 100%);
      }
      .wallpaper-soft {
        background:
          radial-gradient(circle at 22% 22%, rgba(93, 121, 255, 0.15), transparent 34%),
          radial-gradient(circle at 74% 72%, rgba(105, 79, 173, 0.12), transparent 34%),
          linear-gradient(145deg, #f6f4ff 0%, #f1f0f8 62%, #ebeaf1 100%);
      }
      .mark::after { background: #f6f5fb; box-shadow: inset 0 0 0 1px rgba(31,34,48,0.08); }
      .search-cue { background: rgba(255,255,255,0.74); box-shadow: 0 18px 48px rgba(50,50,70,0.09); }
      kbd { border-color: rgba(31,34,48,0.13); border-bottom-color: rgba(31,34,48,0.20); background: rgba(31,34,48,0.05); color: #454753; }
      .shortcut { color: #353743; }
      .shortcut:hover, .shortcut:focus-visible { background: rgba(31,34,48,0.045); }
      .shortcut-badge { background: rgba(31,34,48,0.055); box-shadow: inset 0 0 0 1px rgba(31,34,48,0.05); color: #2d2f3a; }
      .context-row { background: rgba(31,34,48,0.04); }
      .context-row strong { color: #343641; }
      .customize { border-color: rgba(31,34,48,0.09); background: rgba(255,255,255,0.52); }
      .customize summary { color: #555865; }
      .option { background: rgba(31,34,48,0.025); color: #555865; }
      .option:hover { background: rgba(31,34,48,0.055); }
    )css";
}

std::string AccentCss(const std::string& accent) {
    std::string primary = "#7567ff";
    std::string secondary = "#b35cff";
    std::string highlight = "#cbc6ff";
    std::string border = "rgba(117, 103, 255, 0.52)";
    std::string fill = "rgba(117, 103, 255, 0.11)";

    if (accent == "blue") {
        primary = "#399cf7";
        secondary = "#4ec7ff";
        highlight = "#b9e6ff";
        border = "rgba(57, 156, 247, 0.52)";
        fill = "rgba(57, 156, 247, 0.11)";
    } else if (accent == "rose") {
        primary = "#f05f98";
        secondary = "#c96cff";
        highlight = "#ffd0e2";
        border = "rgba(240, 95, 152, 0.52)";
        fill = "rgba(240, 95, 152, 0.11)";
    } else if (accent == "green") {
        primary = "#42c996";
        secondary = "#73d66f";
        highlight = "#c7f5df";
        border = "rgba(66, 201, 150, 0.52)";
        fill = "rgba(66, 201, 150, 0.11)";
    }

    return ":root { --aura-accent: " + primary + "; --aura-accent-2: " + secondary + "; }\n"
        ".search-icon { background: " + fill + "; color: " + highlight + "; }\n"
        "#wallpaper-aura:checked ~ main label[for=\"wallpaper-aura\"], "
        "#wallpaper-midnight:checked ~ main label[for=\"wallpaper-midnight\"], "
        "#wallpaper-soft:checked ~ main label[for=\"wallpaper-soft\"], "
        "#show-shortcuts:checked ~ main label[for=\"show-shortcuts\"], "
        "#show-context:checked ~ main label[for=\"show-context\"] { border-color: " + border + "; background: " + fill + "; color: " + highlight + "; }\n";
}

std::string AppearanceCss(const NewTabAppearance& appearance) {
    std::string css = AccentCss(appearance.accent);
    if (appearance.theme == "light") {
        css += LightThemeCss();
    } else if (appearance.theme == "system") {
        css += "@media (prefers-color-scheme: light) {\n" + LightThemeCss() + "\n}\n";
    }
    return css;
}

std::string RenderNewTabHtml(const NewTabAppearance& appearance) {
    std::string html(kNewTabHtmlTemplate);
    ReplaceAll(html, "{{APPEARANCE_CSS}}", AppearanceCss(appearance));
    ReplaceAll(html, "{{WALLPAPER_AURA_CHECKED}}", appearance.wallpaper == "aura" ? "checked" : "");
    ReplaceAll(html, "{{WALLPAPER_MIDNIGHT_CHECKED}}", appearance.wallpaper == "midnight" ? "checked" : "");
    ReplaceAll(html, "{{WALLPAPER_SOFT_CHECKED}}", appearance.wallpaper == "soft" ? "checked" : "");
    ReplaceAll(html, "{{SHOW_SHORTCUTS_CHECKED}}", appearance.show_shortcuts ? "checked" : "");
    ReplaceAll(html, "{{SHOW_CONTEXT_CHECKED}}", appearance.show_context ? "checked" : "");
    return html;
}

std::string AppearanceKey(const NewTabAppearance& appearance) {
    return appearance.theme + "|" + appearance.accent + "|" + appearance.wallpaper + "|" +
        (appearance.show_shortcuts ? "1" : "0") + "|" + (appearance.show_context ? "1" : "0");
}

std::string& CachedHtmlForAppearance(const NewTabAppearance& appearance) {
    static std::mutex cache_mutex;
    static std::map<std::string, std::unique_ptr<std::string>> cache;

    const std::string key = AppearanceKey(appearance);
    std::lock_guard<std::mutex> lock(cache_mutex);
    const auto found = cache.find(key);
    if (found != cache.end()) {
        return *found->second;
    }

    auto html = std::make_unique<std::string>(RenderNewTabHtml(appearance));
    std::string& result = *html;
    cache.emplace(key, std::move(html));
    return result;
}

class NewTabPageFactory final : public CefSchemeHandlerFactory {
public:
    explicit NewTabPageFactory(std::shared_ptr<NewTabAppearanceState> state)
        : state_(std::move(state)) {}

    CefRefPtr<CefResourceHandler> Create(
        CefRefPtr<CefBrowser> /*browser*/,
        CefRefPtr<CefFrame> /*frame*/,
        const CefString& /*scheme_name*/,
        CefRefPtr<CefRequest> request) override {
        if (!request || request->GetURL().ToString() != std::string(core::navigation::kNewTabUrl)) {
            return nullptr;
        }

        NewTabAppearance appearance;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            appearance = state_->appearance;
        }

        std::string& html = CachedHtmlForAppearance(appearance);
        auto stream = CefStreamReader::CreateForData(html.data(), html.size());
        if (!stream) {
            return nullptr;
        }

        CefResponse::HeaderMap headers;
        headers.emplace("Cache-Control", "no-store");
        headers.emplace(
            "Content-Security-Policy",
            "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'");
        headers.emplace("X-Content-Type-Options", "nosniff");
        headers.emplace("Referrer-Policy", "no-referrer");

        return new CefStreamResourceHandler(
            200,
            "OK",
            "text/html",
            std::move(headers),
            stream);
    }

private:
    std::shared_ptr<NewTabAppearanceState> state_;

    IMPLEMENT_REFCOUNTING(NewTabPageFactory);
};

CefRefPtr<CefSchemeHandlerFactory> CreateNewTabFactory(
    const std::shared_ptr<NewTabAppearanceState>& state) {
    return new NewTabPageFactory(state);
}

}  // namespace

InternalPageRegistry::InternalPageRegistry()
    : appearance_state_(std::make_shared<NewTabAppearanceState>()) {
    registered_ = CefRegisterSchemeHandlerFactory(
        "https",
        kNewTabHost,
        CreateNewTabFactory(appearance_state_));
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
        CreateNewTabFactory(appearance_state_));
}

void InternalPageRegistry::SetNewTabAppearance(NewTabAppearance appearance) {
    std::lock_guard<std::mutex> lock(appearance_state_->mutex);
    appearance_state_->appearance = std::move(appearance);
}

}  // namespace openbrowser::desktop
