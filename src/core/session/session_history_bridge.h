#pragma once

#include "core/history/history_manager.h"
#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/session/browser_session_observer.h"

#include <functional>
#include <unordered_map>
#include <utility>

namespace openbrowser::core {

// Orchestrates history recording and session persistence in reaction to
// BrowserSession state changes, enforcing privacy boundaries and correct visit semantics:
//
// 1. If active profile is ephemeral: NO history is recorded, NO session state is persisted.
// 2. If active tab is ephemeral: NO history is recorded, NO session state is persisted.
// 3. Visit semantics:
//    - Persistent profile + persistent tab
//    - Navigation state == NavigationState::Idle (committed navigation)
//    - Committed URL is valid, not empty, and not "about:blank"
//    - Committed URL changed since last history commit for that tab
// 4. On committed visit: record visit and trigger history auto-save.
// 5. Title changes, tab activations, reload notifications, suspend/resume DO NOT count as new visits.
class SessionHistoryBridge final : public BrowserSessionObserver {
public:
    explicit SessionHistoryBridge(
        HistoryManager* history_manager,
        const ProfileManager* profile_manager,
        std::function<void(bool is_clean)> save_session_fn = nullptr)
        : history_manager_(history_manager),
        profile_manager_(profile_manager),
        save_session_fn_(std::move(save_session_fn)) {}

    ~SessionHistoryBridge() override = default;

    SessionHistoryBridge(const SessionHistoryBridge&) = delete;
    SessionHistoryBridge& operator=(const SessionHistoryBridge&) = delete;

    void OnBrowserSessionChanged(const BrowserSession& session) override {
        const bool is_profile_private = (profile_manager_ &&
                                         profile_manager_->GetActiveProfile() &&
                                         profile_manager_->GetActiveProfile()->IsEphemeral());

        if (session.ActiveTabId().has_value()) {
            const auto* active_tab = session.FindTab(*session.ActiveTabId());
            if (active_tab) {
                const bool is_tab_private = active_tab->is_ephemeral;
                if (!is_profile_private && !is_tab_private) {
                    // Check if URL is valid, not empty, and not "about:blank"
                    if (!active_tab->url.empty() && active_tab->url != "about:blank") {
                        // Do not record visit if navigation has explicitly failed
                        if (active_tab->navigation_state != NavigationState::Failed) {
                            const auto& tab_id = active_tab->id;
                            const auto it = last_committed_url_per_tab_.find(tab_id);
                            const bool url_changed = (it == last_committed_url_per_tab_.end() || it->second != active_tab->url);

                            if (url_changed) {
                                last_committed_url_per_tab_[tab_id] = active_tab->url;
                                if (history_manager_) {
                                    history_manager_->RecordVisit(active_tab->url, active_tab->title, active_tab->workspace_id);
                                }
                            }
                        }
                    }

                    if (save_session_fn_) {
                        save_session_fn_(false);
                    }
                }
            }
        }
    }

    void ForgetTab(const TabId& tab_id) {
        last_committed_url_per_tab_.erase(tab_id);
    }

private:
    HistoryManager* history_manager_{nullptr};
    const ProfileManager* profile_manager_{nullptr};
    std::function<void(bool is_clean)> save_session_fn_;
    std::unordered_map<TabId, std::string> last_committed_url_per_tab_;
};

} // namespace openbrowser::core
