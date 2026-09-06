#pragma once

namespace openbrowser::core {

class BrowserSession;

// Browser UI projections observe the session instead of owning duplicate tab or
// navigation state. Observers are non-owning and must unregister before they
// are destroyed.
class BrowserSessionObserver {
public:
    virtual ~BrowserSessionObserver() = default;

    virtual void OnBrowserSessionChanged(const BrowserSession& session) = 0;
};

}  // namespace openbrowser::core
