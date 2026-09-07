#pragma once

#include "core/bookmarks/bookmark_manager.h"
#include "core/session/browser_session_observer.h"

#include "include/cef_base.h"
#include "include/views/cef_button.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_panel.h"

#include <memory>
#include <string>
#include <vector>

namespace openbrowser::core {
class BrowserSession;
}

class CefBoxLayout;
class CefLabelButton;

namespace openbrowser::desktop {

class BookmarksBar final : public core::BrowserSessionObserver {
public:
    BookmarksBar(
        core::BookmarkManager& bookmark_manager,
        core::BrowserSession& session);
    ~BookmarksBar() override;

    BookmarksBar(const BookmarksBar&) = delete;
    BookmarksBar& operator=(const BookmarksBar&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();

    void BookmarkCurrentPage();
    void RebuildBar();

    // BrowserSessionObserver
    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    class BookmarkItemDelegate;
    class AddBookmarkDelegate;

    void NavigateTo(const std::string& url);

    core::BookmarkManager& bookmark_manager_;
    core::BrowserSession& session_;

    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    CefRefPtr<CefLabelButton> add_button_;
    CefRefPtr<CefButtonDelegate> add_delegate_;
    std::vector<CefRefPtr<CefButtonDelegate>> item_delegates_;
};

} // namespace openbrowser::desktop
