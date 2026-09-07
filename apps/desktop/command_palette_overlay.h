#pragma once

#include "core/commands/action_registry.h"
#include "core/commands/command_palette.h"

#include "include/cef_base.h"
#include "include/views/cef_button.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"

#include <memory>
#include <string>
#include <vector>

class CefBoxLayout;
class CefLabelButton;

namespace openbrowser::desktop {

class CommandPaletteOverlay final {
public:
    explicit CommandPaletteOverlay(
        core::ActionRegistry& action_registry,
        std::function<void(const std::string&)> on_action_executed = nullptr);
    ~CommandPaletteOverlay();

    CommandPaletteOverlay(const CommandPaletteOverlay&) = delete;
    CommandPaletteOverlay& operator=(const CommandPaletteOverlay&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();

    void SetQuery(const std::string& query);
    [[nodiscard]] const std::string& GetQuery() const noexcept;
    void ExecuteTopResult();

private:
    class QueryFieldDelegate;
    class ActionItemDelegate;

    void RebuildResultsView();
    void ExecuteAction(const std::string& action_id);

    core::ActionRegistry& action_registry_;
    core::CommandPalette palette_;
    std::function<void(const std::string&)> on_action_executed_;

    std::string current_query_;
    std::vector<core::CommandPaletteMatch> current_results_;

    CefRefPtr<CefPanel> container_;
    CefRefPtr<CefBoxLayout> container_layout_;
    CefRefPtr<CefTextfield> search_field_;
    CefRefPtr<CefPanel> results_panel_;
    CefRefPtr<CefBoxLayout> results_layout_;

    CefRefPtr<CefTextfieldDelegate> search_delegate_;
    std::vector<CefRefPtr<CefButtonDelegate>> result_delegates_;
};

} // namespace openbrowser::desktop
