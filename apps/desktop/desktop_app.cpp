#include "desktop_app.h"

#include "core/session/session_persistence.h"

#include "include/cef_command_line.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_fill_layout.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "include/wrapper/cef_helpers.h"

#include <fstream>
#include <memory>
#include <string>
#include <utility>

namespace openbrowser::desktop {
namespace {

class DesktopWindowDelegate final : public CefWindowDelegate {
public:
    DesktopWindowDelegate(
        CefRefPtr<CefPanel> tab_strip_panel,
        CefRefPtr<CefPanel> chrome_panel,
        CefRefPtr<CefPanel> command_palette_panel,
        CefRefPtr<CefPanel> bookmarks_bar_panel,
        CefRefPtr<CefPanel> focus_sidebar_panel,
        CefRefPtr<CefPanel> browser_host,
        CefRefPtr<CefPanel> downloads_panel,
        CefRefPtr<CefPanel> network_lab_panel,
        CefRefPtr<CefBrowserEngine> engine)
        : tab_strip_panel_(std::move(tab_strip_panel)),
          chrome_panel_(std::move(chrome_panel)),
          command_palette_panel_(std::move(command_palette_panel)),
          bookmarks_bar_panel_(std::move(bookmarks_bar_panel)),
          focus_sidebar_panel_(std::move(focus_sidebar_panel)),
          browser_host_(std::move(browser_host)),
          downloads_panel_(std::move(downloads_panel)),
          network_lab_panel_(std::move(network_lab_panel)),
          engine_(std::move(engine)) {}

    DesktopWindowDelegate(const DesktopWindowDelegate&) = delete;
    DesktopWindowDelegate& operator=(const DesktopWindowDelegate&) = delete;

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
        CEF_REQUIRE_UI_THREAD();
        window->SetTitle("Openbrowser");

        CefRefPtr<CefPanel> root_panel = CefPanel::CreatePanel(nullptr);
        CefBoxLayoutSettings root_settings{};
        root_settings.horizontal = 0;
        root_settings.between_child_spacing = 0;
        root_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
        CefRefPtr<CefBoxLayout> root_layout = root_panel->SetToBoxLayout(root_settings);

        if (tab_strip_panel_) {
            root_panel->AddChildView(tab_strip_panel_);
            root_layout->SetFlexForView(tab_strip_panel_, 0);
        }
        if (chrome_panel_) {
            root_panel->AddChildView(chrome_panel_);
            root_layout->SetFlexForView(chrome_panel_, 0);
        }
        if (command_palette_panel_) {
            root_panel->AddChildView(command_palette_panel_);
            root_layout->SetFlexForView(command_palette_panel_, 0);
        }
        if (bookmarks_bar_panel_) {
            root_panel->AddChildView(bookmarks_bar_panel_);
            root_layout->SetFlexForView(bookmarks_bar_panel_, 0);
        }

        CefRefPtr<CefPanel> body_panel = CefPanel::CreatePanel(nullptr);
        CefBoxLayoutSettings body_settings{};
        body_settings.horizontal = 1;
        body_settings.between_child_spacing = 0;
        body_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
        CefRefPtr<CefBoxLayout> body_layout = body_panel->SetToBoxLayout(body_settings);

        if (focus_sidebar_panel_) {
            body_panel->AddChildView(focus_sidebar_panel_);
            body_layout->SetFlexForView(focus_sidebar_panel_, 0);
        }
        if (browser_host_) {
            body_panel->AddChildView(browser_host_);
            body_layout->SetFlexForView(browser_host_, 1);
        }

        root_panel->AddChildView(body_panel);
        root_layout->SetFlexForView(body_panel, 1);

        if (downloads_panel_) {
            root_panel->AddChildView(downloads_panel_);
            root_layout->SetFlexForView(downloads_panel_, 0);
        }
        if (network_lab_panel_) {
            root_panel->AddChildView(network_lab_panel_);
            root_layout->SetFlexForView(network_lab_panel_, 0);
        }

        window->AddChildView(root_panel);
        root_panel->Layout();
        window->Show();
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow> /*window*/) override {
        CEF_REQUIRE_UI_THREAD();
        engine_->NotifyWindowDestroyed();
        tab_strip_panel_ = nullptr;
        chrome_panel_ = nullptr;
        command_palette_panel_ = nullptr;
        bookmarks_bar_panel_ = nullptr;
        focus_sidebar_panel_ = nullptr;
        browser_host_ = nullptr;
        downloads_panel_ = nullptr;
        network_lab_panel_ = nullptr;
        engine_ = nullptr;
    }

