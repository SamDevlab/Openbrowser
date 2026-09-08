#include "library_panel.h"

#include "core/session/browser_session.h"

#include "include/wrapper/cef_helpers.h"

#include <algorithm>
#include <utility>

namespace openbrowser::desktop {
namespace {

constexpr std::size_t kHistoryLimit = 24;
constexpr std::size_t kLabelLimit = 48;

std::string TruncateLabel(std::string text) {
    if (text.size() <= kLabelLimit) {
        return text;
    }
    text.resize(kLabelLimit - 3);
    text += "...";
    return text;
}

CefRefPtr<CefPanel> CreateRow() {
    CefRefPtr<CefPanel> row = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings settings{};
    settings.horizontal = 1;
    settings.between_child_spacing = 6;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    row->SetToBoxLayout(settings);
    return row;
}

}  // namespace

class LibraryPanel::ActionDelegate final : public CefButtonDelegate {
public:
    ActionDelegate(LibraryPanel& panel, Action action)
        : panel_(panel), action_(std::move(action)) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        panel_.HandleAction(action_);
    }

private:
    LibraryPanel& panel_;
    Action action_;

    IMPLEMENT_REFCOUNTING(ActionDelegate);
};

class LibraryPanel::PassiveDelegate final : public CefButtonDelegate {
public:
    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
    }

private:
    IMPLEMENT_REFCOUNTING(PassiveDelegate);
};

LibraryPanel::LibraryPanel(
    core::HistoryManager& history_manager,
    core::BookmarkManager& bookmark_manager,
    core::BrowserSession& session,
    DataChangedCallback on_data_changed)
    : history_manager_(history_manager),
      bookmark_manager_(bookmark_manager),
      session_(session),
      on_data_changed_(std::move(on_data_changed)) {
    session_.AddObserver(this);

    panel_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings panel_settings{};
    panel_settings.horizontal = 0;
    panel_settings.between_child_spacing = 6;
    panel_settings.inside_border_horizontal_spacing = 10;
    panel_settings.inside_border_vertical_spacing = 8;
    panel_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    layout_ = panel_->SetToBoxLayout(panel_settings);

    CefRefPtr<CefPanel> header = CreateRow();
    CefRefPtr<CefBoxLayout> header_layout = header->GetLayout()->AsBoxLayout();

    CefRefPtr<CefLabelButton> title = MakeLabel("Library", static_delegates_);
    history_button_ = MakeActionButton(
        {ActionKind::ShowHistory, {}}, "History", static_delegates_);
    bookmarks_button_ = MakeActionButton(
        {ActionKind::ShowBookmarks, {}}, "Bookmarks", static_delegates_);
    CefRefPtr<CefLabelButton> close = MakeActionButton(
        {ActionKind::Close, {}}, "Close", static_delegates_);

    header->AddChildView(title);
    if (header_layout) {
        header_layout->SetFlexForView(title, 1);
    }
    header->AddChildView(history_button_);
    header->AddChildView(bookmarks_button_);
    header->AddChildView(close);
    panel_->AddChildView(header);
    if (layout_) {
        layout_->SetFlexForView(header, 0);
    }

    body_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings body_settings{};
    body_settings.horizontal = 0;
    body_settings.between_child_spacing = 4;
    body_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    body_layout_ = body_->SetToBoxLayout(body_settings);
    panel_->AddChildView(body_);
    if (layout_) {
        layout_->SetFlexForView(body_, 1);
    }

    RebuildBody();
    panel_->SetVisible(false);
}

LibraryPanel::~LibraryPanel() {
    session_.RemoveObserver(this);
}

CefRefPtr<CefPanel> LibraryPanel::View() const noexcept {
    return panel_;
}

void LibraryPanel::SetVisible(const bool visible) {
    CEF_REQUIRE_UI_THREAD();
    if (!panel_) {
        return;
    }
    if (visible) {
        Refresh();
    }
    panel_->SetVisible(visible);
    if (const auto parent = panel_->GetParentView(); parent) {
        if (const auto parent_panel = parent->AsPanel(); parent_panel) {
            parent_panel->Layout();
        }
    }
}

