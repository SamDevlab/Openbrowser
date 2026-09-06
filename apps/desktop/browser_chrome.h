#pragma once

#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield_delegate.h"

namespace openbrowser::core {
class BrowserSession;
}

class CefBoxLayout;
class CefLabelButton;
class CefTextfield;

namespace openbrowser::desktop {

class BrowserChrome final : public core::BrowserSessionObserver {
public:
    explicit BrowserChrome(core::BrowserSession& session);
    ~BrowserChrome() override;

    BrowserChrome(const BrowserChrome&) = delete;
    BrowserChrome& operator=(const BrowserChrome&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    enum class NavigationAction {
        Back,
        Forward,
        Reload,
    };

    class NavigationButtonDelegate;
    class AddressFieldDelegate;

    void HandleNavigationAction(NavigationAction action);
    bool HandleAddressKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event);
    void HandleAddressUserAction();
    void HandleAddressBlur();
    void SyncAddressFromSession(const core::BrowserSession& session);

    core::BrowserSession& session_;
    CefRefPtr<CefPanel> toolbar_;
    CefRefPtr<CefBoxLayout> layout_;
    CefRefPtr<CefLabelButton> back_button_;
    CefRefPtr<CefLabelButton> forward_button_;
    CefRefPtr<CefLabelButton> reload_button_;
    CefRefPtr<CefTextfield> address_bar_;
    CefRefPtr<CefButtonDelegate> back_delegate_;
    CefRefPtr<CefButtonDelegate> forward_delegate_;
    CefRefPtr<CefButtonDelegate> reload_delegate_;
    CefRefPtr<CefTextfieldDelegate> address_delegate_;
    bool address_editing_{false};
};

}  // namespace openbrowser::desktop