    bool CanClose(CefRefPtr<CefWindow> /*window*/) override {
        CEF_REQUIRE_UI_THREAD();
        engine_->BeginWindowClose();
        return engine_->CanCloseWindow();
    }

    CefSize GetPreferredSize(CefRefPtr<CefView> /*view*/) override {
        return CefSize(1280, 800);
    }

private:
    CefRefPtr<CefPanel> tab_strip_panel_;
    CefRefPtr<CefPanel> chrome_panel_;
    CefRefPtr<CefPanel> command_palette_panel_;
    CefRefPtr<CefPanel> bookmarks_bar_panel_;
    CefRefPtr<CefPanel> focus_sidebar_panel_;
    CefRefPtr<CefPanel> browser_host_;
    CefRefPtr<CefPanel> downloads_panel_;
    CefRefPtr<CefPanel> network_lab_panel_;
    CefRefPtr<CefBrowserEngine> engine_;

    IMPLEMENT_REFCOUNTING(DesktopWindowDelegate);
};


}  // namespace

CefRefPtr<CefBrowserProcessHandler> DesktopApp::GetBrowserProcessHandler() {
    return this;
}

void DesktopApp::OnContextInitialized() {
    CEF_REQUIRE_UI_THREAD();

    browser_host_ = CefPanel::CreatePanel(nullptr);
    browser_host_->SetToFillLayout();

    engine_ = new CefBrowserEngine(browser_host_);
    engine_->SetStorageRoot(StorageDirectory());

    capability_policy_ = std::make_unique<core::CapabilityPolicy>(
        core::CapabilityPolicy::CreateDefault());
    engine_->SetCapabilityPolicy(capability_policy_.get());

    transfer_broker_ = std::make_unique<core::TransferBroker>();
    engine_->SetTransferBroker(transfer_broker_.get());

    content_filter_ = std::make_unique<core::ContentFilter>();
    engine_->SetContentFilter(content_filter_.get());

    history_manager_ = std::make_unique<core::HistoryManager>();
    static_cast<void>(history_manager_->LoadFromFile(StorageDirectory() / "history.json"));

    bookmark_manager_ = std::make_unique<core::BookmarkManager>();
    static_cast<void>(bookmark_manager_->LoadFromFile(StorageDirectory() / "bookmarks.json"));

    sync_provider_ = std::make_unique<core::LocalFilesystemSyncProvider>(
        StorageDirectory() / "sync", "desktop-main");

    file_broker_ = std::make_unique<core::FileBroker>(StorageDirectory() / "downloads");

    profile_manager_ = std::make_unique<core::ProfileManager>();
    mitigation_registry_ = std::make_unique<core::CompatibilityMitigationRegistry>();
    ua_engine_ = std::make_unique<core::UserAgentPolicyEngine>();

    action_registry_ = std::make_unique<core::ActionRegistry>();
    action_registry_->RegisterAction({
        .id = "network_lab.toggle",
        .title = "Toggle Network Lab",
        .description = "Show or hide the Network Lab developer drawer",
        .category = core::ActionCategory::NetworkLab,
        .shortcut_hint = "Ctrl+Shift+L",
        .handler = [this]() {
            ToggleNetworkLab();
            return true;
        },
    });
    action_registry_->RegisterAction({
        .id = "command_palette.toggle",
        .title = "Toggle Command Palette",
        .description = "Open or close the command palette search overlay",
        .category = core::ActionCategory::Navigation,
        .shortcut_hint = "Ctrl+K",
        .handler = [this]() {
            ToggleCommandPalette();
            return true;
        },
    });
    action_registry_->RegisterAction({
        .id = "bookmarks.toggle_bar",
        .title = "Toggle Bookmarks Bar",
        .description = "Show or hide the horizontal bookmarks bar",
        .category = core::ActionCategory::Navigation,
        .shortcut_hint = "Ctrl+Shift+B",
        .handler = [this]() {
            ToggleBookmarksBar();
            return true;
        },
    });
    action_registry_->RegisterAction({
        .id = "downloads.toggle_panel",
        .title = "Toggle Downloads Panel",
        .description = "Open or close the downloads transfer drawer",
        .category = core::ActionCategory::Navigation,
        .shortcut_hint = "Ctrl+J",
        .handler = [this]() {
            ToggleDownloadsPanel();
            return true;
        },
    });
    static_cast<void>(action_registry_->RegisterAction({
        .id = "profile.toggle_incognito",
        .title = "Toggle Private Profile",
        .description = "Switch between Default profile and Ephemeral Incognito profile",
        .category = core::ActionCategory::Privacy,
        .shortcut_hint = "Ctrl+Shift+P",
        .handler = [this]() {
            ToggleProfile();
            return true;
        },
    }));
    static_cast<void>(action_registry_->RegisterAction({
        .id = "sync.trigger_local",
        .title = "Synchronize Local Data",
        .description = "Synchronize bookmarks and workspaces using local filesystem provider",
        .category = core::ActionCategory::Navigation,
        .shortcut_hint = "Ctrl+Shift+S",
        .handler = [this]() {
            SyncLocalData();
            return true;
        },
    }));
    static_cast<void>(action_registry_->RegisterAction({
        .id = "network_lab.toggle_recorder",
        .title = "Toggle .obtrace Live Recording",
        .description = "Start or stop streaming live network events to a local .obtrace file",
        .category = core::ActionCategory::NetworkLab,
        .shortcut_hint = "Ctrl+Shift+R",
        .handler = [this]() {
            ToggleLiveTraceRecording();
            return true;
        },
    }));
    // M7.4 — Developer tools export actions.
    static_cast<void>(action_registry_->RegisterAction({
        .id = "devtools.export_har",
        .title = "Export Network HAR",
        .description = "Export the current Network Lab trace as a HAR 1.2 file",
        .category = core::ActionCategory::NetworkLab,
        .shortcut_hint = "Ctrl+Shift+H",
        .handler = [this]() {
            ExportNetworkHar();
            return true;
        },
    }));
    static_cast<void>(action_registry_->RegisterAction({
        .id = "devtools.export_obtrace",
        .title = "Export Network .obtrace",
        .description = "Export the current Network Lab trace as a versioned .obtrace NDJSON file",
        .category = core::ActionCategory::NetworkLab,
        .shortcut_hint = "Ctrl+Shift+O",
        .handler = [this]() {
            ExportNetworkObtrace();
            return true;
        },
    }));

    network_trace_ = std::make_unique<devtools::network::NetworkTraceBuffer>();
    engine_->SetNetworkObservationSink(network_trace_.get());

    // M7.1 — Connection diagnostics registry.
    connection_registry_ = std::make_unique<devtools::network::ConnectionRegistry>();
    engine_->SetConnectionRegistry(connection_registry_.get());

    // M7.2 — Filter decision log; wire into ContentFilter and engine.
    filter_decision_log_ = std::make_unique<core::FilterDecisionLog>();
    content_filter_->SetDecisionLog(filter_decision_log_.get());
    engine_->SetDecisionLog(filter_decision_log_.get());

    // Wire UserAgentEngine and MitigationRegistry into engine.
    engine_->SetUserAgentPolicyEngine(ua_engine_.get());
    engine_->SetMitigationRegistry(mitigation_registry_.get());

    // Attach decision log to network trace buffer for live correlation
    network_trace_->SetDecisionLog(filter_decision_log_.get());

    // M7.3 — .obtrace trace recorder (begins recording on user request).
    obtrace_recorder_ = std::make_unique<devtools::network::ObtraceRecorder>();
    network_trace_->AddObserver(obtrace_recorder_.get());

    workspace_manager_ = std::make_unique<core::WorkspaceManager>();
    focus_queue_ = std::make_unique<core::FocusQueue>();
    session_ = std::make_unique<core::BrowserSession>(*engine_);
    session_file_path_ = SessionFilePath();

    bool restored = false;
    const auto snapshot = core::SessionPersistence::LoadFromFile(session_file_path_);
    if (snapshot.has_value() && !snapshot->tabs.empty()) {
        restored = core::SessionPersistence::RestoreSession(*session_, *focus_queue_, *snapshot);
    }

    if (!restored || session_->Tabs().empty()) {
        core::Tab initial_tab;
        initial_tab.id = "initial";
        initial_tab.url = StartupUrl();
        initial_tab.title = "New tab";
        initial_tab.lifecycle = core::TabLifecycle::Active;
        initial_tab.workspace_id = std::nullopt;

        const bool opened = session_->OpenTab(std::move(initial_tab));

        if (!opened) {
            engine_->BeginWindowClose();
            CefQuitMessageLoop();
            return;
        }
    }

    tab_strip_ = std::make_unique<TabStrip>(*session_, workspace_manager_.get(), profile_manager_.get());
    bookmarks_bar_ = std::make_unique<BookmarksBar>(*bookmark_manager_, *session_);
    downloads_panel_ = std::make_unique<DownloadsPanel>(*transfer_broker_, *file_broker_);
    command_palette_overlay_ = std::make_unique<CommandPaletteOverlay>(*action_registry_);
    chrome_ = std::make_unique<BrowserChrome>(
        *session_, engine_,
        [this]() { ToggleNetworkLab(); },
        [this]() { ToggleCommandPalette(); },
        [this]() { ToggleBookmarksBar(); },
        [this]() { ToggleDownloadsPanel(); },
        [this]() { ToggleProfile(); });
    focus_sidebar_ = std::make_unique<FocusSidebar>(*session_, *focus_queue_);
    network_lab_panel_ = std::make_unique<NetworkLabPanel>(
        *network_trace_, *session_,
        connection_registry_.get(),
        filter_decision_log_.get(),
        obtrace_recorder_.get());

    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    const bool show_network_lab = command_line && command_line->HasSwitch("network-lab");
    network_lab_panel_->SetVisible(show_network_lab);

    session_->AddObserver(this);

    // Persist running session with clean_shutdown = false for crash detection
    SaveCurrentSession(false);

    CefWindow::CreateTopLevelWindow(
        new DesktopWindowDelegate(
            tab_strip_->View(),
            chrome_->View(),
            command_palette_overlay_->View(),
            bookmarks_bar_->View(),
            focus_sidebar_->View(),
            browser_host_,
            downloads_panel_->View(),
            network_lab_panel_->View(),
            engine_));
}

