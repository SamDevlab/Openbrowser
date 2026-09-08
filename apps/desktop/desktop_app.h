#pragma once

#include "aura_sidebar.h"
#include "bookmarks_bar.h"
#include "browser_chrome.h"
#include "cef_browser_engine.h"
#include "command_palette_overlay.h"
#include "downloads_panel.h"
#include "library_panel.h"
#include "settings_panel.h"
#include "core/bookmarks/bookmark_manager.h"
#include "core/capabilities/capability_policy.h"
#include "core/commands/action_registry.h"
#include "core/compatibility/compatibility_mitigations.h"
#include "core/compatibility/user_agent_policy.h"
#include "core/filters/content_filter.h"
#include "core/focus_queue/focus_queue.h"
#include "core/history/history_manager.h"
#include "core/network/filter_decision_log.h"  // M7.2
#include "core/profiles/profile_manager.h"
#include "core/session/browser_session.h"
#include "core/session/browser_session_observer.h"
#include "core/session/session_history_bridge.h"
#include "core/session/session_privacy_orchestrator.h"
#include "core/settings/settings_manager.h"
#include "core/sync/local_filesystem_sync.h"
#include "core/transfers/file_broker.h"
#include "core/transfers/transfer_broker.h"
#include "core/workspaces/workspace_manager.h"
#include "devtools/network/connection_diagnostics.h" // M7.1
#include "devtools/network/network_trace.h"
#include "devtools/network/obtrace_recorder.h"       // M7.3
#include "focus_sidebar.h"
#include "network_lab_panel.h"
#include "tab_strip.h"

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"

#include <filesystem>
#include <memory>
#include <string>

namespace openbrowser::desktop {

class DesktopApp final : public CefApp,
                         public CefBrowserProcessHandler,
                         public core::BrowserSessionObserver {
public:
    DesktopApp() = default;
    DesktopApp(const DesktopApp&) = delete;
    DesktopApp& operator=(const DesktopApp&) = delete;

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
    void OnContextInitialized() override;

    // Must run after CefRunMessageLoop returns and before CefShutdown.
    void ShutdownRuntime();

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    [[nodiscard]] std::string StartupUrl() const;
    [[nodiscard]] std::filesystem::path SessionFilePath() const;
    [[nodiscard]] std::filesystem::path StorageDirectory() const;
    void SaveCurrentSession(bool clean_shutdown);
    void HideTransientPanels();
    void ToggleAuraSidebar();
    void CycleWorkspace();
    void SelectWorkspace(const std::string& workspace_id);
    void ToggleFocusPanel();
    void ApplyFocusChromeState(bool enabled);
    void RelayoutBrowserWindow();
    void ToggleNetworkLab();
    void ToggleCommandPalette();
    void ToggleBookmarksBar();
    void ToggleDownloadsPanel();
    void ToggleLibraryPanel();
    void ToggleSettingsPanel();
    void ToggleProfile();
    void ApplySettings(const core::BrowserSettings& settings);
    void SyncLocalData();
    void ToggleLiveTraceRecording();
    void ExportNetworkHar();      // M7.4
    void ExportNetworkObtrace(); // M7.4
    void OpenNewTab();
    void CloseActiveTab();
    void ReopenClosedTab();
    void CycleTab(bool forward);

    CefRefPtr<CefPanel> browser_host_;
    CefRefPtr<CefBrowserEngine> engine_;
    std::unique_ptr<core::SettingsManager> settings_manager_;
    std::unique_ptr<core::WorkspaceManager> workspace_manager_;
    std::unique_ptr<core::FocusQueue> focus_queue_;
    std::unique_ptr<core::BrowserSession> session_;
    std::unique_ptr<TabStrip> tab_strip_;
    std::unique_ptr<BrowserChrome> chrome_;
    std::unique_ptr<BookmarksBar> bookmarks_bar_;
    std::unique_ptr<DownloadsPanel> downloads_panel_;
    std::unique_ptr<LibraryPanel> library_panel_;
    std::unique_ptr<SettingsPanel> settings_panel_;
    std::unique_ptr<CommandPaletteOverlay> command_palette_overlay_;
    std::unique_ptr<AuraSidebar> aura_sidebar_;
    std::unique_ptr<FocusSidebar> focus_sidebar_;
    std::unique_ptr<devtools::network::NetworkTraceBuffer> network_trace_;
    std::unique_ptr<NetworkLabPanel> network_lab_panel_;
    std::unique_ptr<core::CapabilityPolicy> capability_policy_;
    std::unique_ptr<core::TransferBroker> transfer_broker_;
    std::unique_ptr<core::FileBroker> file_broker_;
    std::unique_ptr<core::ContentFilter> content_filter_;
    std::unique_ptr<core::HistoryManager> history_manager_;
    std::unique_ptr<core::BookmarkManager> bookmark_manager_;
    std::unique_ptr<core::ActionRegistry> action_registry_;
    std::unique_ptr<core::LocalFilesystemSyncProvider> sync_provider_;
    std::unique_ptr<core::ProfileManager> profile_manager_;
    std::unique_ptr<core::CompatibilityMitigationRegistry> mitigation_registry_;
    std::unique_ptr<core::UserAgentPolicyEngine> ua_engine_;
    // M7 subsystems
    std::unique_ptr<devtools::network::ConnectionRegistry> connection_registry_; // M7.1
    std::unique_ptr<core::FilterDecisionLog> filter_decision_log_;               // M7.2
    std::unique_ptr<devtools::network::ObtraceRecorder> obtrace_recorder_;       // M7.3
    std::unique_ptr<core::SessionHistoryBridge> session_history_bridge_;
    std::unique_ptr<core::SessionPrivacyOrchestrator> privacy_orchestrator_;
    std::filesystem::path session_file_path_;
    std::size_t next_tab_index_{1};

    bool focus_chrome_active_{false};
    bool aura_sidebar_visible_before_focus_{false};
    bool bookmarks_bar_visible_before_focus_{false};
    bool tab_strip_visible_before_focus_{true};

    IMPLEMENT_REFCOUNTING(DesktopApp);
};

}  // namespace openbrowser::desktop
