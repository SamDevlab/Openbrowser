#pragma once

#include "core/tabs/tab.h"

#include <string>

namespace openbrowser::engine {

struct NavigationStartedEvent {
    core::TabId tab_id;
    std::string url;
};

struct NavigationCommittedEvent {
    core::TabId tab_id;
    std::string url;
};

struct NavigationFailedEvent {
    core::TabId tab_id;
    std::string url;
    int error_code{0};
    std::string error_text;
};

struct TitleChangedEvent {
    core::TabId tab_id;
    std::string title;
};

struct RendererCrashedEvent {
    core::TabId tab_id;
    std::string reason;
};

class BrowserEngineEventSink {
public:
    virtual ~BrowserEngineEventSink() = default;

    virtual void OnNavigationStarted(const NavigationStartedEvent& event) = 0;
    virtual void OnNavigationCommitted(const NavigationCommittedEvent& event) = 0;
    virtual void OnNavigationFailed(const NavigationFailedEvent& event) = 0;
    virtual void OnTitleChanged(const TitleChangedEvent& event) = 0;
    virtual void OnRendererCrashed(const RendererCrashedEvent& event) = 0;
};

}  // namespace openbrowser::engine
