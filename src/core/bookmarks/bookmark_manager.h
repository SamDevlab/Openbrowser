#pragma once

#include "core/tabs/tab.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openbrowser::core {

struct BookmarkItem {
    std::string id;
    std::string url;
    std::string title;
    std::vector<std::string> tags;
    std::int64_t created_at_ms{0};
    std::optional<WorkspaceId> workspace_id;
};

class BookmarkManager {
public:
    BookmarkManager() = default;

    bool AddBookmark(BookmarkItem item);
    bool RemoveBookmark(const std::string& id);
    bool RemoveBookmarkByUrl(const std::string& url);

    bool EditBookmark(
        const std::string& id,
        const std::string& new_title,
        const std::string& new_url,
        const std::optional<std::vector<std::string>>& new_tags = std::nullopt);

    [[nodiscard]] bool IsBookmarked(const std::string& url) const noexcept;
    [[nodiscard]] const BookmarkItem* FindBookmark(const std::string& id) const noexcept;
    [[nodiscard]] std::vector<BookmarkItem> ListBookmarks(
        const std::optional<WorkspaceId>& workspace_id = std::nullopt) const;

    [[nodiscard]] std::vector<BookmarkItem> Search(const std::string& query) const;
    [[nodiscard]] std::size_t TotalBookmarks() const noexcept;

    void SetAutoSavePath(std::filesystem::path path);
    [[nodiscard]] const std::filesystem::path& AutoSavePath() const noexcept;

    [[nodiscard]] std::string Serialize() const;
    bool Deserialize(std::string_view json);

    [[nodiscard]] bool SaveToFile(const std::filesystem::path& path) const;
    bool LoadFromFile(const std::filesystem::path& path);

private:
    void TriggerAutoSave() const;

    std::vector<BookmarkItem> bookmarks_;
    std::size_t next_id_{1};
    std::filesystem::path auto_save_path_;
};

}  // namespace openbrowser::core
