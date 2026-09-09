#include "downloads_panel.h"

#include "deferred_ui_action.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_label_button.h"
#include "include/wrapper/cef_helpers.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#endif

namespace openbrowser::desktop {

class DownloadsPanel::ActionDelegate final : public CefButtonDelegate {
public:
    ActionDelegate(DownloadsPanel& panel, TransferAction action, std::string transfer_id = "")
        : panel_(panel), action_(action), transfer_id_(std::move(transfer_id)) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        DownloadsPanel* panel = &panel_;
        const TransferAction action = action_;
        const std::string transfer_id = transfer_id_;
        PostDeferredUiAction(panel_.alive_token_, [panel, action, transfer_id]() {
            panel->HandleAction(action, transfer_id);
        });
    }

private:
    DownloadsPanel& panel_;
    TransferAction action_;
    std::string transfer_id_;

    IMPLEMENT_REFCOUNTING(ActionDelegate);
};

namespace {

std::string FormatTransferState(const core::TransferState state) {
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

std::string RiskLabel(const core::FileRiskLevel risk) {
    switch (risk) {
    case core::FileRiskLevel::Safe:
        return {};
    case core::FileRiskLevel::CautionExecutable:
        return " [Warning: executable]";
    case core::FileRiskLevel::DangerousScript:
        return " [Warning: script]";
    }
    return {};
}

bool LaunchPath(const std::filesystem::path& path) {
#if defined(_WIN32)
    if (path.empty()) {
        return false;
    }
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
        nullptr,
        L"open",
        path.wstring().c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL));
    return result > 32;
#else
    (void)path;
    return false;
#endif
}

}  // namespace

DownloadsPanel::DownloadsPanel(
    core::TransferBroker& transfer_broker,
    core::FileBroker& file_broker)
    : transfer_broker_(transfer_broker), file_broker_(file_broker) {

    transfer_broker_.SetFileBroker(&file_broker_);
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
    *alive_token_ = false;
    transfer_broker_.RemoveObserver(this);
    transfer_broker_.SetFileBroker(nullptr);
}

CefRefPtr<CefPanel> DownloadsPanel::View() const noexcept {
    return panel_;
}

void DownloadsPanel::SetVisible(const bool visible) {
    const bool was_visible = IsVisible();
    panel_->SetVisible(visible);
    if (visible) {
        RebuildView();
    }
    if (was_visible != IsVisible() && on_visibility_changed_) {
        on_visibility_changed_(IsVisible());
    }
}

void DownloadsPanel::SetVisibilityChangedCallback(VisibilityChangedCallback callback) {
    on_visibility_changed_ = std::move(callback);
}

bool DownloadsPanel::IsVisible() const {
    return panel_->IsVisible();
}

void DownloadsPanel::ToggleVisibility() {
    SetVisible(!IsVisible());
}

void DownloadsPanel::HandleAction(const TransferAction action, const std::string& transfer_id) {
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
    case TransferAction::Open:
    case TransferAction::ShowInFolder: {
        const auto items = transfer_broker_.ListTransfers();
        const auto it = std::find_if(items.begin(), items.end(), [&](const core::TransferItem& item) {
            return item.id == transfer_id;
        });
        if (it == items.end() || it->state != core::TransferState::Completed || it->target_path.empty()) {
            break;
        }

        const std::filesystem::path target(it->target_path);
        if (!file_broker_.IsPathContained(target)) {
            break;
        }

        if (action == TransferAction::Open) {
            if (core::FileBroker::AssessRisk(target) == core::FileRiskLevel::Safe) {
                static_cast<void>(LaunchPath(target));
            }
        } else {
            static_cast<void>(LaunchPath(target.parent_path()));
        }
        break;
    }
    case TransferAction::Close:
        SetVisible(false);
        break;
    }
    RebuildView();
}

void DownloadsPanel::RebuildView() {
    panel_->RemoveAllChildViews();
    button_delegates_.clear();

    auto header = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings header_settings{};
    header_settings.horizontal = 1;
    header_settings.between_child_spacing = 8;
    header_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    header->SetToBoxLayout(header_settings);

    auto title_btn = CefLabelButton::CreateLabelButton(nullptr, "Downloads & Transfers");
    header->AddChildView(title_btn);

    auto close_delegate = new ActionDelegate(*this, TransferAction::Close);
    button_delegates_.push_back(close_delegate);
    auto close_btn = CefLabelButton::CreateLabelButton(close_delegate, "[Close]");
    header->AddChildView(close_btn);
    panel_->AddChildView(header);

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
            const std::filesystem::path risk_path = item.target_path.empty()
                ? std::filesystem::path(safe_name)
                : std::filesystem::path(item.target_path);
            const auto risk = core::FileBroker::AssessRisk(risk_path);
            const double pct = (item.total_bytes > 0)
                ? (static_cast<double>(item.received_bytes) * 100.0 / static_cast<double>(item.total_bytes))
                : 0.0;

            std::ostringstream oss;
            oss << safe_name << " - "
                << std::fixed << std::setprecision(1) << pct << "% ("
                << (item.speed_bytes_per_sec / 1024) << " KB/s) ["
                << FormatTransferState(item.state) << "]"
                << RiskLabel(risk);
            if (item.state == core::TransferState::Failed && !item.error_message.empty()) {
                oss << " Error: " << item.error_message;
            }

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
            } else if (item.state == core::TransferState::Completed && !item.target_path.empty()) {
#if defined(_WIN32)
                if (risk == core::FileRiskLevel::Safe) {
                    auto open_delegate = new ActionDelegate(*this, TransferAction::Open, item.id);
                    button_delegates_.push_back(open_delegate);
                    auto open_btn = CefLabelButton::CreateLabelButton(open_delegate, "[Open]");
                    row->AddChildView(open_btn);
                }

                auto folder_delegate = new ActionDelegate(*this, TransferAction::ShowInFolder, item.id);
                button_delegates_.push_back(folder_delegate);
                auto folder_btn = CefLabelButton::CreateLabelButton(folder_delegate, "[Show Folder]");
                row->AddChildView(folder_btn);
#endif
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
