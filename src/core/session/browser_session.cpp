#include "core/session/browser_session.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <utility>

namespace openbrowser::core {

BrowserSession::BrowserSession(engine::BrowserEngine& engine) noexcept : engine_(engine) {
    engine_.SetEventSink(this);
}

BrowserSession::~BrowserSession() {
    observers_.clear();
    engine_.SetEventSink(nullptr);
}

void BrowserSession::AddObserver(BrowserSessionObserver* observer) {
    if (observer == nullptr || std::find(observers_.begin(), observers_.end(), observer) != observers_.end()) {
        return;
    }
    observers_.push_back(observer);
}

void BrowserSession::RemoveObserver(BrowserSessionObserver* observer) noexcept {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
}

bool BrowserSession::OpenTab(Tab tab, const bool activate) {
    if (tab.id.empty() || tab.url.empty() || FindTab(tab.id) != nullptr) {
        return false;
    }

    const bool should_activate = tabs_.empty() || activate;
    if (should_activate) {
        DemoteActiveTab();
        tab.lifecycle = TabLifecycle::Active;
        tab.last_activated_sequence = ++last_activated_counter_;
        active_tab_id_ = tab.id;
    } else {
        tab.lifecycle = TabLifecycle::Background;
    }

    tab.pending_url = tab.url;
    tab.navigation_state = NavigationState::Requested;
    tab.last_error.reset();
    tab.renderer_crashed = false;
    tab.has_committed_navigation = false;

    tabs_.push_back(std::move(tab));
    engine_.CreateTab(tabs_.back());

    if (should_activate) {
        engine_.ActivateTab(*active_tab_id_);
    }

    NotifyObservers();
    return true;
}

bool BrowserSession::CloseTab(
    const TabId& tab_id,
    const CloseActivationPolicy activation_policy) {
    const auto it = FindMutable(tab_id);
    if (it == tabs_.end()) {
        return false;
    }

    if (!it->url.empty() && it->url != "about:blank") {
        closed_tabs_.push_back(ClosedTabRecord{
            .url = it->url,
            .title = it->title,
            .workspace_id = it->workspace_id,
            .is_ephemeral = it->is_ephemeral,
        });
        if (closed_tabs_.size() > 25) {
            closed_tabs_.erase(closed_tabs_.begin());
        }
    }

    const bool was_active = active_tab_id_.has_value() && *active_tab_id_ == tab_id;
    const auto closed_index = static_cast<std::size_t>(std::distance(tabs_.begin(), it));

    engine_.CloseTab(tab_id);
    tabs_.erase(it);

    if (was_active) {
        active_tab_id_.reset();
        if (activation_policy == CloseActivationPolicy::ActivateFallback) {
            ActivateAfterClose(closed_index);
        }
    }

    NotifyObservers();
    return true;
}

std::optional<TabId> BrowserSession::ReopenLastClosedTab(const ClosedTabMode mode) {
    if (closed_tabs_.empty()) {
        return std::nullopt;
    }

    for (auto it = closed_tabs_.rbegin(); it != closed_tabs_.rend(); ++it) {
        if (mode == ClosedTabMode::PersistentOnly && it->is_ephemeral) {
            continue;
        }
        if (mode == ClosedTabMode::EphemeralOnly && !it->is_ephemeral) {
            continue;
        }

        ClosedTabRecord record = *it;
        closed_tabs_.erase(std::next(it).base());

        Tab restored_tab;
        restored_tab.id = "restored-" + std::to_string(++restored_counter_);
        restored_tab.url = record.url;
        restored_tab.title = record.title.empty() ? record.url : record.title;
        restored_tab.workspace_id = record.workspace_id;
        restored_tab.is_ephemeral = record.is_ephemeral;
        restored_tab.lifecycle = TabLifecycle::Active;

        const std::string restored_id = restored_tab.id;
        if (OpenTab(std::move(restored_tab), true)) {
            return restored_id;
        }
        return std::nullopt;
    }

    return std::nullopt;
}

const std::vector<ClosedTabRecord>& BrowserSession::ClosedTabs() const noexcept {
    return closed_tabs_;
}

void BrowserSession::PurgeEphemeralClosedTabs() {
    std::erase_if(closed_tabs_, [](const ClosedTabRecord& record) {
        return record.is_ephemeral;
    });
}

bool BrowserSession::CycleTab(
    const bool forward,
    const std::function<bool(const Tab&)>& filter) {
    if (tabs_.empty()) {
        return false;
    }

    std::vector<std::size_t> visible_indices;
    visible_indices.reserve(tabs_.size());
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        if (!filter || filter(tabs_[i])) {
            visible_indices.push_back(i);
        }
    }

