#include "browser_chrome.h"

#include "cef_browser_engine.h"
#include "find_bar.h"
#include "core/capabilities/capability_policy.h"
#include "core/navigation/address_input.h"
#include "core/navigation/internal_urls.h"
#include "core/session/browser_session.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/wrapper/cef_helpers.h"

#include <memory>
#include <utility>

namespace openbrowser::desktop {
namespace {

std::string SecurityBadgeLabel(const bool private_mode, std::string_view state) {
    if (private_mode) {
        return "🕶 " + std::string(state);
    }
    return std::string(state);
}

class PassiveButtonDelegate final : public CefButtonDelegate {
public:
    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
    }

private:
    IMPLEMENT_REFCOUNTING(PassiveButtonDelegate);
};

}  // namespace

class BrowserChrome::ChromeButtonDelegate final : public CefButtonDelegate {
public:
    ChromeButtonDelegate(BrowserChrome& chrome, const ChromeAction action)
        : chrome_(chrome), action_(action) {}

    ChromeButtonDelegate(const ChromeButtonDelegate&) = delete;
    ChromeButtonDelegate& operator=(const ChromeButtonDelegate&) = delete;

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        chrome_.HandleAction(action_);
    }

private:
    BrowserChrome& chrome_;
    ChromeAction action_;

    IMPLEMENT_REFCOUNTING(ChromeButtonDelegate);
};

class BrowserChrome::AddressFieldDelegate final : public CefTextfieldDelegate {
public:
    explicit AddressFieldDelegate(BrowserChrome& chrome) : chrome_(chrome) {}

    AddressFieldDelegate(const AddressFieldDelegate&) = delete;
    AddressFieldDelegate& operator=(const AddressFieldDelegate&) = delete;

    bool OnKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event) override {
        CEF_REQUIRE_UI_THREAD();
        return chrome_.HandleAddressKeyEvent(std::move(textfield), event);
    }

    void OnAfterUserAction(CefRefPtr<CefTextfield> /*textfield*/) override {
        CEF_REQUIRE_UI_THREAD();
        chrome_.HandleAddressUserAction();
    }

    void OnBlur(CefRefPtr<CefView> /*view*/) override {
        CEF_REQUIRE_UI_THREAD();
        chrome_.HandleAddressBlur();
    }

private:
    BrowserChrome& chrome_;

    IMPLEMENT_REFCOUNTING(AddressFieldDelegate);
};

