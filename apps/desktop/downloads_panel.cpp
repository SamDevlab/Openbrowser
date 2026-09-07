#include "downloads_panel.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_label_button.h"
#include "include/wrapper/cef_helpers.h"

#include <iomanip>
#include <sstream>

namespace openbrowser::desktop {

class DownloadsPanel::ActionDelegate final : public CefButtonDelegate {
public:
    ActionDelegate(DownloadsPanel& panel, TransferAction action, uint64_t transfer_id = 0)
        : panel_(panel), action_(action), transfer_id_(transfer_id) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        panel_.HandleAction(action_, transfer_id_);
    }

private:
    DownloadsPanel& panel_;
    TransferAction action_;
    uint64_t transfer_id_;

    IMPLEMENT_REFCOUNTING(ActionDelegate);
};

DownloadsPanel::DownloadsPanel(
    core::TransferBroker& transfer_broker,
    core::FileBroker& file_broker)
    : transfer_broker_(transfer_broker), file_broker_(file_broker) {

    transfer_broker_.AddObserver(this);

    panel_ = CefPanel::CreatePanel(nullptr);

    CefBoxLayoutSettings settings{};
    settings.horizontal = 0;
    settings.between_child_spacing = 4;
    settings.inside_border_horizontal_spacing = 10;
    settings.inside_border_vertical_spacing = 6;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    layout_ = panel_->SetAsBoxLayout(settings);

    list_panel_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings list_settings{};
    list_settings.horizontal = 0;
    list_settings.between_child_spacing = 4;
    list_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    list_layout_ = list_panel_->SetAsBoxLayout(list_settings);

    panel_->SetVisible(false);
    RebuildView();
}

DownloadsPanel::~DownloadsPanel() {
    transfer_broker_.RemoveObserver(this);
}

CefRefPtr<CefPanel> DownloadsPanel::View() const noexcept {
    return panel_;
}

void DownloadsPanel::SetVisible(bool visible) {
    panel_->SetVisible(visible);
    if (visible) {
        RebuildView();
    }
}

bool DownloadsPanel::IsVisible() const {
    return panel_->IsVisible();
}

void DownloadsPanel::ToggleVisibility() {
    SetVisible(!IsVisible());
}

void DownloadsPanel::HandleAction(TransferAction action, uint64_t transfer_id) {
    switch (action) {
    case TransferAction::Pause:
        transfer_broker_.PauseTransfer(transfer_id);
        break;
    case TransferAction::Resume:
        transfer_broker_.ResumeTransfer(transfer_id);
        break;
    case TransferAction::Cancel:
        transfer_broker_.CancelTransfer(transfer_id);
        break;
    case TransferAction::Close:
        SetVisible(false);
        break;
    }
    RebuildView();
}

void DownloadsPanel::RebuildView() {
    panel_->RemoveAllChildViews();
    button_delegates_.clear();

    // Header bar
    auto header = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings header_settings{};
    header_settings.horizontal = 1;
    header_settings.between_child_spacing = 8;
    header_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    header->SetAsBoxLayout(header_settings);

    auto title_btn = CefLabelButton::CreateLabelButton(nullptr, "📥 Downloads & Transfers");
    header->AddChildView(title_btn);

    auto close_delegate = new ActionDelegate(*this, TransferAction::Close);
    button_delegates_.push_back(close_delegate);
    auto close_btn = CefLabelButton::CreateLabelButton(close_delegate, "[✕ Close]");
    header->AddChildView(close_btn);
    panel_->AddChildView(header);

    // List of items
    const auto items = transfer_broker_.ListTransfers();
    if (items.empty()) {
        auto empty_label = CefLabelButton::CreateLabelButton(nullptr, "No active or recent downloads.");
        panel_->AddChildView(empty_label);
    } else {
        for (const auto& item : items) {
            auto row = CefPanel::CreatePanel(nullptr);
            CefBoxLayoutSettings row_settings{};
            row_settings.horizontal = 1;
            row_settings.between_child_spacing = 6;
            row_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
            row->SetAsBoxLayout(row_settings);

            const std::string safe_name = core::FileBroker::SanitizeFilename(item.suggested_filename);
            std::ostringstream oss;
            oss << safe_name << " - "
                << std::fixed << std::setprecision(1) << item.ProgressPercentage() << "% ("
                << (item.speed_bytes_per_sec / 1024) << " KB/s) ["
                << core::TransferStateToString(item.state) << "]";

            auto item_label = CefLabelButton::CreateLabelButton(nullptr, oss.str());
            row->AddChildView(item_label);

            if (item.state == core::TransferState::Downloading) {
                auto pause_delegate = new ActionDelegate(*this, TransferAction::Pause, item.id);
                button_delegates_.push_back(pause_delegate);
                auto pause_btn = CefLabelButton::CreateLabelButton(pause_delegate, "[Pause]");
                row->AddChildView(pause_btn);

                auto cancel_delegate = new ActionDelegate(*this, TransferAction::Cancel, item.id);
                button_delegates_.push_back(cancel_delegate);
                auto cancel_btn = CefLabelButton::CreateLabelButton(cancel_delegate, "[Cancel]");
                row->AddChildView(cancel_btn);
            } else if (item.state == core::TransferState::Paused) {
                auto resume_delegate = new ActionDelegate(*this, TransferAction::Resume, item.id);
                button_delegates_.push_back(resume_delegate);
                auto resume_btn = CefLabelButton::CreateLabelButton(resume_delegate, "[Resume]");
                row->AddChildView(resume_btn);

                auto cancel_delegate = new ActionDelegate(*this, TransferAction::Cancel, item.id);
                button_delegates_.push_back(cancel_delegate);
                auto cancel_btn = CefLabelButton::CreateLabelButton(cancel_delegate, "[Cancel]");
                row->AddChildView(cancel_btn);
            }

            panel_->AddChildView(row);
        }
    }

    panel_->InvalidateLayout();
}

void DownloadsPanel::OnTransferStarted(const core::TransferItem& /*item*/) {
    if (IsVisible()) {
        RebuildView();
    }
}

void DownloadsPanel::OnTransferProgress(const core::TransferItem& /*item*/) {
    if (IsVisible()) {
        RebuildView();
    }
}

void DownloadsPanel::OnTransferCompleted(const core::TransferItem& /*item*/) {
    if (IsVisible()) {
        RebuildView();
    }
}

void DownloadsPanel::OnTransferFailed(const core::TransferItem& /*item*/, const std::string& /*error*/) {
    if (IsVisible()) {
        RebuildView();
    }
}

} // namespace openbrowser::desktop