    if (visible_indices.empty()) {
        return false;
    }

    if (visible_indices.size() == 1) {
        const auto target_id = tabs_[visible_indices[0]].id;
        return ActivateTab(target_id);
    }

    std::size_t current_pos = 0;
    bool found_active = false;
    if (active_tab_id_.has_value()) {
        for (std::size_t i = 0; i < visible_indices.size(); ++i) {
            if (tabs_[visible_indices[i]].id == *active_tab_id_) {
                current_pos = i;
                found_active = true;
                break;
            }
        }
    }

    std::size_t next_pos = 0;
    if (!found_active) {
        next_pos = 0;
    } else if (forward) {
        next_pos = (current_pos + 1) % visible_indices.size();
    } else {
        next_pos = (current_pos + visible_indices.size() - 1) % visible_indices.size();
    }

    const auto target_id = tabs_[visible_indices[next_pos]].id;
    return ActivateTab(target_id);
}

bool BrowserSession::ActivateTab(const TabId& tab_id) {
    const auto it = FindMutable(tab_id);
    if (it == tabs_.end()) {
        return false;
    }

    if (active_tab_id_.has_value() && *active_tab_id_ == tab_id) {
        return true;
    }

    if (it->lifecycle == TabLifecycle::Discarded) {
        engine_.CreateTab(*it);
        it->lifecycle = TabLifecycle::Active;
        it->last_activated_sequence = ++last_activated_counter_;
        DemoteActiveTab();
        active_tab_id_ = tab_id;
        engine_.ActivateTab(tab_id);
        NotifyObservers();
        return true;
    }

    if (it->lifecycle == TabLifecycle::Suspended) {
        engine_.Resume(tab_id);
    }

    DemoteActiveTab();
    it->lifecycle = TabLifecycle::Active;
    it->last_activated_sequence = ++last_activated_counter_;
    active_tab_id_ = tab_id;
    engine_.ActivateTab(tab_id);
    NotifyObservers();
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
    NotifyObservers();
    return true;
}

bool BrowserSession::GoBack(const TabId& tab_id) {
    if (!PrepareTabForNavigationCommand(tab_id)) {
        return false;
    }
    engine_.GoBack(tab_id);
    NotifyObservers();
    return true;
}

bool BrowserSession::GoForward(const TabId& tab_id) {
    if (!PrepareTabForNavigationCommand(tab_id)) {
        return false;
    }
    engine_.GoForward(tab_id);
    NotifyObservers();
    return true;
}

bool BrowserSession::Reload(const TabId& tab_id) {
    if (!PrepareTabForNavigationCommand(tab_id)) {
        return false;
    }
    engine_.Reload(tab_id);
    NotifyObservers();
    return true;
}

bool BrowserSession::SuspendTab(const TabId& tab_id) {
    const auto it = FindMutable(tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded || it->lifecycle == TabLifecycle::Active) {
        return false;
    }
    if (it->lifecycle == TabLifecycle::Suspended) {
        return true;
    }

    engine_.Suspend(tab_id);
    it->lifecycle = TabLifecycle::Suspended;
    NotifyObservers();
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
    NotifyObservers();
    return true;
}

bool BrowserSession::DiscardTab(const TabId& tab_id) {
    const auto it = FindMutable(tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded || it->lifecycle == TabLifecycle::Active) {
        return false;
    }

    engine_.CloseTab(tab_id);
    it->lifecycle = TabLifecycle::Discarded;
    NotifyObservers();
    return true;
}

