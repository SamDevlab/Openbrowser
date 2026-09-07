#pragma once

#include "core/transfers/file_broker.h"
#include "core/transfers/transfer_broker.h"
#include "core/transfers/transfer_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_panel.h"

#include <memory>
#include <string>
#include <vector>

class CefBoxLayout;
class CefLabelButton;

namespace openbrowser::desktop {

class DownloadsPanel final : public core::TransferObserver {
public:
    DownloadsPanel(
        core::TransferBroker& transfer_broker,
        core::FileBroker& file_broker);
    ~DownloadsPanel() override;

    DownloadsPanel(const DownloadsPanel&) = delete;
    DownloadsPanel& operator=(const DownloadsPanel&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();

    void RebuildView();

    // TransferObserver
    void OnTransferStarted(const core::TransferItem& item) override;
    void OnTransferProgress(const core::TransferItem& item) override;
    void OnTransferCompleted(const core::TransferItem& item) override;
    void OnTransferFailed(const core::TransferItem& item, const std::string& error) override;

private:
    enum class TransferAction {
        Pause,
        Resume,
        Cancel,
        Close
    };

    class ActionDelegate;

    void HandleAction(TransferAction action, uint64_t transfer_id = 0);

    core::TransferBroker& transfer_broker_;
    core::FileBroker& file_broker_;

    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    CefRefPtr<CefPanel> list_panel_;
    CefRefPtr<CefBoxLayout> list_layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> button_delegates_;
};

} // namespace openbrowser::desktop