bool LibraryPanel::IsVisible() const {
    return panel_ && panel_->IsVisible();
}

void LibraryPanel::ToggleVisibility() {
    CEF_REQUIRE_UI_THREAD();
    SetVisible(!IsVisible());
}

void LibraryPanel::ShowHistory() {
    CEF_REQUIRE_UI_THREAD();
    section_ = Section::History;
    Refresh();
    SetVisible(true);
}

void LibraryPanel::ShowBookmarks() {
    CEF_REQUIRE_UI_THREAD();
    section_ = Section::Bookmarks;
    Refresh();
    SetVisible(true);
}

void LibraryPanel::Refresh() {
    CEF_REQUIRE_UI_THREAD();
    RebuildBody();
}

void LibraryPanel::OnBrowserSessionChanged(const core::BrowserSession& /*session*/) {
    CEF_REQUIRE_UI_THREAD();
    if (IsVisible()) {
        Refresh();
    }
}

CefRefPtr<CefLabelButton> LibraryPanel::MakeActionButton(
    const Action& action,
    const std::string& text,
    std::vector<CefRefPtr<CefButtonDelegate>>& owner) {
    CefRefPtr<CefButtonDelegate> delegate = new ActionDelegate(*this, action);
    owner.push_back(delegate);
    return CefLabelButton::CreateLabelButton(delegate, text);
}

CefRefPtr<CefLabelButton> LibraryPanel::MakeLabel(
    const std::string& text,
    std::vector<CefRefPtr<CefButtonDelegate>>& owner) {
    CefRefPtr<CefButtonDelegate> delegate = new PassiveDelegate();
    owner.push_back(delegate);
    CefRefPtr<CefLabelButton> label = CefLabelButton::CreateLabelButton(delegate, text);
    label->SetEnabled(false);
    return label;
}

void LibraryPanel::HandleAction(const Action& action) {
    CEF_REQUIRE_UI_THREAD();

    switch (action.kind) {
        case ActionKind::ShowHistory:
            section_ = Section::History;
            RebuildBody();
            return;

        case ActionKind::ShowBookmarks:
            section_ = Section::Bookmarks;
            RebuildBody();
            return;

        case ActionKind::OpenUrl:
            NavigateTo(action.value);
            return;

        case ActionKind::RemoveHistory:
            if (!IsPrivateContext() && history_manager_.RemoveEntry(action.value)) {
                NotifyDataChanged();
                RebuildBody();
            }
            return;

        case ActionKind::RemoveBookmark:
            if (!IsPrivateContext() && bookmark_manager_.RemoveBookmark(action.value)) {
                NotifyDataChanged();
                RebuildBody();
            }
            return;

        case ActionKind::ClearHistory:
            if (!IsPrivateContext()) {
                history_manager_.ClearHistory();
                NotifyDataChanged();
                RebuildBody();
            }
            return;

        case ActionKind::Close:
            SetVisible(false);
            return;
    }
}

void LibraryPanel::RebuildBody() {
    if (!body_) {
        return;
    }

    body_->RemoveAllChildViews();
    body_delegates_.clear();

    if (history_button_) {
        history_button_->SetText(section_ == Section::History ? "History *" : "History");
    }
    if (bookmarks_button_) {
        bookmarks_button_->SetText(section_ == Section::Bookmarks ? "Bookmarks *" : "Bookmarks");
    }

    if (section_ == Section::History) {
        AddHistoryRows();
    } else {
        AddBookmarkRows();
    }

    body_->InvalidateLayout();
    panel_->InvalidateLayout();
    panel_->Layout();
}