BrowserChrome::BrowserChrome(
    core::BrowserSession& session,
    CefRefPtr<CefBrowserEngine> engine,
    std::function<void()> on_toggle_network_lab,
    std::function<void()> on_toggle_command_palette,
    std::function<void()> on_toggle_bookmarks_bar,
    std::function<void()> on_toggle_downloads_panel,
    std::function<void()> on_toggle_profile)
    : session_(session),
      engine_(std::move(engine)),
      on_toggle_network_lab_(std::move(on_toggle_network_lab)),
      on_toggle_command_palette_(std::move(on_toggle_command_palette)),
      on_toggle_bookmarks_bar_(std::move(on_toggle_bookmarks_bar)),
      on_toggle_downloads_panel_(std::move(on_toggle_downloads_panel)),
      on_toggle_profile_(std::move(on_toggle_profile)) {
    session_.AddObserver(this);
    if (engine_) {
        engine_->AddPermissionPromptObserver(this);
        if (auto* policy = engine_->MutablePolicy(); policy != nullptr && !engine_->StorageRoot().empty()) {
            const auto permissions_path = engine_->StorageRoot() / "permissions.json";
            static_cast<void>(policy->LoadOriginRulesFromFile(permissions_path));
            policy->SetAutoSavePath(permissions_path);
        }
    }

    address_delegate_ = new AddressFieldDelegate(*this);

    auto make_delegate = [this](ChromeAction action) {
        CefRefPtr<CefButtonDelegate> del(new ChromeButtonDelegate(*this, action));
        button_delegates_.push_back(del);
        return del;
    };

    container_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings container_settings{};
    container_settings.horizontal = 0;
    container_settings.between_child_spacing = 1;
    container_layout_ = container_->SetToBoxLayout(container_settings);

    toolbar_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings toolbar_settings{};
    toolbar_settings.horizontal = 1;
    toolbar_settings.between_child_spacing = 6;
    toolbar_settings.inside_border_horizontal_spacing = 10;
    toolbar_settings.inside_border_vertical_spacing = 6;
    toolbar_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    toolbar_layout_ = toolbar_->SetToBoxLayout(toolbar_settings);

    back_button_ = CefLabelButton::CreateLabelButton(make_delegate(ChromeAction::Back), "←");
    forward_button_ = CefLabelButton::CreateLabelButton(make_delegate(ChromeAction::Forward), "→");
    reload_button_ = CefLabelButton::CreateLabelButton(make_delegate(ChromeAction::Reload), "↻");
    security_badge_ = CefLabelButton::CreateLabelButton(
        make_delegate(ChromeAction::ToggleSecurityDetails), "🔒");
    address_bar_ = CefTextfield::CreateTextfield(address_delegate_);
    address_bar_->SetPlaceholderText("Search or enter an address");
    bookmarks_button_ = CefLabelButton::CreateLabelButton(
        make_delegate(ChromeAction::ToggleBookmarksBar), "☆");
    downloads_button_ = CefLabelButton::CreateLabelButton(
        make_delegate(ChromeAction::ToggleDownloadsPanel), "↓");
    profile_button_ = CefLabelButton::CreateLabelButton(
        make_delegate(ChromeAction::ToggleProfile), "👤");
    palette_button_ = CefLabelButton::CreateLabelButton(
        make_delegate(ChromeAction::ToggleCommandPalette), "⋮");

    // Aura keeps the permanent chrome intentionally small. Find and zoom remain
    // first-class browser actions through shortcuts/Command Palette, but no
    // longer consume horizontal space beside the omnibox.
    toolbar_->AddChildView(back_button_);
    toolbar_layout_->SetFlexForView(back_button_, 0);
    toolbar_->AddChildView(forward_button_);
    toolbar_layout_->SetFlexForView(forward_button_, 0);
    toolbar_->AddChildView(reload_button_);
    toolbar_layout_->SetFlexForView(reload_button_, 0);
    toolbar_->AddChildView(security_badge_);
    toolbar_layout_->SetFlexForView(security_badge_, 0);
    toolbar_->AddChildView(address_bar_);
    toolbar_layout_->SetFlexForView(address_bar_, 1);
    toolbar_->AddChildView(bookmarks_button_);
    toolbar_layout_->SetFlexForView(bookmarks_button_, 0);
    toolbar_->AddChildView(downloads_button_);
    toolbar_layout_->SetFlexForView(downloads_button_, 0);
    toolbar_->AddChildView(profile_button_);
    toolbar_layout_->SetFlexForView(profile_button_, 0);
    toolbar_->AddChildView(palette_button_);
    toolbar_layout_->SetFlexForView(palette_button_, 0);

    container_->AddChildView(toolbar_);
    container_layout_->SetFlexForView(toolbar_, 0);

    prompt_panel_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings prompt_settings{};
    prompt_settings.horizontal = 1;
    prompt_settings.between_child_spacing = 6;
    prompt_settings.inside_border_horizontal_spacing = 8;
    prompt_settings.inside_border_vertical_spacing = 3;
    prompt_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    prompt_layout_ = prompt_panel_->SetToBoxLayout(prompt_settings);

    prompt_label_ = CefLabelButton::CreateLabelButton(
        new PassiveButtonDelegate(), "Permission request");
    prompt_label_->SetEnabled(false);
    allow_once_button_ = CefLabelButton::CreateLabelButton(
        make_delegate(ChromeAction::AllowPermissionOnce), "Allow once");
    always_allow_button_ = CefLabelButton::CreateLabelButton(
        make_delegate(ChromeAction::AlwaysAllowPermission), "Always allow");
    block_button_ = CefLabelButton::CreateLabelButton(
        make_delegate(ChromeAction::BlockPermission), "Block");

    prompt_panel_->AddChildView(prompt_label_);
    prompt_layout_->SetFlexForView(prompt_label_, 1);
    prompt_panel_->AddChildView(allow_once_button_);
    prompt_layout_->SetFlexForView(allow_once_button_, 0);
    prompt_panel_->AddChildView(always_allow_button_);
    prompt_layout_->SetFlexForView(always_allow_button_, 0);
    prompt_panel_->AddChildView(block_button_);
    prompt_layout_->SetFlexForView(block_button_, 0);

    prompt_panel_->SetVisible(false);
    container_->AddChildView(prompt_panel_);
    container_layout_->SetFlexForView(prompt_panel_, 0);

    security_details_panel_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings details_settings{};
    details_settings.horizontal = 1;
    details_settings.between_child_spacing = 6;
    details_settings.inside_border_horizontal_spacing = 8;
    details_settings.inside_border_vertical_spacing = 3;
    details_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    security_details_layout_ = security_details_panel_->SetToBoxLayout(details_settings);

    security_details_label_ = CefLabelButton::CreateLabelButton(
        new PassiveButtonDelegate(), "Site security");
    security_details_label_->SetEnabled(false);
    reset_permissions_button_ = CefLabelButton::CreateLabelButton(
        make_delegate(ChromeAction::ResetOriginPermissions), "Reset rules");

    security_details_panel_->AddChildView(security_details_label_);
    security_details_layout_->SetFlexForView(security_details_label_, 1);
    security_details_panel_->AddChildView(reset_permissions_button_);
    security_details_layout_->SetFlexForView(reset_permissions_button_, 0);

    security_details_panel_->SetVisible(false);
    container_->AddChildView(security_details_panel_);
    container_layout_->SetFlexForView(security_details_panel_, 0);

    find_bar_ = std::make_unique<FindBar>(session_, engine_);
    container_->AddChildView(find_bar_->View());
    container_layout_->SetFlexForView(find_bar_->View(), 0);

    SyncAddressFromSession(session_);
    UpdateZoomPresentation();
    UpdatePrivatePresentation();
}

