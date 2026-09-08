#pragma once

#include "core/settings/settings_manager.h"

#include "include/cef_base.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_panel.h"

#include <functional>
#include <vector>

class CefBoxLayout;
class CefLabelButton;
class CefTextfield;

namespace openbrowser::desktop {

class SettingsPanel final {
public:
    using SettingsChangedCallback = std::function<void(const core::BrowserSettings&)>;

    SettingsPanel(
        core::SettingsManager& settings_manager,
        SettingsChangedCallback on_settings_changed = nullptr);
    ~SettingsPanel();

    SettingsPanel(const SettingsPanel&) = delete;
    SettingsPanel& operator=(const SettingsPanel&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();
    void RefreshFromSettings();

private:
    enum class Action {
        CycleTheme,
        CycleAccent,
        CycleSidebarState,
        CycleWallpaper,
        ToggleNewTabShortcuts,
        ToggleNewTabContext,
        ToggleRestoreSession,
        SaveHomePage,
        SaveSearchProvider,
        SaveDownloadsDirectory,
        ResetDefaults,
        Close,
    };

    class ActionDelegate;

    void HandleAction(Action action);
    void NotifySettingsChanged(const char* status_text);
    void SetStatus(const char* text);

    core::SettingsManager& settings_manager_;
    SettingsChangedCallback on_settings_changed_;

    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    CefRefPtr<CefLabelButton> theme_button_;
    CefRefPtr<CefLabelButton> accent_button_;
    CefRefPtr<CefLabelButton> sidebar_state_button_;
    CefRefPtr<CefLabelButton> wallpaper_button_;
    CefRefPtr<CefLabelButton> shortcuts_button_;
    CefRefPtr<CefLabelButton> context_button_;
    CefRefPtr<CefLabelButton> restore_session_button_;
    CefRefPtr<CefTextfield> home_page_field_;
    CefRefPtr<CefTextfield> search_provider_field_;
    CefRefPtr<CefTextfield> search_url_field_;
    CefRefPtr<CefTextfield> downloads_directory_field_;
    CefRefPtr<CefLabelButton> status_label_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
