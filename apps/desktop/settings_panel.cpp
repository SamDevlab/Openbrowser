#include "settings_panel.h"

#include "aura_sidebar.h"
#include "cef_browser_engine.h"
#include "core/navigation/internal_urls.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_textfield.h"
#include "include/wrapper/cef_helpers.h"

#include <cctype>
#include <initializer_list>
#include <string>
#include <utility>

namespace openbrowser::desktop {
namespace {

class PassiveButtonDelegate final : public CefButtonDelegate {
public:
    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
    }

private:
    IMPLEMENT_REFCOUNTING(PassiveButtonDelegate);
};

CefRefPtr<CefPanel> CreateRow() {
    CefRefPtr<CefPanel> row = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings settings{};
    settings.horizontal = 1;
    settings.between_child_spacing = 6;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    row->SetToBoxLayout(settings);
    return row;
}

std::string DisplayValue(std::string value) {
    if (!value.empty()) {
        value.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(value.front())));
    }
    return value;
}

std::string CycleValue(
    const std::string& current,
    const std::initializer_list<const char*> values) {
    if (values.size() == 0) {
        return current;
    }

    auto it = values.begin();
    for (; it != values.end(); ++it) {
        if (current == *it) {
            auto next = it;
            ++next;
            return next == values.end() ? std::string(*values.begin()) : std::string(*next);
        }
    }
    return std::string(*values.begin());
}

void ApplyAuraPreferences(const core::BrowserSettings& settings) {
    ApplyGlobalAuraSidebarState(settings.aura_sidebar_state);

    if (auto* engine = CefBrowserEngine::ActiveInstance(); engine != nullptr) {
        engine->SetNewTabAppearance({
            .theme = settings.appearance_theme,
            .accent = settings.appearance_accent,
            .wallpaper = settings.new_tab_wallpaper,
            .show_shortcuts = settings.new_tab_show_shortcuts,
            .show_context = settings.new_tab_show_context,
        });
    }
}

}  // namespace

class SettingsPanel::ActionDelegate final : public CefButtonDelegate {
public:
    ActionDelegate(SettingsPanel& panel, const Action action)
        : panel_(panel), action_(action) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        panel_.HandleAction(action_);
    }

private:
    SettingsPanel& panel_;
    Action action_;

    IMPLEMENT_REFCOUNTING(ActionDelegate);
};