void DesktopApp::ShutdownRuntime() {
    if (session_) {
        session_->RemoveObserver(this);
    }

    const bool is_ephemeral = (engine_ && engine_->IsEphemeralMode()) ||
        (profile_manager_ && profile_manager_->GetActiveProfile() &&
         profile_manager_->GetActiveProfile()->IsEphemeral());

    if (!is_ephemeral) {
        SaveCurrentSession(true);
        if (history_manager_) {
            static_cast<void>(history_manager_->SaveToFile(StorageDirectory() / "history.json"));
        }
        if (bookmark_manager_) {
            static_cast<void>(bookmark_manager_->SaveToFile(StorageDirectory() / "bookmarks.json"));
        }
    }

    if (obtrace_recorder_ && obtrace_recorder_->IsRecording()) {
        obtrace_recorder_->StopRecording();
    }

    if (engine_) {
        engine_->SetDecisionLog(nullptr);
        engine_->SetMitigationRegistry(nullptr);
        engine_->SetUserAgentPolicyEngine(nullptr);
        engine_->SetConnectionRegistry(nullptr);
        engine_->SetTransferBroker(nullptr);
        engine_->SetContentFilter(nullptr);
        engine_->SetCapabilityPolicy(nullptr);
        engine_->SetNetworkObservationSink(nullptr);
    }

    command_palette_overlay_.reset();
    bookmarks_bar_.reset();
    downloads_panel_.reset();
    ua_engine_.reset();
    mitigation_registry_.reset();
    profile_manager_.reset();
    sync_provider_.reset();
    action_registry_.reset();
    bookmark_manager_.reset();
    history_manager_.reset();
    content_filter_.reset();
    file_broker_.reset();
    transfer_broker_.reset();
    network_lab_panel_.reset();
    focus_sidebar_.reset();
    chrome_.reset();
    tab_strip_.reset();
    session_.reset();
    focus_queue_.reset();
    workspace_manager_.reset();
    network_trace_.reset();
    capability_policy_.reset();
    engine_ = nullptr;
    browser_host_ = nullptr;
}

