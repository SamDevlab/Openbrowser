#include "core/session/browser_session.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace openbrowser::core {

BrowserSession::BrowserSession(engine::BrowserEngine& engine) noexcept : engine_(engine) {
    engine_.SetEventSink(this);
}

BrowserSession::~BrowserSession() {
    engine_.SetEventSink(nullptr);
}

bool BrowserSession::OpenTab(Tab tab, const bool activate) {
    if (tab.id.empty() || tab.url.empty() || FindTab(tab.id) != nullptr) {
        return false;
    }

    const bool should_activate = tabs_.empty() || activate;
    if (should_activate) {
        DemoteActiveTab();
        tab.lifecycle = TabLifecycle::Active;
        active_tab_id_ = tab.id;
    } else {
        tab.lifecycle = TabLifecycle::Background;
    }

    tab.pending_url = tab.url;
    tab.navigation_state = NavigationState::Requested;
    tab.last_error.reset();
    tab.renderer_crashed = false;

    // Register domain state before crossing the engine boundary. An adapter is
    // allowed to synchronously emit lifecycle/navigation callbacks from
    // CreateTab(), and those callbacks must already be able to resolve TabId.
    tabs_.push_back(std::move(tab));
    engine_.CreateTab(tabs_.back());

    if (should_activate) {
        engine_.ActivateTab(*active_tab_id_);
    }

    return true;
}

bool BrowserSession::CloseTab(const TabId& tab_id) {
    const auto it = FindMutable(tab_id);
    if (it == tabs_.end()) {
        return false;
    }

    const bool was_active = active_tab_id_.has_value() && *active_tab_id_ == tab_id;
    const auto closed_index = static_cast<std::size_t>(std::distance(tabs_.begin(), it));

    engine_.CloseTab(tab_id);
    tabs_.erase(it);

    if (was_active) {
        active_tab_id_.reset();
        ActivateAfterClose(closed_index);
    }

    return true;
}

bool BrowserSession::ActivateTab(const TabId& tab_id) {
    const auto it = FindMutable(tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) {
        return false;
    }

    if (active_tab_id_.has_value() && *active_tab_id_ == tab_id) {
        return true;
    }

    if (it->lifecycle == TabLifecycle::Suspended) {
        engine_.Resume(tab_id);
    }

    DemoteActiveTab();
    it->lifecycle = TabLifecycle::Active;
    active_tab_id_ = tab_id;
    engine_.ActivateTab(tab_id);
    return true;
}

bool BrowserSession::Navigate(const TabId& tab_id, std::string url) {
    if (url.empty()) {
        return false;
    }

    const auto it = FindMutable(tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) {
        return false;
    }

    if (it->lifecycle == TabLifecycle::Suspended) {
        engine_.Resume(tab_id);
        it->lifecycle = TabLifecycle::Background;
    }

    it->pending_url = url;
    it->navigation_state = NavigationState::Requested;
    it->last_error.reset();
    it->renderer_crashed = false;
    engine_.Navigate(engine::NavigationRequest{.tab_id = tab_id, .url = std::move(url)});
    return true;
}

bool BrowserSession::SuspendTab(const TabId& tab_id) {
    const auto it = FindMutable(tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) {
        return false;
    }

    if (it->lifecycle == TabLifecycle::Active) {
        return false;
    }

    if (it->lifecycle == TabLifecycle::Suspended) {
        return true;
    }

    engine_.Suspend(tab_id);
    it->lifecycle = TabLifecycle::Suspended;
    return true;
}

bool BrowserSession::ResumeTab(const TabId& tab_id) {
    const auto it = FindMutable(tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) {
        return false;
    }

    if (it->lifecycle != TabLifecycle::Suspended) {
        return true;
    }

    engine_.Resume(tab_id);
    it->lifecycle = TabLifecycle::Background;
    return true;
}

const Tab* BrowserSession::FindTab(const TabId& tab_id) const {
    const auto it = std::find_if(tabs_.cbegin(), tabs_.cend(), [&tab_id](const Tab& tab) {
        return tab.id == tab_id;
    });

    return it == tabs_.cend() ? nullptr : &(*it);
}

const std::vector<Tab>& BrowserSession::Tabs() const noexcept {
    return tabs_;
}

const std::optional<TabId>& BrowserSession::ActiveTabId() const noexcept {
    return active_tab_id_;
}

void BrowserSession::OnNavigationStarted(const engine::NavigationStartedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) {
        return;
    }

    it->pending_url = event.url;
    it->navigation_state = NavigationState::Loading;
    it->last_error.reset();
    it->renderer_crashed = false;
}

void BrowserSession::OnNavigationCommitted(const engine::NavigationCommittedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) {
        return;
    }

    it->url = event.url;
    it->pending_url.reset();
    it->navigation_state = NavigationState::Idle;
    it->last_error.reset();
    it->renderer_crashed = false;
}

void BrowserSession::OnNavigationFailed(const engine::NavigationFailedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) {
        return;
    }

    it->pending_url = event.url;
    it->navigation_state = NavigationState::Failed;
    it->last_error = event.error_text.empty() ? std::string{"navigation failed"} : event.error_text;
    it->renderer_crashed = false;
}

void BrowserSession::OnTitleChanged(const engine::TitleChangedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) {
        return;
    }

    it->title = event.title;
}

void BrowserSession::OnRendererCrashed(const engine::RendererCrashedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) {
        return;
    }

    it->renderer_crashed = true;
    it->navigation_state = NavigationState::Failed;
    it->last_error = event.reason.empty() ? std::string{"renderer crashed"} : event.reason;
}

BrowserSession::TabIterator BrowserSession::FindMutable(const TabId& tab_id) {
    return std::find_if(tabs_.begin(), tabs_.end(), [&tab_id](const Tab& tab) {
        return tab.id == tab_id;
    });
}

void BrowserSession::DemoteActiveTab() {
    if (!active_tab_id_.has_value()) {
        return;
    }

    const auto active = FindMutable(*active_tab_id_);
    if (active != tabs_.end() && active->lifecycle == TabLifecycle::Active) {
        active->lifecycle = TabLifecycle::Background;
    }
}

void BrowserSession::ActivateAfterClose(const std::size_t preferred_index) {
    if (tabs_.empty()) {
        return;
    }

    const auto start_index = std::min(preferred_index, tabs_.size() - 1);
    for (std::size_t offset = 0; offset < tabs_.size(); ++offset) {
        const auto index = (start_index + tabs_.size() - offset) % tabs_.size();
        auto& candidate = tabs_[index];
        if (candidate.lifecycle == TabLifecycle::Discarded) {
            continue;
        }

        if (candidate.lifecycle == TabLifecycle::Suspended) {
            engine_.Resume(candidate.id);
        }

        candidate.lifecycle = TabLifecycle::Active;
        active_tab_id_ = candidate.id;
        engine_.ActivateTab(candidate.id);
        return;
    }
}

}  // namespace openbrowser::core
