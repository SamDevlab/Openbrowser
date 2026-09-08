#include "find_bar.h"

#include "core/session/browser_session.h"

#include "include/wrapper/cef_helpers.h"

#include <string>
#include <utility>

namespace openbrowser::desktop {
namespace {

class PassiveButtonDelegate final : public CefButtonDelegate {
public:
    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
    }

private:
    IMPLEMENT_REFCOUNTING(PassiveButtonDelegate);
};

}  // namespace

class FindBar::ActionDelegate final : public CefButtonDelegate {
public:
    ActionDelegate(FindBar& bar, const Action action)
        : bar_(bar), action_(action) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        bar_.HandleAction(action_);
    }

private:
    FindBar& bar_;
    Action action_;

    IMPLEMENT_REFCOUNTING(ActionDelegate);
};

class FindBar::FieldDelegate final : public CefTextfieldDelegate {
public:
    explicit FieldDelegate(FindBar& bar) : bar_(bar) {}

    bool OnKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event) override {
        CEF_REQUIRE_UI_THREAD();
        return bar_.HandleFieldKeyEvent(std::move(textfield), event);
    }

    void OnAfterUserAction(CefRefPtr<CefTextfield> /*textfield*/) override {
        CEF_REQUIRE_UI_THREAD();
        bar_.HandleFieldUserAction();
    }

private:
    FindBar& bar_;

    IMPLEMENT_REFCOUNTING(FieldDelegate);
};

FindBar::FindBar(core::BrowserSession& session, CefRefPtr<CefBrowserEngine> engine)
    : session_(session), engine_(std::move(engine)) {
    session_.AddObserver(this);

    panel_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings settings{};
    settings.horizontal = 1;
    settings.between_child_spacing = 5;
    settings.inside_border_horizontal_spacing = 8;
    settings.inside_border_vertical_spacing = 4;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    layout_ = panel_->SetToBoxLayout(settings);

    field_delegate_ = new FieldDelegate(*this);
    search_field_ = CefTextfield::CreateTextfield(field_delegate_);
    search_field_->SetPlaceholderText("Find in page");
    search_field_->SetAccessibleName("Find in page");

    CefRefPtr<CefButtonDelegate> previous_delegate(new ActionDelegate(*this, Action::Previous));
    CefRefPtr<CefButtonDelegate> next_delegate(new ActionDelegate(*this, Action::Next));
    CefRefPtr<CefButtonDelegate> close_delegate(new ActionDelegate(*this, Action::Close));
    action_delegates_.push_back(previous_delegate);
    action_delegates_.push_back(next_delegate);
    action_delegates_.push_back(close_delegate);

    result_label_ = CefLabelButton::CreateLabelButton(new PassiveButtonDelegate(), "0 / 0");
    result_label_->SetEnabled(false);
    CefRefPtr<CefLabelButton> previous = CefLabelButton::CreateLabelButton(previous_delegate, "↑");
    CefRefPtr<CefLabelButton> next = CefLabelButton::CreateLabelButton(next_delegate, "↓");
    CefRefPtr<CefLabelButton> close = CefLabelButton::CreateLabelButton(close_delegate, "×");

    panel_->AddChildView(search_field_);
    layout_->SetFlexForView(search_field_, 1);
    panel_->AddChildView(result_label_);
    layout_->SetFlexForView(result_label_, 0);
    panel_->AddChildView(previous);
    layout_->SetFlexForView(previous, 0);
    panel_->AddChildView(next);
    layout_->SetFlexForView(next, 0);
    panel_->AddChildView(close);
    layout_->SetFlexForView(close, 0);

    panel_->SetVisible(false);

    if (engine_) {
        engine_->SetFindResultCallback(
            [this](const core::TabId& tab_id,
                   const int match_count,
                   const int active_match_ordinal,
                   const bool final_update) {
                UpdateResult(tab_id, match_count, active_match_ordinal, final_update);
            });
    }
}

FindBar::~FindBar() {
    session_.RemoveObserver(this);
    if (engine_) {
        engine_->SetFindResultCallback(nullptr);
    }
}

CefRefPtr<CefPanel> FindBar::View() const noexcept {
    return panel_;
}

void FindBar::Open() {
    CEF_REQUIRE_UI_THREAD();
    if (!panel_) {
        return;
    }

    panel_->SetVisible(true);
    LayoutParent();

    if (search_field_) {
        search_field_->RequestFocus();
        search_field_->SelectAll(false);
        if (!search_field_->GetText().empty()) {
            StartSearch(true, false);
        }
    }
}

void FindBar::Close() {
    CEF_REQUIRE_UI_THREAD();
    StopCurrentSearch(true);
    ResetResultLabel();
    if (panel_) {
        panel_->SetVisible(false);
    }
    LayoutParent();
}

