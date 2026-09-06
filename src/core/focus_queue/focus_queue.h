#pragma once

#include "core/tabs/tab.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace openbrowser::core {

enum class FocusState {
    Now,
    Next,
    Later,
    Paused,
};

struct FocusQueueItem {
    std::string id;
    std::string url;
    std::optional<TabId> tab_id;
    std::optional<WorkspaceId> workspace_id;
    FocusState state{FocusState::Next};
};

class FocusQueue {
public:
    [[nodiscard]] bool Enqueue(FocusQueueItem item);
    [[nodiscard]] bool Remove(const std::string& item_id);
    [[nodiscard]] bool Move(const std::string& item_id, std::size_t new_index);
    [[nodiscard]] bool SetState(const std::string& item_id, FocusState state);
    [[nodiscard]] bool PromoteToNow(const std::string& item_id);

    [[nodiscard]] bool Contains(const std::string& item_id) const;
    [[nodiscard]] const FocusQueueItem* Find(const std::string& item_id) const;
    [[nodiscard]] const std::vector<FocusQueueItem>& Items() const noexcept;

private:
    [[nodiscard]] std::vector<FocusQueueItem>::iterator FindMutable(const std::string& item_id);

    std::vector<FocusQueueItem> items_;
};

}  // namespace openbrowser::core
