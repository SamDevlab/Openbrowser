#pragma once

#include "cef_browser_engine.h"
#include "core/session/browser_session_observer.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"

#include <optional>
#include <string>
#include <vector>

namespace openbrowser::core {
class BrowserSession;
}

namespace openbrowser::desktop {

class FindBar final : public core::BrowserSessionObserver {
public:
    FindBar(core::BrowserSession& session, CefRefPtr<CefBrowserEngine> engine);
    ~FindBar() override;

    FindBar(const FindBar&) = delete;
    FindBar& operator=(const FindBar&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void Open();
    void Close();
    void ToggleVisibility();
    [[nodiscard]] bool IsVisible() const;

    void FindNext();
    void FindPrevious();
    void UpdateResult(
        const core::TabId& tab_id,
        int match_count,
        int active_match_ordinal,
        bool final_update);

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    enum class Action {
        Previous,
        Next,
        Close,
    };

    class ActionDelegate;
    class FieldDelegate;

    void HandleAction(Action action);
    bool HandleFieldKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event);
    void HandleFieldUserAction();
    void StartSearch(bool forward, bool find_next);
    void StopCurrentSearch(bool clear_selection);
    void ResetResultLabel();
    void LayoutParent();

    core::BrowserSession& session_;
    CefRefPtr<CefBrowserEngine> engine_;

    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    CefRefPtr<CefTextfield> search_field_;
    CefRefPtr<CefLabelButton> result_label_;
    CefRefPtr<CefTextfieldDelegate> field_delegate_;
    std::vector<CefRefPtr<CefButtonDelegate>> action_delegates_;

    std::optional<core::TabId> searched_tab_id_;
};

}  // namespace openbrowser::desktop