void DesktopApp::OnBrowserSessionChanged(const core::BrowserSession& session) {
    CEF_REQUIRE_UI_THREAD();
    const bool is_profile_private = (profile_manager_ && profile_manager_->GetActiveProfile() &&
                                     profile_manager_->GetActiveProfile()->IsEphemeral());
    const bool is_engine_private = (engine_ && engine_->IsEphemeralMode());

    if (session.ActiveTabId().has_value()) {
        const auto* active_tab = session.FindTab(*session.ActiveTabId());
        if (active_tab) {
            const bool is_tab_private = active_tab->is_ephemeral;
            const bool is_private = is_profile_private || is_engine_private || is_tab_private;

            if (!is_private && history_manager_ && !active_tab->url.empty() && active_tab->url != "about:blank") {
                history_manager_->RecordVisit(active_tab->url, active_tab->title);
            }
            if (!is_private) {
                SaveCurrentSession(false);
            }
        }
    }
}

void DesktopApp::ToggleNetworkLab() {
    CEF_REQUIRE_UI_THREAD();
    if (network_lab_panel_) {
        network_lab_panel_->ToggleVisibility();
    }
}

void DesktopApp::ToggleCommandPalette() {
    CEF_REQUIRE_UI_THREAD();
    if (command_palette_overlay_) {
        command_palette_overlay_->ToggleVisibility();
    }
}

