#pragma once

#include "core/navigation/internal_urls.h"

#include <string>

namespace openbrowser::core {

struct BrowserSettings {
    std::string search_provider_name{"DuckDuckGo"};
    std::string search_url_template{"https://duckduckgo.com/?q=%s"};
    bool restore_session_on_startup{true};
    std::string home_page_url{navigation::kNewTabUrl};
    std::string downloads_directory;

    // Project Aura personalization. String-backed values keep settings.json
    // forward-compatible while the visual system is still evolving.
    std::string appearance_theme{"dark"};       // dark | light | system
    std::string appearance_accent{"violet"};    // violet | blue | rose | green
    std::string aura_sidebar_state{"compact"};  // compact | expanded | hidden
    std::string new_tab_wallpaper{"aura"};      // aura | midnight | soft
    bool new_tab_show_shortcuts{true};
    bool new_tab_show_context{true};
};

}  // namespace openbrowser::core
