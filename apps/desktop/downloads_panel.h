#pragma once

#include "core/transfers/file_broker.h"
#include "core/transfers/transfer_broker.h"

#include "include/cef_base.h"
#include "include/views/cef_button.h"
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

    class PanelActionDelegate;

    void HandleTransferAction(TransferAction action, const std::string& transfer_id = {});
    [[nodiscard]] std::string FormatStatus(const core::TransferItem& item) const;
    [[nodiscard]] static std::string FormatBytes(std::uint64_t bytes);
    [[nodiscard]] static std::string FormatRate(double bytes_per_second);

    core::TransferBroker& transfer_broker_;
    core::FileBroker& file_broker_;

    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    std::vector<CefRefPtr<CefButtonDelegate>> delegates_;
};

}  // namespace openbrowser::desktop
