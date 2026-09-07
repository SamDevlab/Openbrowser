#pragma once

#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/tabs/tab.h"
#include "engine/browser_engine.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace openbrowser::core {

// Orchestrates Private Mode lifecycle transitions, tab visibility, and programmatic
// activation guarding across BrowserSession, ProfileManager, and BrowserEngine.
//
// Guaranteed invariants:
// 1. In Private Mode, only ephemeral tabs (is_ephemeral == true) are visible in TabStrip.
// 2. In Private Mode, activating a persistent tab is programmatically REJECTED.
// 3. Normal tabs are preserved in background when entering Private Mode.
// 4. Exiting Private Mode follows the strict sequence:
//    a) Close all ephemeral tabs.
//    b) engine->SetEphemeralMode(false).
//    c) engine->PurgeEphemeralContext().
//    d) profile_manager->SetActiveProfile("default") & PurgeEphemeralProfiles().
//    e) Notify private mode changed callback (e.g. to silence/restore obtrace recorder).
//    f) Restore persistent tab (or create a new persistent tab if none remain).
class SessionPrivacyOrchestrator final {
public:
    explicit SessionPrivacyOrchestrator(
        BrowserSession& session,
        ProfileManager& profile_manager,
        engine::BrowserEngine* engine = nullptr,
        std::function<void(bool is_private)> on_private_mode_changed = nullptr);

    ~SessionPrivacyOrchestrator() = default;

    SessionPrivacyOrchestrator(const SessionPrivacyOrchestrator&) = delete;
    SessionPrivacyOrchestrator& operator=(const SessionPrivacyOrchestrator&) = delete;

    [[nodiscard]] bool IsPrivateModeActive() const noexcept;

    [[nodiscard]] bool CanActivateTab(const TabId& tab_id) const noexcept;
    [[nodiscard]] bool CanActivateTab(const Tab& tab) const noexcept;

    [[nodiscard]] bool IsTabVisible(const Tab& tab) const noexcept;

    [[nodiscard]] bool ActivateTab(const TabId& tab_id);

    [[nodiscard]] bool CloseTab(const TabId& tab_id);
    [[nodiscard]] bool CloseActiveTab();
    [[nodiscard]] std::optional<TabId> ReopenClosedTab();

    [[nodiscard]] bool EnterPrivateMode();
    void ExitPrivateMode();

    void SetPrivateModeChangedCallback(std::function<void(bool is_private)> cb) noexcept;

private:
    BrowserSession& session_;
    ProfileManager& profile_manager_;
    engine::BrowserEngine* engine_{nullptr};
    std::function<void(bool is_private)> on_private_mode_changed_;
    std::size_t ephemeral_tab_counter_{0};
    std::size_t persistent_tab_counter_{0};
};

} // namespace openbrowser::core
