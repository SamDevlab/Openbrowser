#include "core/session/focus_session_controller.h"

#include <utility>

namespace openbrowser::core {

bool FocusSessionController::EnqueueActiveTab(
    const BrowserSession& session,
    FocusQueue& queue,
    std::string item_id,
    const FocusState initial_state) {
    const auto& active_tab_id = session.ActiveTabId();
    if (!active_tab_id.has_value()) {
        return false;
    }

    const auto* tab = session.FindTab(*active_tab_id);
    if (tab == nullptr || tab->url.empty()) {
        return false;
    }

    if (item_id.empty()) {
        item_id = "focus-" + *active_tab_id + "-" + std::to_string(queue.Items().size() + 1);
    }

    if (queue.Contains(item_id)) {
        return false;
    }

    return queue.Enqueue({
        .id = std::move(item_id),
        .url = tab->url,
        .tab_id = *active_tab_id,
        .workspace_id = tab->workspace_id,
        .state = initial_state,
    });
}

bool FocusSessionController::ActivateFocusItem(
    BrowserSession& session,
    FocusQueue& queue,
    const std::string& item_id) {
    const auto* item = queue.Find(item_id);
    if (item == nullptr || item->url.empty()) {
        return false;
    }

    if (item->tab_id.has_value()) {
        const auto* tab = session.FindTab(*item->tab_id);
        if (tab != nullptr && tab->lifecycle != TabLifecycle::Discarded) {
            return session.ActivateTab(*item->tab_id);
        }
    }

    // Tab was closed or not yet opened; open new tab and bind tab_id
    std::string new_tab_id = "focus-tab-" + item_id;
    std::size_t suffix = 1;
    while (session.FindTab(new_tab_id) != nullptr) {
        new_tab_id = "focus-tab-" + item_id + "-" + std::to_string(suffix++);
    }

    const bool opened = session.OpenTab({
        .id = new_tab_id,
        .url = item->url,
        .title = "Focus Tab",
        .lifecycle = TabLifecycle::Active,
        .workspace_id = item->workspace_id,
    }, true);

    if (opened) {
        queue.SetTabId(item_id, new_tab_id);
        return true;
    }

    return false;
}

void FocusSessionController::SynchronizeTabClosures(
    const BrowserSession& session,
    FocusQueue& queue) {
    for (const auto& item : queue.Items()) {
        if (item.tab_id.has_value()) {
            if (session.FindTab(*item.tab_id) == nullptr) {
                queue.SetTabId(item.id, std::nullopt);
            }
        }
    }
}

}  // namespace openbrowser::core
