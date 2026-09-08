#include "bookmarks_bar.h"

#include "core/session/browser_session.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_label_button.h"
#include "include/wrapper/cef_helpers.h"

namespace openbrowser::desktop {

class BookmarksBar::BookmarkItemDelegate final : public CefButtonDelegate {
public:
    BookmarkItemDelegate(BookmarksBar& bar, std::string url)
        : bar_(bar), url_(std::move(url)) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        bar_.NavigateTo(url_);
    }

private:
    BookmarksBar& bar_;
    std::string url_;

    IMPLEMENT_REFCOUNTING(BookmarkItemDelegate);
};

class BookmarksBar::AddBookmarkDelegate final : public CefButtonDelegate {
public:
    explicit AddBookmarkDelegate(BookmarksBar& bar)
        : bar_(bar) {}

    void OnButtonPressed(CefRefPtr<CefButton> /*button*/) override {
        CEF_REQUIRE_UI_THREAD();
        bar_.BookmarkCurrentPage();
    }

private:
    BookmarksBar& bar_;

    IMPLEMENT_REFCOUNTING(AddBookmarkDelegate);
};

BookmarksBar::BookmarksBar(
    core::BookmarkManager& bookmark_manager,
    core::BrowserSession& session)
    : bookmark_manager_(bookmark_manager), session_(session) {

    session_.AddObserver(this);

    panel_ = CefPanel::CreatePanel(nullptr);

    CefBoxLayoutSettings settings{};
    settings.horizontal = 1;
    settings.between_child_spacing = 4;
    settings.inside_border_horizontal_spacing = 8;
    settings.inside_border_vertical_spacing = 2;
    settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
    layout_ = panel_->SetToBoxLayout(settings);

    add_delegate_ = new AddBookmarkDelegate(*this);
    add_button_ = CefLabelButton::CreateLabelButton(add_delegate_, "☆ Add bookmark");

    RebuildBar();
    panel_->SetVisible(false);
}

BookmarksBar::~BookmarksBar() {
    session_.RemoveObserver(this);
}

CefRefPtr<CefPanel> BookmarksBar::View() const noexcept {
    return panel_;
}

void BookmarksBar::SetVisible(bool visible) {
    panel_->SetVisible(visible);
}

bool BookmarksBar::IsVisible() const {
    return panel_->IsVisible();
}

void BookmarksBar::ToggleVisibility() {
    SetVisible(!IsVisible());
}

void BookmarksBar::BookmarkCurrentPage() {
    const auto& active_id = session_.ActiveTabId();
    if (!active_id) {
        return;
    }
    const auto* active_tab = session_.FindTab(*active_id);
    if (!active_tab || active_tab->url.empty() || active_tab->is_ephemeral) {
        return;
    }

    const std::string title = active_tab->title.empty() ? active_tab->url : active_tab->title;
    const std::string workspace = active_tab->workspace_id.value_or("default");

    core::BookmarkItem item;
    item.url = active_tab->url;
    item.title = title;
    item.workspace_id = workspace;
    static_cast<void>(bookmark_manager_.AddBookmark(std::move(item)));
    RebuildBar();
}

void BookmarksBar::NavigateTo(const std::string& url) {
    const auto& active_id = session_.ActiveTabId();
    if (active_id && !url.empty()) {
        static_cast<void>(session_.Navigate(*active_id, url));
    }
}

void BookmarksBar::RebuildBar() {
    panel_->RemoveAllChildViews();
    item_delegates_.clear();

    panel_->AddChildView(add_button_);

    std::string current_workspace = "default";
    const auto& active_id = session_.ActiveTabId();
    if (active_id) {
        const auto* active_tab = session_.FindTab(*active_id);
        if (active_tab && active_tab->workspace_id.has_value() && !active_tab->workspace_id->empty()) {
            current_workspace = *active_tab->workspace_id;
        }
    }

    const auto bookmarks = bookmark_manager_.ListBookmarks(current_workspace);
    for (const auto& b : bookmarks) {
        std::string display_title = b.title.empty() ? b.url : b.title;
        if (display_title.length() > 20) {
            display_title = display_title.substr(0, 17) + "...";
        }
        std::string label = "★ " + display_title;

        auto delegate = new BookmarkItemDelegate(*this, b.url);
        item_delegates_.push_back(delegate);

        auto btn = CefLabelButton::CreateLabelButton(delegate, label);
        panel_->AddChildView(btn);
    }

    panel_->InvalidateLayout();
}

void BookmarksBar::OnBrowserSessionChanged(const core::BrowserSession& /*session*/) {
    RebuildBar();
}

} // namespace openbrowser::desktop
