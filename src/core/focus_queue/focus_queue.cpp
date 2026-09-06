#include "core/focus_queue/focus_queue.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace openbrowser::core {

bool FocusQueue::Enqueue(FocusQueueItem item) {
    if (item.id.empty() || item.url.empty() || Contains(item.id)) {
        return false;
    }

    items_.push_back(std::move(item));
    return true;
}

bool FocusQueue::Remove(const std::string& item_id) {
    const auto it = FindMutable(item_id);
    if (it == items_.end()) {
        return false;
    }

    items_.erase(it);
    return true;
}

bool FocusQueue::Move(const std::string& item_id, const std::size_t new_index) {
    if (new_index >= items_.size()) {
        return false;
    }

    const auto it = FindMutable(item_id);
    if (it == items_.end()) {
        return false;
    }

    const auto old_index = static_cast<std::size_t>(std::distance(items_.begin(), it));
    if (old_index == new_index) {
        return true;
    }

    auto item = std::move(*it);
    items_.erase(it);
    items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(new_index), std::move(item));
    return true;
}

bool FocusQueue::SetState(const std::string& item_id, const FocusState state) {
    const auto it = FindMutable(item_id);
    if (it == items_.end()) {
        return false;
    }

    it->state = state;
    return true;
}

bool FocusQueue::SetTabId(const std::string& item_id, std::optional<TabId> tab_id) {
    const auto it = FindMutable(item_id);
    if (it == items_.end()) {
        return false;
    }

    it->tab_id = std::move(tab_id);
    return true;
}

bool FocusQueue::PromoteToNow(const std::string& item_id) {
    const auto selected = FindMutable(item_id);
    if (selected == items_.end()) {
        return false;
    }

    for (auto& item : items_) {
        if (item.state == FocusState::Now) {
            item.state = FocusState::Next;
        }
    }

    selected->state = FocusState::Now;
    return Move(item_id, 0);
}

bool FocusQueue::Contains(const std::string& item_id) const {
    return Find(item_id) != nullptr;
}

const FocusQueueItem* FocusQueue::Find(const std::string& item_id) const {
    const auto it = std::find_if(items_.cbegin(), items_.cend(), [&item_id](const FocusQueueItem& item) {
        return item.id == item_id;
    });

    return it == items_.cend() ? nullptr : &(*it);
}

const std::vector<FocusQueueItem>& FocusQueue::Items() const noexcept {
    return items_;
}

std::vector<FocusQueueItem>::iterator FocusQueue::FindMutable(const std::string& item_id) {
    return std::find_if(items_.begin(), items_.end(), [&item_id](const FocusQueueItem& item) {
        return item.id == item_id;
    });
}

}  // namespace openbrowser::core