const Tab* BrowserSession::FindTab(const TabId& tab_id) const {
    const auto it = std::find_if(tabs_.cbegin(), tabs_.cend(), [&tab_id](const Tab& tab) { return tab.id == tab_id; });
    return it == tabs_.cend() ? nullptr : &(*it);
}

const std::vector<Tab>& BrowserSession::Tabs() const noexcept { return tabs_; }
const std::optional<TabId>& BrowserSession::ActiveTabId() const noexcept { return active_tab_id_; }

void BrowserSession::OnNavigationStarted(const engine::NavigationStartedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) return;
    it->pending_url = event.url;
    it->navigation_state = NavigationState::Loading;
    it->last_error.reset();
    it->renderer_crashed = false;
    NotifyObservers();
}

void BrowserSession::OnNavigationCommitted(const engine::NavigationCommittedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) return;
    it->url = event.url;
    it->pending_url.reset();
    it->navigation_state = NavigationState::Idle;
    it->last_error.reset();
    it->renderer_crashed = false;
    it->has_committed_navigation = true;
    NotifyObservers();
}

void BrowserSession::OnNavigationFailed(const engine::NavigationFailedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) return;
    it->pending_url = event.url;
    it->navigation_state = NavigationState::Failed;
    it->last_error = event.error_text.empty() ? std::string{"navigation failed"} : event.error_text;
    it->renderer_crashed = false;
    NotifyObservers();
}

void BrowserSession::OnTitleChanged(const engine::TitleChangedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) return;
    it->title = event.title;
    NotifyObservers();
}

void BrowserSession::OnRendererCrashed(const engine::RendererCrashedEvent& event) {
    const auto it = FindMutable(event.tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) return;
    it->renderer_crashed = true;
    it->navigation_state = NavigationState::Failed;
    it->last_error = event.reason.empty() ? std::string{"renderer crashed"} : event.reason;
    NotifyObservers();
}

BrowserSession::TabIterator BrowserSession::FindMutable(const TabId& tab_id) {
    return std::find_if(tabs_.begin(), tabs_.end(), [&tab_id](const Tab& tab) { return tab.id == tab_id; });
}

bool BrowserSession::PrepareTabForNavigationCommand(const TabId& tab_id) {
    const auto it = FindMutable(tab_id);
    if (it == tabs_.end() || it->lifecycle == TabLifecycle::Discarded) return false;
    if (it->lifecycle == TabLifecycle::Suspended) {
        engine_.Resume(tab_id);
        it->lifecycle = TabLifecycle::Background;
    }
    return true;
}

void BrowserSession::NotifyObservers() {
    const auto snapshot = observers_;
    for (auto* observer : snapshot) {
        if (observer != nullptr && std::find(observers_.begin(), observers_.end(), observer) != observers_.end()) {
            observer->OnBrowserSessionChanged(*this);
        }
    }
}

void BrowserSession::DemoteActiveTab() {
    if (!active_tab_id_.has_value()) return;
    const auto active = FindMutable(*active_tab_id_);
    if (active != tabs_.end() && active->lifecycle == TabLifecycle::Active) active->lifecycle = TabLifecycle::Background;
}

void BrowserSession::ActivateAfterClose(const std::size_t preferred_index) {
    if (tabs_.empty()) return;
    const auto start_index = std::min(preferred_index, tabs_.size() - 1);
    for (std::size_t offset = 0; offset < tabs_.size(); ++offset) {
        const auto index = (start_index + tabs_.size() - offset) % tabs_.size();
        auto& candidate = tabs_[index];
        if (candidate.lifecycle == TabLifecycle::Discarded) continue;
        if (candidate.lifecycle == TabLifecycle::Suspended) engine_.Resume(candidate.id);
        candidate.lifecycle = TabLifecycle::Active;
        candidate.last_activated_sequence = ++last_activated_counter_;
        active_tab_id_ = candidate.id;
        engine_.ActivateTab(candidate.id);
        return;
    }
    auto& fallback = tabs_[start_index];
    engine_.CreateTab(fallback);
    fallback.lifecycle = TabLifecycle::Active;
    fallback.last_activated_sequence = ++last_activated_counter_;
    active_tab_id_ = fallback.id;
    engine_.ActivateTab(fallback.id);
}

}  // namespace openbrowser::core