BrowserChrome::~BrowserChrome() {
    if (engine_) {
        engine_->RemovePermissionPromptObserver(this);
    }
    session_.RemoveObserver(this);
}

CefRefPtr<CefPanel> BrowserChrome::View() const noexcept {
    return container_;
}

void BrowserChrome::OnBrowserSessionChanged(const core::BrowserSession& session) {
    CEF_REQUIRE_UI_THREAD();
    SyncAddressFromSession(session);
    UpdateZoomPresentation();

    // New Tab is a launch surface, not a destination the user should have to
    // erase from the omnibox. Keep the primary action ready for immediate use.
    const auto& active_id = session.ActiveTabId();
    if (active_id.has_value()) {
        const auto* tab = session.FindTab(*active_id);
        if (tab != nullptr) {
            const std::string& display_url =
                tab->pending_url.has_value() ? *tab->pending_url : tab->url;
            if (core::navigation::IsBlankOrNewTabUrl(display_url) && !address_editing_) {
                FocusAddressBar();
            }
        }
    }

    if (active_id.has_value() && engine_) {
        const auto prompt = engine_->FindPromptForTab(*active_id);
        if (prompt.has_value()) {
            ShowPrompt(*prompt);
        } else if (current_prompt_id_.has_value()) {
            current_prompt_id_.reset();
            prompt_panel_->SetVisible(false);
            container_->Layout();
        }
    }
}

void BrowserChrome::OnPermissionPromptRequested(const core::PermissionPrompt& prompt) {
    CEF_REQUIRE_UI_THREAD();
    const auto& active_id = session_.ActiveTabId();
    if (active_id.has_value() && *active_id == prompt.tab_id) {
        ShowPrompt(prompt);
    }
}

void BrowserChrome::OnPermissionPromptDismissed(const uint64_t prompt_id) {
    CEF_REQUIRE_UI_THREAD();
    if (current_prompt_id_.has_value() && *current_prompt_id_ == prompt_id) {
        current_prompt_id_.reset();
        prompt_panel_->SetVisible(false);
        container_->Layout();
    }
}