void LibraryPanel::AddHistoryRows() {
    if (IsPrivateContext()) {
        body_->AddChildView(MakeLabel(
            "History is hidden in Private mode.", body_delegates_));
        return;
    }

    const auto entries = history_manager_.ListHistory(kHistoryLimit);
    if (entries.empty()) {
        body_->AddChildView(MakeLabel("No browsing history yet.", body_delegates_));
        return;
    }

    body_->AddChildView(MakeLabel(
        "Recent history (up to 24 entries)", body_delegates_));

    for (const auto& entry : entries) {
        CefRefPtr<CefPanel> row = CreateRow();
        CefRefPtr<CefBoxLayout> row_layout = row->GetLayout()->AsBoxLayout();

        std::string label = entry.title.empty() ? entry.url : entry.title;
        label = TruncateLabel(std::move(label));
        label += "  ·  " + std::to_string(entry.visit_count) + " visit";
        if (entry.visit_count != 1) {
            label += "s";
        }

        CefRefPtr<CefLabelButton> open = MakeActionButton(
            {ActionKind::OpenUrl, entry.url}, label, body_delegates_);
        CefRefPtr<CefLabelButton> remove = MakeActionButton(
            {ActionKind::RemoveHistory, entry.id}, "Remove", body_delegates_);

        row->AddChildView(open);
        if (row_layout) {
            row_layout->SetFlexForView(open, 1);
        }
        row->AddChildView(remove);
        if (row_layout) {
            row_layout->SetFlexForView(remove, 0);
        }
        body_->AddChildView(row);
        if (body_layout_) {
            body_layout_->SetFlexForView(row, 0);
        }
    }

    CefRefPtr<CefLabelButton> clear = MakeActionButton(
        {ActionKind::ClearHistory, {}}, "Clear all history", body_delegates_);
    body_->AddChildView(clear);
    if (body_layout_) {
        body_layout_->SetFlexForView(clear, 0);
    }
}

void LibraryPanel::AddBookmarkRows() {
    const std::string workspace = CurrentWorkspaceId();
    const auto bookmarks = bookmark_manager_.ListBookmarks(workspace);
    const bool read_only = IsPrivateContext();

    std::string heading = "Bookmarks · workspace: " + workspace;
    if (read_only) {
        heading += " · read-only in Private mode";
    }
    body_->AddChildView(MakeLabel(heading, body_delegates_));

    if (bookmarks.empty()) {
        body_->AddChildView(MakeLabel(
            "No bookmarks in this workspace.", body_delegates_));
        return;
    }

    for (const auto& bookmark : bookmarks) {
        CefRefPtr<CefPanel> row = CreateRow();
        CefRefPtr<CefBoxLayout> row_layout = row->GetLayout()->AsBoxLayout();

        std::string label = bookmark.title.empty() ? bookmark.url : bookmark.title;
        label = TruncateLabel(std::move(label));

        CefRefPtr<CefLabelButton> open = MakeActionButton(
            {ActionKind::OpenUrl, bookmark.url}, label, body_delegates_);
        row->AddChildView(open);
        if (row_layout) {
            row_layout->SetFlexForView(open, 1);
        }

        if (!read_only) {
            CefRefPtr<CefLabelButton> remove = MakeActionButton(
                {ActionKind::RemoveBookmark, bookmark.id}, "Remove", body_delegates_);
            row->AddChildView(remove);
            if (row_layout) {
                row_layout->SetFlexForView(remove, 0);
            }
        }

        body_->AddChildView(row);
        if (body_layout_) {
            body_layout_->SetFlexForView(row, 0);
        }
    }
}

void LibraryPanel::NavigateTo(const std::string& url) {
    if (url.empty()) {
        return;
    }
    const auto& active_id = session_.ActiveTabId();
    if (!active_id) {
        return;
    }
    static_cast<void>(session_.Navigate(*active_id, url));
    SetVisible(false);
}

bool LibraryPanel::IsPrivateContext() const {
    const auto& active_id = session_.ActiveTabId();
    if (!active_id) {
        return false;
    }
    const auto* tab = session_.FindTab(*active_id);
    return tab && tab->is_ephemeral;
}

std::string LibraryPanel::CurrentWorkspaceId() const {
    const auto& active_id = session_.ActiveTabId();
    if (!active_id) {
        return "default";
    }
    const auto* tab = session_.FindTab(*active_id);
    if (!tab || !tab->workspace_id.has_value() || tab->workspace_id->empty()) {
        return "default";
    }
    return *tab->workspace_id;
}

void LibraryPanel::NotifyDataChanged() {
    if (on_data_changed_) {
        on_data_changed_();
    }
}

}  // namespace openbrowser::desktop
