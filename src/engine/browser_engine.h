#pragma once

#include "core/tabs/tab.h"
#include "engine/browser_engine_events.h"

#include <string>

namespace openbrowser::engine {

struct NavigationRequest {
    core::TabId tab_id;
    std::string url;
};

class BrowserEngine {
public:
    virtual ~BrowserEngine() = default;

    virtual void SetEventSink(BrowserEngineEventSink* sink) noexcept = 0;
    virtual void CreateTab(const core::Tab& tab) = 0;
    virtual void CloseTab(const core::TabId& tab_id) = 0;
    virtual void ActivateTab(const core::TabId& tab_id) = 0;
    virtual void Navigate(const NavigationRequest& request) = 0;
    virtual void Suspend(const core::TabId& tab_id) = 0;
    virtual void Resume(const core::TabId& tab_id) = 0;
};

}  // namespace openbrowser::engine
