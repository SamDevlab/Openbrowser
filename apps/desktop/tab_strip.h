#pragma once

#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_panel.h"

#include <cstddef>
#include <string>
#include <vector>

namespace openbrowser::core {
class BrowserSession;
class WorkspaceManager;
class ProfileManager;
class SessionPrivacyOrchestrator;
struct Tab;
}

class CefBoxLayout;
class CefButtonDelegate;

namespace openbrowser::desktop {

class TabStrip final : public core::BrowserSessionObserver {
public:
    explicit TabStrip(
        core::BrowserSession& session,
        core::WorkspaceManager* workspace_manager = nullptr,
        core::ProfileManager* profile_manager = nullptr,
        core::SessionPrivacyOrchestrator* privacy_orchestrator = nullptr);
    ~TabStrip() override;

    TabStrip(const TabStrip&) = delete;
    TabStrip& operator=(const TabStrip&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    enum class TabAction {
        Activate,
        Close,
        NewTab,
        CycleWorkspace,
    };

    class TabActionDelegate;

    void HandleTabAction(TabAction action, const std::string& tab_id);
    [[nodiscard]] bool IsTabInActiveWorkspace(const core::Tab& tab) const;
    [[nodiscard]] bool IsTabVisibleInCurrentContext(const core::Tab& tab) const;
    bool ActivateOrCreateTabForActiveWorkspace();
    bool OpenNewTabForActiveWorkspace();
    void SyncWorkspaceToActiveTab();
    void RebuildTabs();

    core::BrowserSession& session_;
    core::WorkspaceManager* workspace_manager_{nullptr};
    core::ProfileManager* profile_manager_{nullptr};
    core::SessionPrivacyOrchestrator* privacy_orchestrator_{nullptr};
    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::size_t next_tab_index_{1};
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
