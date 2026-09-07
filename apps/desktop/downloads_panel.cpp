#include "downloads_panel.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_label_button.h"
#include "include/wrapper/cef_helpers.h"

#include <iomanip>
#include <sstream>

namespace openbrowser::desktop {

class DownloadsPanel::ActionDelegate final : public CefButtonDelegate {
public:
    ActionDelegate(DownloadsPanel& panel, TransferAction action, std::string transfer_id = "")
        : panel_(panel), action_(action), transfer_id_(std::move(transfer_id)) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        panel_.HandleAction(action_, transfer_id_);
    }

private:
    DownloadsPanel& panel_;
    TransferAction action_;
    std::string transfer_id_;

    IMPLEMENT_REFCOUNTING(ActionDelegate);
};

namespace {
std::string FormatTransferState(core::TransferState state) {
    switch (state) {
    case core::TransferState::Queued:
        return "Queued";
    case core::TransferState::InProgress:
        return "Downloading";
    case core::TransferState::Paused:
        return "Paused";
    case core::TransferState::Completed:
        return "Completed";
    case core::TransferState::Failed:
        return "Failed";
    case core::TransferState::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}
} // namespace

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
    layout_ = panel_->SetToBoxLayout(settings);

    list_panel_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings list_settings{};
    list_settings.horizontal = 0;
    list_settings.between_child_spacing = 4;
    list_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    list_layout_ = list_panel_->SetToBoxLayout(list_settings);

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

void DownloadsPanel::HandleAction(TransferAction action, const std::string& transfer_id) {
    switch (action) {
    case TransferAction::Pause:
        static_cast<void>(transfer_broker_.PauseTransfer(transfer_id));
        break;
    case TransferAction::Resume:
        static_cast<void>(transfer_broker_.ResumeTransfer(transfer_id));
        break;
    case TransferAction::Cancel:
        static_cast<void>(transfer_broker_.CancelTransfer(transfer_id));
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
    header->SetToBoxLayout(header_settings);

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
            row->SetToBoxLayout(row_settings);

            const std::string safe_name = core::FileBroker::SanitizeFilename(item.suggested_filename);
            const double pct = (item.total_bytes > 0)
                ? (static_cast<double>(item.received_bytes) * 100.0 / static_cast<double>(item.total_bytes))
                : 0.0;
            std::ostringstream oss;
            oss << safe_name << " - "
                << std::fixed << std::setprecision(1) << pct << "% ("
                << (item.speed_bytes_per_sec / 1024) << " KB/s) ["
                << FormatTransferState(item.state) << "]";

            auto item_label = CefLabelButton::CreateLabelButton(nullptr, oss.str());
            row->AddChildView(item_label);

            if (item.state == core::TransferState::InProgress) {
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

void DownloadsPanel::OnTransferUpdated(const core::TransferItem& /*item*/) {
    if (IsVisible()) {
        RebuildView();
    }
}

void DownloadsPanel::OnTransferFinished(const core::TransferItem& /*item*/) {
    if (IsVisible()) {
        RebuildView();
    }
}

} // namespace openbrowser::desktop