void BrowserChrome::ShowPrompt(const core::PermissionPrompt& prompt) {
    current_prompt_id_ = prompt.prompt_id;
    std::string caps_str;
    for (std::size_t i = 0; i < prompt.capabilities.size(); ++i) {
        if (i > 0) {
            caps_str += ", ";
        }
        caps_str += core::ToString(prompt.capabilities[i]);
    }

    const std::string prefix = private_mode_ ? "Private permission: " : "Permission: ";
    prompt_label_->SetText(prefix + prompt.origin + " wants access to: " + caps_str);
    always_allow_button_->SetEnabled(!private_mode_);
    block_button_->SetText(private_mode_ ? "Block once" : "Block");
    prompt_panel_->SetVisible(true);
    container_->Layout();
}

void BrowserChrome::UpdateSecurityDetails() {
    if (!show_security_details_) {
        security_details_panel_->SetVisible(false);
        return;
    }

    std::string text = "Site: " + (current_origin_.empty() ? "No origin" : current_origin_);
    if (private_mode_) {
        text += " | Private session: permission choices are not persisted";
    }
    if (engine_ && engine_->Policy() && !current_origin_.empty()) {
        const auto rules = engine_->Policy()->GetOriginRules(current_origin_);
        if (!rules.empty()) {
            text += " | Remembered origin rules: ";
            for (const auto& [cap, dec] : rules) {
                text += std::string(core::ToString(cap)) + ": " +
                    (dec == core::CapabilityDecision::Allow ? "Allow " :
                     (dec == core::CapabilityDecision::Deny ? "Deny " : "Ask "));
            }
        } else {
            text += " | Default permissions policy";
        }
    }
    security_details_label_->SetText(text);
    reset_permissions_button_->SetEnabled(!private_mode_ && !current_origin_.empty());
    security_details_panel_->SetVisible(true);
}

void BrowserChrome::HandleAction(const ChromeAction action) {
    CEF_REQUIRE_UI_THREAD();

    if (action == ChromeAction::OpenFindBar) {
        OpenFindBar();
        return;
    }

    if (action == ChromeAction::ToggleNetworkLab) {
        if (on_toggle_network_lab_) {
            on_toggle_network_lab_();
        }
        return;
    }

    if (action == ChromeAction::ToggleSecurityDetails) {
        show_security_details_ = !show_security_details_;
        UpdateSecurityDetails();
        container_->Layout();
        return;
    }

    if (action == ChromeAction::AllowPermissionOnce) {
        if (current_prompt_id_.has_value() && engine_) {
            engine_->RespondToPermission(*current_prompt_id_, core::PermissionResponse::Allow, false);
        }
        return;
    }

    if (action == ChromeAction::AlwaysAllowPermission) {
        if (!private_mode_ && current_prompt_id_.has_value() && engine_) {
            engine_->RespondToPermission(*current_prompt_id_, core::PermissionResponse::Allow, true);
        }
        return;
    }

    if (action == ChromeAction::BlockPermission) {
        if (current_prompt_id_.has_value() && engine_) {
            const bool remember = core::ShouldRememberPermissionForOrigin(true, private_mode_);
            engine_->RespondToPermission(*current_prompt_id_, core::PermissionResponse::Block, remember);
        }
        return;
    }

    if (action == ChromeAction::ResetOriginPermissions) {
        if (!private_mode_ && engine_ && engine_->MutablePolicy() && !current_origin_.empty()) {
            engine_->MutablePolicy()->ClearOriginRules(current_origin_);
            UpdateSecurityDetails();
            container_->Layout();
        }
        return;
    }

    if (action == ChromeAction::ToggleCommandPalette) {
        if (on_toggle_command_palette_) {
            on_toggle_command_palette_();
        }
        return;
    }

    if (action == ChromeAction::ToggleBookmarksBar) {
        if (on_toggle_bookmarks_bar_) {
            on_toggle_bookmarks_bar_();
        }
        return;
    }

    if (action == ChromeAction::ToggleDownloadsPanel) {
        if (on_toggle_downloads_panel_) {
            on_toggle_downloads_panel_();
        }
        return;
    }

    if (action == ChromeAction::ToggleProfile) {
        if (on_toggle_profile_) {
            on_toggle_profile_();
        }
        return;
    }

    const auto& active_id = session_.ActiveTabId();
    if (!active_id.has_value()) {
        return;
    }

    switch (action) {
        case ChromeAction::Back:
            static_cast<void>(session_.GoBack(*active_id));
            break;
        case ChromeAction::Forward:
            static_cast<void>(session_.GoForward(*active_id));
            break;
        case ChromeAction::Reload:
            static_cast<void>(session_.Reload(*active_id));
            break;
        case ChromeAction::ZoomOut:
            if (engine_) {
                static_cast<void>(engine_->ZoomOut(*active_id));
                UpdateZoomPresentation();
            }
            break;
        case ChromeAction::ResetZoom:
            if (engine_) {
                static_cast<void>(engine_->ResetZoom(*active_id));
                UpdateZoomPresentation();
            }
            break;
        case ChromeAction::ZoomIn:
            if (engine_) {
                static_cast<void>(engine_->ZoomIn(*active_id));
                UpdateZoomPresentation();
            }
            break;
        default:
            break;
    }
}