SettingsPanel::SettingsPanel(
    core::SettingsManager& settings_manager,
    SettingsChangedCallback on_settings_changed)
    : settings_manager_(settings_manager),
      on_settings_changed_(std::move(on_settings_changed)) {
    panel_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings panel_settings{};
    panel_settings.horizontal = 0;
    panel_settings.between_child_spacing = 6;
    panel_settings.inside_border_horizontal_spacing = 10;
    panel_settings.inside_border_vertical_spacing = 8;
    panel_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    layout_ = panel_->SetToBoxLayout(panel_settings);

    const auto make_button = [this](const Action action, const std::string& text) {
        CefRefPtr<CefButtonDelegate> delegate(new ActionDelegate(*this, action));
        delegates_.push_back(delegate);
        return CefLabelButton::CreateLabelButton(delegate, text);
    };
    const auto make_passive = [](const std::string& text) {
        auto label = CefLabelButton::CreateLabelButton(new PassiveButtonDelegate(), text);
        label->SetEnabled(false);
        return label;
    };

    CefRefPtr<CefPanel> header = CreateRow();
    CefRefPtr<CefBoxLayout> header_layout = header->GetLayout()->AsBoxLayout();
    CefRefPtr<CefLabelButton> title = make_passive("Settings - Personalization");
    CefRefPtr<CefLabelButton> close = make_button(Action::Close, "Close");
    header->AddChildView(title);
    if (header_layout) {
        header_layout->SetFlexForView(title, 1);
    }
    header->AddChildView(close);
    if (header_layout) {
        header_layout->SetFlexForView(close, 0);
    }
    panel_->AddChildView(header);
    layout_->SetFlexForView(header, 0);

    CefRefPtr<CefLabelButton> appearance_label = make_passive("Appearance");
    panel_->AddChildView(appearance_label);
    layout_->SetFlexForView(appearance_label, 0);

    CefRefPtr<CefPanel> appearance_row = CreateRow();
    CefRefPtr<CefBoxLayout> appearance_layout = appearance_row->GetLayout()->AsBoxLayout();
    theme_button_ = make_button(Action::CycleTheme, "Theme: Dark");
    accent_button_ = make_button(Action::CycleAccent, "Accent: Violet");
    sidebar_state_button_ = make_button(Action::CycleSidebarState, "Sidebar default: Compact");
    appearance_row->AddChildView(theme_button_);
    appearance_row->AddChildView(accent_button_);
    appearance_row->AddChildView(sidebar_state_button_);
    if (appearance_layout) {
        appearance_layout->SetFlexForView(theme_button_, 1);
        appearance_layout->SetFlexForView(accent_button_, 1);
        appearance_layout->SetFlexForView(sidebar_state_button_, 1);
    }
    panel_->AddChildView(appearance_row);
    layout_->SetFlexForView(appearance_row, 0);

    CefRefPtr<CefPanel> new_tab_row = CreateRow();
    CefRefPtr<CefBoxLayout> new_tab_layout = new_tab_row->GetLayout()->AsBoxLayout();
    wallpaper_button_ = make_button(Action::CycleWallpaper, "Wallpaper: Aura");
    shortcuts_button_ = make_button(Action::ToggleNewTabShortcuts, "Quick access: On");
    context_button_ = make_button(Action::ToggleNewTabContext, "Browser hints: On");
    new_tab_row->AddChildView(wallpaper_button_);
    new_tab_row->AddChildView(shortcuts_button_);
    new_tab_row->AddChildView(context_button_);
    if (new_tab_layout) {
        new_tab_layout->SetFlexForView(wallpaper_button_, 1);
        new_tab_layout->SetFlexForView(shortcuts_button_, 1);
        new_tab_layout->SetFlexForView(context_button_, 1);
    }
    panel_->AddChildView(new_tab_row);
    layout_->SetFlexForView(new_tab_row, 0);

    CefRefPtr<CefLabelButton> browser_label = make_passive("Browser behavior");
    panel_->AddChildView(browser_label);
    layout_->SetFlexForView(browser_label, 0);

    restore_session_button_ = make_button(Action::ToggleRestoreSession, "Restore previous session: On");
    panel_->AddChildView(restore_session_button_);
    layout_->SetFlexForView(restore_session_button_, 0);

    CefRefPtr<CefPanel> home_row = CreateRow();
    CefRefPtr<CefBoxLayout> home_layout = home_row->GetLayout()->AsBoxLayout();
    home_page_field_ = CefTextfield::CreateTextfield(nullptr);
    home_page_field_->SetPlaceholderText("Startup home page URL");
    CefRefPtr<CefLabelButton> save_home = make_button(Action::SaveHomePage, "Save home");
    home_row->AddChildView(home_page_field_);
    if (home_layout) {
        home_layout->SetFlexForView(home_page_field_, 1);
    }
    home_row->AddChildView(save_home);
    if (home_layout) {
        home_layout->SetFlexForView(save_home, 0);
    }
    panel_->AddChildView(home_row);
    layout_->SetFlexForView(home_row, 0);

    CefRefPtr<CefPanel> search_name_row = CreateRow();
    CefRefPtr<CefBoxLayout> search_name_layout = search_name_row->GetLayout()->AsBoxLayout();
    search_provider_field_ = CefTextfield::CreateTextfield(nullptr);
    search_provider_field_->SetPlaceholderText("Search provider name");
    search_name_row->AddChildView(search_provider_field_);
    if (search_name_layout) {
        search_name_layout->SetFlexForView(search_provider_field_, 1);
    }
    panel_->AddChildView(search_name_row);
    layout_->SetFlexForView(search_name_row, 0);

    CefRefPtr<CefPanel> search_url_row = CreateRow();
    CefRefPtr<CefBoxLayout> search_url_layout = search_url_row->GetLayout()->AsBoxLayout();
    search_url_field_ = CefTextfield::CreateTextfield(nullptr);
    search_url_field_->SetPlaceholderText("Search URL template, e.g. https://duckduckgo.com/?q=%s");
    CefRefPtr<CefLabelButton> save_search = make_button(Action::SaveSearchProvider, "Save search");
    search_url_row->AddChildView(search_url_field_);
    if (search_url_layout) {
        search_url_layout->SetFlexForView(search_url_field_, 1);
    }
    search_url_row->AddChildView(save_search);
    if (search_url_layout) {
        search_url_layout->SetFlexForView(save_search, 0);
    }
    panel_->AddChildView(search_url_row);
    layout_->SetFlexForView(search_url_row, 0);

    CefRefPtr<CefPanel> downloads_row = CreateRow();
    CefRefPtr<CefBoxLayout> downloads_layout = downloads_row->GetLayout()->AsBoxLayout();
    downloads_directory_field_ = CefTextfield::CreateTextfield(nullptr);
    downloads_directory_field_->SetPlaceholderText("Downloads directory (empty = Openbrowser default)");
    CefRefPtr<CefLabelButton> save_downloads = make_button(Action::SaveDownloadsDirectory, "Save downloads");
    downloads_row->AddChildView(downloads_directory_field_);
    if (downloads_layout) {
        downloads_layout->SetFlexForView(downloads_directory_field_, 1);
    }
    downloads_row->AddChildView(save_downloads);
    if (downloads_layout) {
        downloads_layout->SetFlexForView(save_downloads, 0);
    }
    panel_->AddChildView(downloads_row);
    layout_->SetFlexForView(downloads_row, 0);

    CefRefPtr<CefPanel> footer = CreateRow();
    CefRefPtr<CefBoxLayout> footer_layout = footer->GetLayout()->AsBoxLayout();
    status_label_ = make_passive("Settings are stored locally");
    CefRefPtr<CefLabelButton> reset = make_button(Action::ResetDefaults, "Reset defaults");
    footer->AddChildView(status_label_);
    if (footer_layout) {
        footer_layout->SetFlexForView(status_label_, 1);
    }
    footer->AddChildView(reset);
    if (footer_layout) {
        footer_layout->SetFlexForView(reset, 0);
    }
    panel_->AddChildView(footer);
    layout_->SetFlexForView(footer, 0);

    RefreshFromSettings();
    ApplyAuraPreferences(settings_manager_.Settings());
    panel_->SetVisible(false);
}

