#include "browser_chrome.h"

#include "core/navigation/address_input.h"
#include "core/session/browser_session.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/wrapper/cef_helpers.h"

#include <utility>

namespace openbrowser::desktop {

class BrowserChrome::NavigationButtonDelegate final : public CefButtonDelegate {
public:
    NavigationButtonDelegate(BrowserChrome& chrome, const NavigationAction action)
        : chrome_(chrome), action_(action) {}

    NavigationButtonDelegate(const NavigationButtonDelegate&) = delete;
    NavigationButtonDelegate& operator=(const NavigationButtonDelegate&) = delete;

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        chrome_.HandleNavigationAction(action_);
    }

private:
    BrowserChrome& chrome_;
    NavigationAction action_;

    IMPLEMENT_REFCOUNTING(NavigationButtonDelegate);
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
    std::function<void()> on_toggle_network_lab)
    : session_(session), on_toggle_network_lab_(std::move(on_toggle_network_lab)) {
    session_.AddObserver(this);

    back_delegate_ = new NavigationButtonDelegate(*this, NavigationAction::Back);
    forward_delegate_ = new NavigationButtonDelegate(*this, NavigationAction::Forward);
    reload_delegate_ = new NavigationButtonDelegate(*this, NavigationAction::Reload);
    lab_delegate_ = new NavigationButtonDelegate(*this, NavigationAction::ToggleNetworkLab);
    address_delegate_ = new AddressFieldDelegate(*this);

    toolbar_ = CefPanel::CreatePanel(nullptr);

    CefBoxLayoutSettings settings{};
    settings.horizontal = 1;
    settings.between_child_spacing = 6;
    settings.inside_border_horizontal_spacing = 8;
    settings.inside_border_vertical_spacing = 4;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    layout_ = toolbar_->SetToBoxLayout(settings);

    back_button_ = CefLabelButton::CreateLabelButton(back_delegate_, "Back");
    forward_button_ = CefLabelButton::CreateLabelButton(forward_delegate_, "Forward");
    reload_button_ = CefLabelButton::CreateLabelButton(reload_delegate_, "Reload");
    address_bar_ = CefTextfield::CreateTextfield(address_delegate_);
    address_bar_->SetPlaceholderText("Search or enter address");
    lab_button_ = CefLabelButton::CreateLabelButton(lab_delegate_, "Lab");

    toolbar_->AddChildView(back_button_);
    layout_->SetFlexForView(back_button_, 0);

    toolbar_->AddChildView(forward_button_);
    layout_->SetFlexForView(forward_button_, 0);

    toolbar_->AddChildView(reload_button_);
    layout_->SetFlexForView(reload_button_, 0);

    toolbar_->AddChildView(address_bar_);
    layout_->SetFlexForView(address_bar_, 1);

    toolbar_->AddChildView(lab_button_);
    layout_->SetFlexForView(lab_button_, 0);

    SyncAddressFromSession(session_);
}

BrowserChrome::~BrowserChrome() {
    session_.RemoveObserver(this);
}

CefRefPtr<CefPanel> BrowserChrome::View() const noexcept {
    return toolbar_;
}

void BrowserChrome::OnBrowserSessionChanged(const core::BrowserSession& session) {
    CEF_REQUIRE_UI_THREAD();
    SyncAddressFromSession(session);
}

void BrowserChrome::HandleNavigationAction(const NavigationAction action) {
    CEF_REQUIRE_UI_THREAD();
    if (action == NavigationAction::ToggleNetworkLab) {
        if (on_toggle_network_lab_) {
            on_toggle_network_lab_();
        }
        return;
    }

    const auto& active_id = session_.ActiveTabId();
    if (!active_id.has_value()) {
        return;
    }

    switch (action) {
        case NavigationAction::Back:
            static_cast<void>(session_.GoBack(*active_id));
            break;
        case NavigationAction::Forward:
            static_cast<void>(session_.GoForward(*active_id));
            break;
        case NavigationAction::Reload:
            static_cast<void>(session_.Reload(*active_id));
            break;
        case NavigationAction::ToggleNetworkLab:
            break;
    }
}

bool BrowserChrome::HandleAddressKeyEvent(
    CefRefPtr<CefTextfield> textfield,
    const CefKeyEvent& event) {
    CEF_REQUIRE_UI_THREAD();
    if (!textfield) {
        return false;
    }

    // VK_RETURN = 13 (Enter)
    if (event.windows_key_code == 13) {
        if (event.type == KEYEVENT_RAWKEYDOWN || event.type == KEYEVENT_KEYDOWN) {
            const auto text = textfield->GetText().ToString();
            const auto normalized = core::navigation::NormalizeAddressInput(text);
            address_editing_ = false;

            if (normalized.has_value()) {
                const auto& active_id = session_.ActiveTabId();
                if (active_id.has_value()) {
                    static_cast<void>(session_.Navigate(*active_id, *normalized));
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

    // VK_ESCAPE = 27 (Escape)
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

void BrowserChrome::SyncAddressFromSession(const core::BrowserSession& session) {
    CEF_REQUIRE_UI_THREAD();
    if (!address_bar_ || address_editing_) {
        return;
    }

    const auto& active_id = session.ActiveTabId();
    if (!active_id.has_value()) {
        address_bar_->SetText("");
        return;
    }

    const auto* tab = session.FindTab(*active_id);
    if (tab == nullptr) {
        address_bar_->SetText("");
        return;
    }

    const std::string& display_url = tab->pending_url.has_value() ? *tab->pending_url : tab->url;
    address_bar_->SetText(display_url);
}

}  // namespace openbrowser::desktop