void BrowserChrome::SetProfileLabel(const std::string& label) {
    private_mode_ = label.find("Private") != std::string::npos;
    if (profile_button_) {
        profile_button_->SetText(label);
    }
    UpdatePrivatePresentation();
    SyncAddressFromSession(session_);
    UpdateSecurityDetails();
    if (container_) {
        container_->Layout();
    }
}

void BrowserChrome::SetPrivateMode(const bool enabled) {
    CEF_REQUIRE_UI_THREAD();
    private_mode_ = enabled;
    UpdatePrivatePresentation();
    SyncAddressFromSession(session_);
    UpdateSecurityDetails();
    if (container_) {
        container_->Layout();
    }
}

void BrowserChrome::UpdatePrivatePresentation() {
    if (profile_button_) {
        profile_button_->SetText(private_mode_ ? "🕶" : "👤");
    }
    if (address_bar_) {
        address_bar_->SetPlaceholderText(
            private_mode_ ? "Private — search or enter an address" : "Search or enter an address");
    }
    if (always_allow_button_) {
        always_allow_button_->SetEnabled(!private_mode_);
    }
    if (block_button_) {
        block_button_->SetText(private_mode_ ? "Block once" : "Block");
    }
    if (reset_permissions_button_) {
        reset_permissions_button_->SetEnabled(!private_mode_ && !current_origin_.empty());
    }
    if (toolbar_) {
        toolbar_->InvalidateLayout();
    }
}

void BrowserChrome::SetSearchProvider(core::navigation::SearchProvider provider) {
    search_provider_ = std::move(provider);
}

bool BrowserChrome::HandleAddressKeyEvent(
    CefRefPtr<CefTextfield> textfield,
    const CefKeyEvent& event) {
    CEF_REQUIRE_UI_THREAD();
    if (!textfield) {
        return false;
    }

    if (event.windows_key_code == 13) {
        if (event.type == KEYEVENT_RAWKEYDOWN || event.type == KEYEVENT_KEYDOWN) {
            const auto text = textfield->GetText().ToString();
            const auto resolved = core::navigation::ResolveAddressInput(text, search_provider_);
            address_editing_ = false;

            if (resolved.has_value()) {
                const auto& active_id = session_.ActiveTabId();
                if (active_id.has_value()) {
                    static_cast<void>(session_.Navigate(*active_id, *resolved));
                }
            } else {
                SyncAddressFromSession(session_);
            }
            return true;
        }
        if (event.type == KEYEVENT_CHAR) {
            return true;
        }
    }

    if (event.windows_key_code == 27) {
        if (event.type == KEYEVENT_RAWKEYDOWN || event.type == KEYEVENT_KEYDOWN) {
            address_editing_ = false;
            SyncAddressFromSession(session_);
            return true;
        }
        if (event.type == KEYEVENT_CHAR) {
            return true;
        }
    }

    return false;
}

