#include "command_palette_overlay.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_textfield.h"
#include "include/wrapper/cef_helpers.h"

namespace openbrowser::desktop {

class CommandPaletteOverlay::QueryFieldDelegate final : public CefTextfieldDelegate {
public:
    explicit QueryFieldDelegate(CommandPaletteOverlay& overlay)
        : overlay_(overlay) {}

    void OnContentsChanged(CefRefPtr<CefTextfield> textfield) override {
        CEF_REQUIRE_UI_THREAD();
        overlay_.SetQuery(textfield->GetText().ToString());
    }

    bool OnKeyEvent(
        CefRefPtr<CefTextfield> /*textfield*/,
        const CefKeyEvent& event) override {
        CEF_REQUIRE_UI_THREAD();
        if (event.type == KEYEVENT_RAWKEYDOWN || event.type == KEYEVENT_KEYDOWN) {
            if (event.windows_key_code == 13) { // Enter key
                overlay_.ExecuteTopResult();
                return true;
            }
            if (event.windows_key_code == 27) { // Escape key
                overlay_.SetVisible(false);
                return true;
            }
        }
        return false;
    }

private:
    CommandPaletteOverlay& overlay_;

    IMPLEMENT_REFCOUNTING(QueryFieldDelegate);
};

class CommandPaletteOverlay::ActionItemDelegate final : public CefButtonDelegate {
public:
    ActionItemDelegate(CommandPaletteOverlay& overlay, std::string action_id)
        : overlay_(overlay), action_id_(std::move(action_id)) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        overlay_.ExecuteAction(action_id_);
    }

private:
    CommandPaletteOverlay& overlay_;
    std::string action_id_;

    IMPLEMENT_REFCOUNTING(ActionItemDelegate);
};

CommandPaletteOverlay::CommandPaletteOverlay(
    core::ActionRegistry& action_registry,
    std::function<void(const std::string&)> on_action_executed)
    : action_registry_(action_registry),
      palette_(action_registry),
      on_action_executed_(std::move(on_action_executed)) {

    container_ = CefPanel::CreatePanel(nullptr);

    CefBoxLayoutSettings container_settings{};
    container_settings.horizontal = 0;
    container_settings.between_child_spacing = 4;
    container_settings.inside_border_horizontal_spacing = 12;
    container_settings.inside_border_vertical_spacing = 8;
    container_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    container_layout_ = container_->SetAsBoxLayout(container_settings);

    search_delegate_ = new QueryFieldDelegate(*this);
    search_field_ = CefTextfield::CreateTextfield(search_delegate_);
    search_field_->SetPlaceholderText("Type a command or search action (e.g. 'network', 'tab', 'focus')...");
    container_->AddChildView(search_field_);

    results_panel_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings results_settings{};
    results_settings.horizontal = 0;
    results_settings.between_child_spacing = 2;
    results_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    results_layout_ = results_panel_->SetAsBoxLayout(results_settings);
    container_->AddChildView(results_panel_);

    container_->SetVisible(false);
    RebuildResultsView();
}

CefRefPtr<CefPanel> CommandPaletteOverlay::View() const noexcept {
    return container_;
}

void CommandPaletteOverlay::SetVisible(bool visible) {
    container_->SetVisible(visible);
    if (visible) {
        if (search_field_) {
            search_field_->SetText("");
            current_query_.clear();
            search_field_->RequestFocus();
            RebuildResultsView();
        }
    }
}

bool CommandPaletteOverlay::IsVisible() const {
    return container_->IsVisible();
}

void CommandPaletteOverlay::ToggleVisibility() {
    SetVisible(!IsVisible());
}

void CommandPaletteOverlay::SetQuery(const std::string& query) {
    current_query_ = query;
    RebuildResultsView();
}

const std::string& CommandPaletteOverlay::GetQuery() const noexcept {
    return current_query_;
}

void CommandPaletteOverlay::ExecuteTopResult() {
    if (!current_results_.empty()) {
        ExecuteAction(current_results_.front().action.id);
    }
}

void CommandPaletteOverlay::ExecuteAction(const std::string& action_id) {
    SetVisible(false);
    action_registry_.Dispatch(action_id);
    if (on_action_executed_) {
        on_action_executed_(action_id);
    }
}

void CommandPaletteOverlay::RebuildResultsView() {
    results_panel_->RemoveAllChildViews();
    result_delegates_.clear();

    current_results_ = palette_.Search(current_query_, 6);
    for (const auto& res : current_results_) {
        std::string label = "[" + std::string(core::ActionCategoryToString(res.action.category)) + "] " +
                            res.action.title + " - " + res.action.description;
        if (!res.action.default_shortcut.empty()) {
            label += " (" + res.action.default_shortcut + ")";
        }

        auto delegate = new ActionItemDelegate(*this, res.action.id);
        result_delegates_.push_back(delegate);

        auto btn = CefLabelButton::CreateLabelButton(delegate, label);
        results_panel_->AddChildView(btn);
    }

    results_panel_->InvalidateLayout();
}

} // namespace openbrowser::desktop
