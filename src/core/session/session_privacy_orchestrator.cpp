#include "core/session/session_privacy_orchestrator.h"

#include <utility>

namespace openbrowser::core {

SessionPrivacyOrchestrator::SessionPrivacyOrchestrator(
    BrowserSession& session,
    ProfileManager& profile_manager,
    engine::BrowserEngine* engine,
    std::function<void(bool is_private)> on_private_mode_changed)
    : session_(session),
      profile_manager_(profile_manager),
      engine_(engine),
      on_private_mode_changed_(std::move(on_private_mode_changed)) {}

bool SessionPrivacyOrchestrator::IsPrivateModeActive() const noexcept {
    const auto active = profile_manager_.GetActiveProfile();
    return active != nullptr && active->IsEphemeral();
}

bool SessionPrivacyOrchestrator::CanActivateTab(const Tab& tab) const noexcept {
    const bool is_private = IsPrivateModeActive();
    if (is_private) {
        return tab.is_ephemeral;
    }
    return !tab.is_ephemeral;
}

bool SessionPrivacyOrchestrator::CanActivateTab(const TabId& tab_id) const noexcept {
    const auto* tab = session_.FindTab(tab_id);
    if (!tab) {
        return false;
    }
    return CanActivateTab(*tab);
}

bool SessionPrivacyOrchestrator::IsTabVisible(const Tab& tab) const noexcept {
    const bool is_private = IsPrivateModeActive();
    if (is_private) {
        return tab.is_ephemeral;
    }
    return !tab.is_ephemeral;
}

bool SessionPrivacyOrchestrator::ActivateTab(const TabId& tab_id) {
    if (!CanActivateTab(tab_id)) {
        return false;
    }
    return session_.ActivateTab(tab_id);
}

bool SessionPrivacyOrchestrator::CloseActiveTab() {
    const auto& active_id = session_.ActiveTabId();
    if (!active_id.has_value()) {
        return false;
    }
    return CloseTab(*active_id);
}

bool SessionPrivacyOrchestrator::CloseTab(const TabId& tab_id) {
    if (!IsPrivateModeActive()) {
        return session_.CloseTab(tab_id);
    }

    const auto* tab = session_.FindTab(tab_id);
    if (!tab || !tab->is_ephemeral) {
        return false;
    }

    const bool was_active = (session_.ActiveTabId().has_value() && *session_.ActiveTabId() == tab_id);

    // 1. Close ephemeral tab using DoNotActivateFallback so no persistent tab is auto-activated
    if (!session_.CloseTab(tab_id, CloseActivationPolicy::DoNotActivateFallback)) {
        return false;
    }

    // 2. If the closed tab was active, look for another ephemeral tab to activate
    if (was_active) {
        std::optional<TabId> next_ephemeral_id;
        for (const auto& t : session_.Tabs()) {
            if (t.is_ephemeral) {
                next_ephemeral_id = t.id;
                break;
            }
        }

        if (next_ephemeral_id.has_value()) {
            static_cast<void>(session_.ActivateTab(*next_ephemeral_id));
        } else {
            // 3. If no ephemeral tab remains, create a new Private Tab
            Tab priv_tab;
            priv_tab.id = "private-tab-" + std::to_string(++ephemeral_tab_counter_);
            priv_tab.url = "https://example.com/";
            priv_tab.title = "Private Tab";
            priv_tab.lifecycle = TabLifecycle::Active;
            priv_tab.is_ephemeral = true;
            static_cast<void>(session_.OpenTab(std::move(priv_tab), true));
        }
    }

    return true;
}

std::optional<TabId> SessionPrivacyOrchestrator::ReopenClosedTab() {
    const auto mode = IsPrivateModeActive() ? ClosedTabMode::EphemeralOnly : ClosedTabMode::PersistentOnly;
    return session_.ReopenLastClosedTab(mode);
}

bool SessionPrivacyOrchestrator::EnterPrivateMode() {
    auto eph = profile_manager_.CreateEphemeralProfile("Private Session");
    if (!eph) {
        return false;
    }

    profile_manager_.SetActiveProfile(eph->GetId());

    if (engine_) {
        engine_->SetEphemeralMode(true);
    }

    if (on_private_mode_changed_) {
        on_private_mode_changed_(true);
    }

    Tab priv_tab;
    priv_tab.id = "private-tab-" + std::to_string(++ephemeral_tab_counter_);
    priv_tab.url = "https://example.com/";
    priv_tab.title = "Private Tab";
    priv_tab.lifecycle = TabLifecycle::Active;
    priv_tab.is_ephemeral = true;

    return session_.OpenTab(std::move(priv_tab), true);
}

void SessionPrivacyOrchestrator::ExitPrivateMode() {
    // 1. Close all ephemeral tabs with DoNotActivateFallback so no persistent
    //    tab is activated during the private mode transition.
    std::vector<TabId> ephemeral_ids;
    for (const auto& t : session_.Tabs()) {
        if (t.is_ephemeral) {
            ephemeral_ids.push_back(t.id);
        }
    }
    for (const auto& tid : ephemeral_ids) {
        static_cast<void>(session_.CloseTab(tid, CloseActivationPolicy::DoNotActivateFallback));
    }

    // 2. Disable ephemeral engine mode
    if (engine_) {
        engine_->SetEphemeralMode(false);
    }

    // 3. Purge ephemeral RequestContext
    if (engine_) {
        engine_->PurgeEphemeralContext();
    }

    // 4. Restore persistent profile
    profile_manager_.SetActiveProfile("default");
    profile_manager_.PurgeEphemeralProfiles();
    session_.PurgeEphemeralClosedTabs();

    // 5. Notify callback (e.g. obtrace recorder restored)
    if (on_private_mode_changed_) {
        on_private_mode_changed_(false);
    }

    // 6. Only now restore or activate a persistent tab
    if (session_.Tabs().empty()) {
        // All persistent tabs were closed; create a new persistent tab
        Tab def_tab;
        def_tab.id = "tab-" + std::to_string(++persistent_tab_counter_);
        def_tab.url = "https://example.com/";
        def_tab.title = "New Tab";
        def_tab.lifecycle = TabLifecycle::Active;
        def_tab.is_ephemeral = false;
        static_cast<void>(session_.OpenTab(std::move(def_tab), true));
    } else {
        for (const auto& t : session_.Tabs()) {
            if (!t.is_ephemeral) {
                static_cast<void>(session_.ActivateTab(t.id));
                break;
            }
        }
    }
}

void SessionPrivacyOrchestrator::SetPrivateModeChangedCallback(
    std::function<void(bool is_private)> cb) noexcept {
    on_private_mode_changed_ = std::move(cb);
}

} // namespace openbrowser::core
