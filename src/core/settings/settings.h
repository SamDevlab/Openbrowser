#pragma once

#include <string>

namespace openbrowser::core {

struct BrowserSettings {
    std::string search_provider_name{"DuckDuckGo"};
    std::string search_url_template{"https://duckduckgo.com/?q=%s"};
    bool restore_session_on_startup{true};
    std::string home_page_url{"about:blank"};
    std::string downloads_directory;
};

}  // namespace openbrowser::core