SettingsPanel::~SettingsPanel() = default;

CefRefPtr<CefPanel> SettingsPanel::View() const noexcept {
    return panel_;
}

void SettingsPanel::SetVisible(const bool visible) {
    CEF_REQUIRE_UI_THREAD();
    if (!panel_) {
        return;
    }
    const bool was_visible = panel_->IsVisible();
    if (visible) {
        RefreshFromSettings();
    }
    panel_->SetVisible(visible);
    if (const auto parent = panel_->GetParentView(); parent) {
        if (const auto parent_panel = parent->AsPanel(); parent_panel) {
            parent_panel->Layout();
        }
    }
    if (was_visible != panel_->IsVisible() && on_visibility_changed_) {
        on_visibility_changed_(panel_->IsVisible());
    }
}

void SettingsPanel::SetVisibilityChangedCallback(VisibilityChangedCallback callback) {
    on_visibility_changed_ = std::move(callback);
}

bool SettingsPanel::IsVisible() const {
    return panel_ && panel_->IsVisible();
}

void SettingsPanel::ToggleVisibility() {
    CEF_REQUIRE_UI_THREAD();
    SetVisible(!IsVisible());
}

void SettingsPanel::RefreshFromSettings() {
    CEF_REQUIRE_UI_THREAD();
    const auto& settings = settings_manager_.Settings();

    if (theme_button_) {
        theme_button_->SetText("Theme: " + DisplayValue(settings.appearance_theme));
    }
    if (accent_button_) {
        accent_button_->SetText("Accent: " + DisplayValue(settings.appearance_accent));
    }
    if (sidebar_state_button_) {
        sidebar_state_button_->SetText("Sidebar default: " + DisplayValue(settings.aura_sidebar_state));
    }
    if (wallpaper_button_) {
        wallpaper_button_->SetText("Wallpaper: " + DisplayValue(settings.new_tab_wallpaper));
    }
    if (shortcuts_button_) {
        shortcuts_button_->SetText(
            settings.new_tab_show_shortcuts ? "Quick access: On" : "Quick access: Off");
    }
    if (context_button_) {
        context_button_->SetText(
            settings.new_tab_show_context ? "Browser hints: On" : "Browser hints: Off");
    }
    if (restore_session_button_) {
        restore_session_button_->SetText(
            settings.restore_session_on_startup
                ? "Restore previous session: On"
                : "Restore previous session: Off");
    }
    if (home_page_field_) {
        home_page_field_->SetText(settings.home_page_url);
    }
    if (search_provider_field_) {
        search_provider_field_->SetText(settings.search_provider_name);
    }
    if (search_url_field_) {
        search_url_field_->SetText(settings.search_url_template);
    }
    if (downloads_directory_field_) {
        downloads_directory_field_->SetText(settings.downloads_directory);
    }
}