void DesktopApp::ToggleBookmarksBar() {
    CEF_REQUIRE_UI_THREAD();
    if (bookmarks_bar_) {
        bookmarks_bar_->ToggleVisibility();
    }
}

void DesktopApp::ToggleDownloadsPanel() {
    CEF_REQUIRE_UI_THREAD();
    if (downloads_panel_) {
        downloads_panel_->ToggleVisibility();
    }
}

void DesktopApp::ToggleProfile() {
    CEF_REQUIRE_UI_THREAD();
    if (!profile_manager_) {
        return;
    }
    auto active = profile_manager_->GetActiveProfile();
    if (active && active->IsEphemeral()) {
        // Exiting Private Mode: close all private tabs first
        if (session_) {
            std::vector<core::TabId> ephemeral_tab_ids;
            for (const auto& t : session_->Tabs()) {
                if (t.is_ephemeral) {
                    ephemeral_tab_ids.push_back(t.id);
                }
            }
            for (const auto& tid : ephemeral_tab_ids) {
                session_->CloseTab(tid);
            }
            if (session_->Tabs().empty()) {
                static std::size_t s_def_counter = 0;
                core::Tab def_tab;
                def_tab.id = "tab-" + std::to_string(++s_def_counter);
                def_tab.url = "https://example.com/";
                def_tab.title = "New Tab";
                def_tab.lifecycle = core::TabLifecycle::Active;
                def_tab.is_ephemeral = false;
                session_->OpenTab(std::move(def_tab), true);
            } else if (!session_->ActiveTabId().has_value() || session_->FindTab(*session_->ActiveTabId()) == nullptr) {
                session_->ActivateTab(session_->Tabs().front().id);
            }
        }

        profile_manager_->SetActiveProfile("default");
        profile_manager_->PurgeEphemeralProfiles();
        if (engine_) {
            engine_->SetEphemeralMode(false);
            engine_->PurgeEphemeralContext();
        }
        if (obtrace_recorder_) {
            obtrace_recorder_->SetPrivateMode(false);
        }
        if (chrome_) {
            chrome_->SetProfileLabel("[👤 Default]");
        }
    } else {
        // Entering Private Mode: create ephemeral profile, switch engine, and open a private tab
        auto eph = profile_manager_->CreateEphemeralProfile("Private Session");
        if (eph) {
            profile_manager_->SetActiveProfile(eph->GetId());
            if (engine_) {
                engine_->SetEphemeralMode(true);
            }
            if (obtrace_recorder_) {
                obtrace_recorder_->SetPrivateMode(true);
            }
            if (session_) {
                static std::size_t s_priv_counter = 0;
                core::Tab priv_tab;
                priv_tab.id = "private-tab-" + std::to_string(++s_priv_counter);
                priv_tab.url = "https://example.com/";
                priv_tab.title = "Private Tab";
                priv_tab.lifecycle = core::TabLifecycle::Active;
                priv_tab.is_ephemeral = true;
                session_->OpenTab(std::move(priv_tab), true);
            }
            if (chrome_) {
                chrome_->SetProfileLabel("[🕶 Private]");
            }
        }
    }
}

