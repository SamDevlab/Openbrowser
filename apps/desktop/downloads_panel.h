#pragma once

#include "core/transfers/file_broker.h"
#include "core/transfers/transfer_broker.h"

#include "include/cef_base.h"
#include "include/views/cef_button.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_panel.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class CefBoxLayout;
class CefLabelButton;

namespace openbrowser::desktop {

class DownloadsPanel final : public core::TransferObserver {
public:
    using VisibilityChangedCallback = std::function<void(bool)>;
    DownloadsPanel(
        core::TransferBroker& transfer_broker,
        core::FileBroker& file_broker);
    ~DownloadsPanel() override;

    DownloadsPanel(const DownloadsPanel&) = delete;
    DownloadsPanel& operator=(const DownloadsPanel&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    void SetVisibilityChangedCallback(VisibilityChangedCallback callback);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();

    void RebuildView();

    void OnTransferStarted(const core::TransferItem& item) override;
    void OnTransferUpdated(const core::TransferItem& item) override;
    void OnTransferFinished(const core::TransferItem& item) override;

private:
    enum class TransferAction {
        Pause,
        Resume,
        Cancel,
        Open,
        ShowInFolder,
        Close
    };

    class ActionDelegate;

    void HandleAction(TransferAction action, const std::string& transfer_id = "");

    core::TransferBroker& transfer_broker_;
    core::FileBroker& file_broker_;
    std::shared_ptr<bool> alive_token_{std::make_shared<bool>(true)};

    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    CefRefPtr<CefPanel> list_panel_;
    CefRefPtr<CefBoxLayout> list_layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> button_delegates_;
    VisibilityChangedCallback on_visibility_changed_;
};

} // namespace openbrowser::desktop