void FindBar::ToggleVisibility() {
    CEF_REQUIRE_UI_THREAD();
    if (IsVisible()) {
        Close();
    } else {
        Open();
    }
}

bool FindBar::IsVisible() const {
    return panel_ && panel_->IsVisible();
}

void FindBar::FindNext() {
    CEF_REQUIRE_UI_THREAD();
    if (!IsVisible()) {
        Open();
        return;
    }
    StartSearch(true, true);
}

void FindBar::FindPrevious() {
    CEF_REQUIRE_UI_THREAD();
    if (!IsVisible()) {
        Open();
        return;
    }
    StartSearch(false, true);
}

void FindBar::UpdateResult(
    const core::TabId& tab_id,
    const int match_count,
    const int active_match_ordinal,
    const bool /*final_update*/) {
    CEF_REQUIRE_UI_THREAD();
    if (!IsVisible() || !searched_tab_id_.has_value() || *searched_tab_id_ != tab_id || !result_label_) {
        return;
    }

    if (match_count <= 0) {
        result_label_->SetText("0 / 0");
        return;
    }

    const int ordinal = active_match_ordinal > 0 ? active_match_ordinal : 1;
    result_label_->SetText(std::to_string(ordinal) + " / " + std::to_string(match_count));
}

void FindBar::OnBrowserSessionChanged(const core::BrowserSession& session) {
    CEF_REQUIRE_UI_THREAD();
    if (!IsVisible()) {
        return;
    }

    const auto& active_id = session.ActiveTabId();
    if (searched_tab_id_ == active_id) {
        return;
    }

    StopCurrentSearch(true);
    ResetResultLabel();

    if (active_id.has_value() && search_field_ && !search_field_->GetText().empty()) {
        StartSearch(true, false);
    }
}

void FindBar::HandleAction(const Action action) {
    CEF_REQUIRE_UI_THREAD();
    switch (action) {
        case Action::Previous:
            FindPrevious();
            return;
        case Action::Next:
            FindNext();
            return;
        case Action::Close:
            Close();
            return;
    }
}

bool FindBar::HandleFieldKeyEvent(
    CefRefPtr<CefTextfield> /*textfield*/,
    const CefKeyEvent& event) {
    CEF_REQUIRE_UI_THREAD();

    if (event.windows_key_code == 13) {
        if (event.type == KEYEVENT_RAWKEYDOWN || event.type == KEYEVENT_KEYDOWN) {
            const bool backward = (event.modifiers & EVENTFLAG_SHIFT_DOWN) != 0;
            StartSearch(!backward, true);
            return true;
        }
        if (event.type == KEYEVENT_CHAR) {
            return true;
        }
    }

    if (event.windows_key_code == 27) {
        if (event.type == KEYEVENT_RAWKEYDOWN || event.type == KEYEVENT_KEYDOWN) {
            Close();
            return true;
        }
        if (event.type == KEYEVENT_CHAR) {
            return true;
        }
    }

    return false;
}

void FindBar::HandleFieldUserAction() {
    CEF_REQUIRE_UI_THREAD();
    if (!IsVisible()) {
        return;
    }
    StartSearch(true, false);
}

void FindBar::StartSearch(const bool forward, const bool find_next) {
    CEF_REQUIRE_UI_THREAD();
    if (!engine_ || !search_field_) {
        return;
    }

    const auto& active_id = session_.ActiveTabId();
    const std::string text = search_field_->GetText().ToString();
    if (!active_id.has_value() || text.empty()) {
        StopCurrentSearch(true);
        ResetResultLabel();
        return;
    }

    if (!find_next || !searched_tab_id_.has_value() || *searched_tab_id_ != *active_id) {
        ResetResultLabel();
    }

    searched_tab_id_ = *active_id;
    engine_->FindInPage(*active_id, text, forward, find_next);
}

void FindBar::StopCurrentSearch(const bool clear_selection) {
    CEF_REQUIRE_UI_THREAD();
    if (engine_ && searched_tab_id_.has_value()) {
        engine_->StopFinding(*searched_tab_id_, clear_selection);
    }
    searched_tab_id_.reset();
}

void FindBar::ResetResultLabel() {
    if (result_label_) {
        result_label_->SetText("0 / 0");
    }
}

void FindBar::LayoutParent() {
    if (!panel_) {
        return;
    }
    if (const auto parent = panel_->GetParentView(); parent) {
        if (const auto parent_panel = parent->AsPanel(); parent_panel) {
            parent_panel->Layout();
        }
    }
}

}  // namespace openbrowser::desktop