void DesktopApp::SyncLocalData() {
    CEF_REQUIRE_UI_THREAD();
    const bool is_private = (engine_ && engine_->IsEphemeralMode()) ||
        (profile_manager_ && profile_manager_->GetActiveProfile() &&
         profile_manager_->GetActiveProfile()->IsEphemeral());
    if (is_private || !sync_provider_ || !bookmark_manager_) {
        return;
    }

    std::vector<core::SyncRecord> records;
    const auto bookmarks = bookmark_manager_->ListBookmarks();
    for (const auto& bm : bookmarks) {
        records.push_back({
            .entity_type = core::SyncEntityType::Bookmark,
            .record_id = bm.id,
            .version = 1,
            .timestamp = static_cast<std::uint64_t>(bm.created_at_ms > 0 ? bm.created_at_ms : 1),
            .payload_json = "{\"url\":\"" + bm.url + "\",\"title\":\"" + bm.title + "\"}",
            .is_deleted = false,
        });
    }

    sync_provider_->PushRecords(records);
    sync_provider_->SaveToDisk();
}

void DesktopApp::ToggleLiveTraceRecording() {
    CEF_REQUIRE_UI_THREAD();
    if (!obtrace_recorder_) {
        return;
    }
    if (obtrace_recorder_->IsRecording()) {
        obtrace_recorder_->StopRecording();
    } else {
        const bool is_private = (engine_ && engine_->IsEphemeralMode()) ||
            (profile_manager_ && profile_manager_->GetActiveProfile() &&
             profile_manager_->GetActiveProfile()->IsEphemeral());
        if (!is_private) {
            const auto trace_path = StorageDirectory() / "live_trace.obtrace";
            static_cast<void>(obtrace_recorder_->StartRecording(trace_path.string()));
        }
    }
}

std::filesystem::path DesktopApp::StorageDirectory() const {
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    if (command_line && command_line->HasSwitch("storage-dir")) {
        const auto configured = command_line->GetSwitchValue("storage-dir").ToString();
        if (!configured.empty()) {
            return configured;
        }
    }

    return "openbrowser_storage";
}

std::filesystem::path DesktopApp::SessionFilePath() const {
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    if (command_line && command_line->HasSwitch("session-file")) {
        const auto configured = command_line->GetSwitchValue("session-file").ToString();
        if (!configured.empty()) {
            return configured;
        }
    }

    return "openbrowser_session.json";
}

void DesktopApp::SaveCurrentSession(const bool clean_shutdown) {
    const bool is_private = (engine_ && engine_->IsEphemeralMode()) ||
        (profile_manager_ && profile_manager_->GetActiveProfile() &&
         profile_manager_->GetActiveProfile()->IsEphemeral());
    if (is_private || !session_ || !focus_queue_ || session_file_path_.empty()) {
        return;
    }

    const auto snapshot = core::SessionPersistence::CaptureSnapshot(
        *session_, *focus_queue_, clean_shutdown);
    static_cast<void>(core::SessionPersistence::SaveToFile(session_file_path_, snapshot));
}

std::string DesktopApp::StartupUrl() const {
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    if (command_line) {
        const auto configured = command_line->GetSwitchValue("url").ToString();
        if (!configured.empty()) {
            return configured;
        }
    }

    return "https://example.com/";
}

void DesktopApp::ExportNetworkHar() {
    if (!network_trace_) {
        return;
    }
    const auto requests = network_trace_->AggregateRequests();
    const auto har_content = devtools::network::NetworkTraceBuffer::ExportToHar(requests);
    // Write to a local file in the storage directory.
    const auto out_path = StorageDirectory() / "network_export.har";
    if (auto f = std::ofstream(out_path); f.is_open()) {
        f << har_content;
    }
}

void DesktopApp::ExportNetworkObtrace() {
    if (!network_trace_) {
        return;
    }
    const auto requests = network_trace_->AggregateRequests();
    const auto obtrace_content = devtools::network::NetworkTraceBuffer::ExportToObtrace(requests);
    const auto out_path = StorageDirectory() / "network_export.obtrace";
    if (auto f = std::ofstream(out_path); f.is_open()) {
        f << obtrace_content;
    }
}

}  // namespace openbrowser::desktop
