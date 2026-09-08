#pragma once

#include "core/capabilities/permission_request.h"
#include "core/navigation/address_input.h"
#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_button.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield_delegate.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace openbrowser::core {
class BrowserSession;
}

class CefBoxLayout;
class CefLabelButton;
class CefTextfield;

namespace openbrowser::desktop {

class CefBrowserEngine;
class FindBar;

class BrowserChrome final : public core::BrowserSessionObserver,
                            public core::PermissionPromptObserver {
public:
    explicit BrowserChrome(
        core::BrowserSession& session,
        CefRefPtr<CefBrowserEngine> engine,
        std::function<void()> on_toggle_network_lab = nullptr,
        std::function<void()> on_toggle_command_palette = nullptr,
        std::function<void()> on_toggle_bookmarks_bar = nullptr,
        std::function<void()> on_toggle_downloads_panel = nullptr,
        std::function<void()> on_toggle_profile = nullptr);
    ~BrowserChrome() override;

    BrowserChrome(const BrowserChrome&) = delete;
    BrowserChrome& operator=(const BrowserChrome&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

    void OnPermissionPromptRequested(const core::PermissionPrompt& prompt) override;
    void OnPermissionPromptDismissed(uint64_t prompt_id) override;

    void SetProfileLabel(const std::string& label);
    void SetPrivateMode(bool enabled);
    void FocusAddressBar();
    void OpenFindBar();
    void RefreshZoomPresentation();
    void SetSearchProvider(core::navigation::SearchProvider provider);

private:
    enum class ChromeAction {
        Back,
        Forward,
        Reload,
        OpenFindBar,
        ZoomOut,
        ResetZoom,
        ZoomIn,
        ToggleNetworkLab,
        ToggleSecurityDetails,
        AllowPermissionOnce,
        AlwaysAllowPermission,
        BlockPermission,
        ResetOriginPermissions,
        ToggleCommandPalette,
        ToggleBookmarksBar,
        ToggleDownloadsPanel,
        ToggleProfile,
    };

    class ChromeButtonDelegate;
    class AddressFieldDelegate;

    void HandleAction(ChromeAction action);
    bool HandleAddressKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event);
    void HandleAddressUserAction();
    void HandleAddressBlur();
    void SyncAddressFromSession(const core::BrowserSession& session);
    void ShowPrompt(const core::PermissionPrompt& prompt);
    void UpdateSecurityDetails();
    void UpdatePrivatePresentation();
    void UpdateZoomPresentation();

    core::BrowserSession& session_;
    CefRefPtr<CefBrowserEngine> engine_;
    std::function<void()> on_toggle_network_lab_;
    std::function<void()> on_toggle_command_palette_;
    std::function<void()> on_toggle_bookmarks_bar_;
    std::function<void()> on_toggle_downloads_panel_;
    std::function<void()> on_toggle_profile_;

    CefRefPtr<CefPanel> container_;
    CefRefPtr<CefBoxLayout> container_layout_;

    CefRefPtr<CefPanel> toolbar_;
    CefRefPtr<CefBoxLayout> toolbar_layout_;
    CefRefPtr<CefLabelButton> back_button_;
    CefRefPtr<CefLabelButton> forward_button_;
    CefRefPtr<CefLabelButton> reload_button_;
    CefRefPtr<CefLabelButton> security_badge_;
    CefRefPtr<CefTextfield> address_bar_;
    CefRefPtr<CefLabelButton> find_button_;
    CefRefPtr<CefLabelButton> zoom_out_button_;
    CefRefPtr<CefLabelButton> zoom_reset_button_;
    CefRefPtr<CefLabelButton> zoom_in_button_;
    CefRefPtr<CefLabelButton> palette_button_;
    CefRefPtr<CefLabelButton> bookmarks_button_;
    CefRefPtr<CefLabelButton> downloads_button_;
    CefRefPtr<CefLabelButton> profile_button_;
    CefRefPtr<CefLabelButton> lab_button_;

    // Permission Prompt Banner
    CefRefPtr<CefPanel> prompt_panel_;
    CefRefPtr<CefBoxLayout> prompt_layout_;
    CefRefPtr<CefLabelButton> prompt_label_;
    CefRefPtr<CefLabelButton> allow_once_button_;
    CefRefPtr<CefLabelButton> always_allow_button_;
    CefRefPtr<CefLabelButton> block_button_;

    // Security Details Inspector
    CefRefPtr<CefPanel> security_details_panel_;
    CefRefPtr<CefBoxLayout> security_details_layout_;
    CefRefPtr<CefLabelButton> security_details_label_;
    CefRefPtr<CefLabelButton> reset_permissions_button_;

    std::unique_ptr<FindBar> find_bar_;

    std::vector<CefRefPtr<CefButtonDelegate>> button_delegates_;
    CefRefPtr<CefTextfieldDelegate> address_delegate_;

    std::optional<uint64_t> current_prompt_id_;
    bool address_editing_{false};
    bool show_security_details_{false};
    bool private_mode_{false};
    std::string current_origin_;
    core::navigation::SearchProvider search_provider_;
};

}  // namespace openbrowser::desktop