void BrowserChrome::HandleAddressUserAction() {
    CEF_REQUIRE_UI_THREAD();
    address_editing_ = true;
}

void BrowserChrome::HandleAddressBlur() {
    CEF_REQUIRE_UI_THREAD();
    address_editing_ = false;
    SyncAddressFromSession(session_);
}

void BrowserChrome::FocusAddressBar() {
    CEF_REQUIRE_UI_THREAD();
    if (address_bar_) {
        address_bar_->RequestFocus();
        address_bar_->SelectAll(false);
    }
}

void BrowserChrome::OpenFindBar() {
    CEF_REQUIRE_UI_THREAD();
    if (find_bar_) {
        find_bar_->Open();
    }
}

void BrowserChrome::UpdateZoomPresentation() {
    CEF_REQUIRE_UI_THREAD();

    int percent = 100;
    const auto& active_id = session_.ActiveTabId();
    const bool has_active_tab = active_id.has_value();
    if (has_active_tab && engine_) {
        percent = engine_->ZoomPercent(*active_id);
    }

    if (zoom_reset_button_) {
        zoom_reset_button_->SetText(std::to_string(percent) + "%");
        zoom_reset_button_->SetEnabled(has_active_tab);
    }
    if (zoom_out_button_) {
        zoom_out_button_->SetEnabled(has_active_tab && percent > 25);
    }
    if (zoom_in_button_) {
        zoom_in_button_->SetEnabled(has_active_tab && percent < 500);
    }
    if (toolbar_) {
        toolbar_->InvalidateLayout();
    }
}

void BrowserChrome::SyncAddressFromSession(const core::BrowserSession& session) {
    CEF_REQUIRE_UI_THREAD();
    if (!address_bar_ || address_editing_) {
        return;
    }

    const auto& active_id = session.ActiveTabId();
    if (!active_id.has_value()) {
        address_bar_->SetText("");
        current_origin_.clear();
        security_badge_->SetVisible(false);
        if (reload_button_) {
            reload_button_->SetText("↻");
        }
        UpdateSecurityDetails();
        return;
    }

    const auto* tab = session.FindTab(*active_id);
    if (tab == nullptr) {
        address_bar_->SetText("");
        current_origin_.clear();
        security_badge_->SetVisible(false);
        if (reload_button_) {
            reload_button_->SetText("↻");
        }
        UpdateSecurityDetails();
        return;
    }

    const bool is_loading = (tab->navigation_state == core::NavigationState::Loading);
    if (reload_button_) {
        reload_button_->SetText(is_loading ? "⏳" : "↻");
    }

    const std::string& display_url = tab->pending_url.has_value() ? *tab->pending_url : tab->url;

    // Internal launch surfaces are browser chrome, not a web destination. Keep
    // their implementation URL out of the user's search field and reclaim the
    // security-badge slot for a wider omnibox.
    if (core::navigation::IsBlankOrNewTabUrl(display_url)) {
        address_bar_->SetText("");
        current_origin_.clear();
        security_badge_->SetVisible(false);
        UpdateSecurityDetails();
        if (toolbar_) {
            toolbar_->InvalidateLayout();
        }
        return;
    }

    security_badge_->SetVisible(true);
    address_bar_->SetText(display_url);

    current_origin_ = core::CapabilityPolicy::ExtractOrigin(display_url).value_or("");
    if (display_url.rfind("https://", 0) == 0) {
        security_badge_->SetText(SecurityBadgeLabel(private_mode_, "🔒"));
    } else if (display_url.rfind("http://", 0) == 0) {
        security_badge_->SetText(SecurityBadgeLabel(private_mode_, "⚠"));
    } else if (display_url.rfind("about:", 0) == 0) {
        security_badge_->SetText(SecurityBadgeLabel(private_mode_, "⚙"));
    } else {
        security_badge_->SetText(SecurityBadgeLabel(private_mode_, "🌐"));
    }

    UpdateSecurityDetails();
}

}  // namespace openbrowser::desktop
