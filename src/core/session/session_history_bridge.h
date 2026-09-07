#pragma once

#include "core/history/history_manager.h"
#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/session/browser_session_observer.h"

#include <functional>
#include <utility>

namespace openbrowser::core {

// Orchestrates history recording and session persistence in reaction to
// BrowserSession state changes, enforcing privacy boundaries:
//
// 1. If the active profile is ephemeral, NO history is recorded and NO session
//    state is persisted.
// 2. If the active tab is ephemeral (is_ephemeral = true), NO history is
//    recorded and NO session state is persisted.
// 3. Only when BOTH active profile and active tab are persistent are visits
//    recorded and session snapshots triggered.
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
                    if (history_manager_ && !active_tab->url.empty() && active_tab->url != "about:blank") {
                        history_manager_->RecordVisit(active_tab->url, active_tab->title);
                    }
                    if (save_session_fn_) {
                        save_session_fn_(false);
                    }
                }
            }
        }
    }

private:
    HistoryManager* history_manager_{nullptr};
    const ProfileManager* profile_manager_{nullptr};
    std::function<void(bool is_clean)> save_session_fn_;
};

} // namespace openbrowser::core