void SettingsPanel::HandleAction(const Action action) {
    CEF_REQUIRE_UI_THREAD();

    switch (action) {
        case Action::CycleTheme: {
            auto updated = settings_manager_.Settings();
            updated.appearance_theme = CycleValue(updated.appearance_theme, {"dark", "light", "system"});
            settings_manager_.UpdateSettings(std::move(updated));
            NotifySettingsChanged("Theme saved - reload New Tab to refresh existing pages");
            return;
        }
        case Action::CycleAccent: {
            auto updated = settings_manager_.Settings();
            updated.appearance_accent = CycleValue(
                updated.appearance_accent, {"violet", "blue", "rose", "green"});
            settings_manager_.UpdateSettings(std::move(updated));
            NotifySettingsChanged("Accent saved - reload New Tab to refresh existing pages");
            return;
        }
        case Action::CycleSidebarState: {
            auto updated = settings_manager_.Settings();
            updated.aura_sidebar_state = CycleValue(
                updated.aura_sidebar_state, {"compact", "expanded", "hidden"});
            settings_manager_.UpdateSettings(std::move(updated));
            NotifySettingsChanged("Sidebar default saved and applied");
            return;
        }
        case Action::CycleWallpaper: {
            auto updated = settings_manager_.Settings();
            updated.new_tab_wallpaper = CycleValue(
                updated.new_tab_wallpaper, {"aura", "midnight", "soft"});
            settings_manager_.UpdateSettings(std::move(updated));
            NotifySettingsChanged("Wallpaper saved - reload New Tab to refresh existing pages");
            return;
        }
        case Action::ToggleNewTabShortcuts: {
            auto updated = settings_manager_.Settings();
            updated.new_tab_show_shortcuts = !updated.new_tab_show_shortcuts;
            settings_manager_.UpdateSettings(std::move(updated));
            NotifySettingsChanged("Quick access default saved");
            return;
        }
        case Action::ToggleNewTabContext: {
            auto updated = settings_manager_.Settings();
            updated.new_tab_show_context = !updated.new_tab_show_context;
            settings_manager_.UpdateSettings(std::move(updated));
            NotifySettingsChanged("Browser hints default saved");
            return;
        }
        case Action::ToggleRestoreSession:
            settings_manager_.SetRestoreSessionOnStartup(
                !settings_manager_.Settings().restore_session_on_startup);
            NotifySettingsChanged("Startup preference saved locally");
            return;

        case Action::SaveHomePage: {
            std::string value = home_page_field_ ? home_page_field_->GetText().ToString() : std::string{};
            if (value.empty()) {
                value = std::string(core::navigation::kNewTabUrl);
            }
            settings_manager_.SetHomePageUrl(std::move(value));
            NotifySettingsChanged("Home page saved locally");
            return;
        }

        case Action::SaveSearchProvider: {
            std::string name = search_provider_field_ ? search_provider_field_->GetText().ToString() : std::string{};
            std::string url = search_url_field_ ? search_url_field_->GetText().ToString() : std::string{};
            if (url.empty()) {
                SetStatus("Search URL cannot be empty");
                return;
            }
            if (name.empty()) {
                name = "Custom";
            }
            settings_manager_.SetSearchProvider(std::move(name), std::move(url));
            NotifySettingsChanged("Search provider saved locally");
            return;
        }

        case Action::SaveDownloadsDirectory: {
            std::string value = downloads_directory_field_
                ? downloads_directory_field_->GetText().ToString()
                : std::string{};
            settings_manager_.SetDownloadsDirectory(std::move(value));
            NotifySettingsChanged("Downloads directory saved locally");
            return;
        }

        case Action::ResetDefaults:
            settings_manager_.UpdateSettings(core::BrowserSettings{});
            NotifySettingsChanged("Default settings restored");
            return;

        case Action::Close:
            SetVisible(false);
            return;
    }
}

void SettingsPanel::NotifySettingsChanged(const char* status_text) {
    RefreshFromSettings();
    ApplyAuraPreferences(settings_manager_.Settings());
    SetStatus(status_text);
    if (on_settings_changed_) {
        on_settings_changed_(settings_manager_.Settings());
    }
    if (panel_) {
        panel_->Layout();
    }
}

void SettingsPanel::SetStatus(const char* text) {
    if (status_label_) {
        status_label_->SetText(text ? text : "");
    }
}

}  // namespace openbrowser::desktop
