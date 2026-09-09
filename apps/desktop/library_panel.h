#pragma once

#include "core/bookmarks/bookmark_manager.h"
#include "core/history/history_manager.h"
#include "core/session/browser_session_observer.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"

#include <functional>
#include <string>
#include <vector>

namespace openbrowser::core {
class BrowserSession;
}

namespace openbrowser::desktop {

class LibraryPanel final : public core::BrowserSessionObserver {
public:
    using DataChangedCallback = std::function<void()>;
    using VisibilityChangedCallback = std::function<void(bool)>;

    LibraryPanel(
        core::HistoryManager& history_manager,
        core::BookmarkManager& bookmark_manager,
        core::BrowserSession& session,
        DataChangedCallback on_data_changed = nullptr);
    ~LibraryPanel() override;

    LibraryPanel(const LibraryPanel&) = delete;
    LibraryPanel& operator=(const LibraryPanel&) = delete;

    [[nodiscard]] CefRefPtr<CefPanel> View() const noexcept;

    void SetVisible(bool visible);
    void SetVisibilityChangedCallback(VisibilityChangedCallback callback);
    [[nodiscard]] bool IsVisible() const;
    void ToggleVisibility();
    void ShowHistory();
    void ShowBookmarks();
    void Refresh();

    void OnBrowserSessionChanged(const core::BrowserSession& session) override;

private:
    enum class Section {
        History,
        Bookmarks,
    };

    enum class ActionKind {
        ShowHistory,
        ShowBookmarks,
        OpenUrl,
        RemoveHistory,
        RemoveBookmark,
        ClearHistory,
        Close,
    };

    struct Action {
        ActionKind kind{ActionKind::Close};
        std::string value;
    };

    class ActionDelegate;
    class PassiveDelegate;

    [[nodiscard]] CefRefPtr<CefLabelButton> MakeActionButton(
        const Action& action,
        const std::string& text,
        std::vector<CefRefPtr<CefButtonDelegate>>& owner);
    [[nodiscard]] CefRefPtr<CefLabelButton> MakeLabel(
        const std::string& text,
        std::vector<CefRefPtr<CefButtonDelegate>>& owner);

    void HandleAction(const Action& action);
    void RebuildBody();
    void AddHistoryRows();
    void AddBookmarkRows();
    void NavigateTo(const std::string& url);
    [[nodiscard]] bool IsPrivateContext() const;
    [[nodiscard]] std::string CurrentWorkspaceId() const;
    void NotifyDataChanged();

    core::HistoryManager& history_manager_;
    core::BookmarkManager& bookmark_manager_;
    core::BrowserSession& session_;
    DataChangedCallback on_data_changed_;
    Section section_{Section::History};

    CefRefPtr<CefPanel> panel_;
    CefRefPtr<CefBoxLayout> layout_;
    CefRefPtr<CefPanel> body_;
    CefRefPtr<CefBoxLayout> body_layout_;
    CefRefPtr<CefLabelButton> history_button_;
    CefRefPtr<CefLabelButton> bookmarks_button_;
    std::vector<CefRefPtr<CefButtonDelegate>> static_delegates_;
    std::vector<CefRefPtr<CefButtonDelegate>> body_delegates_;
    VisibilityChangedCallback on_visibility_changed_;
};

}  // namespace openbrowser::desktop
