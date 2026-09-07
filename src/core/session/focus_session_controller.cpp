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
    if (tab == nullptr || tab->url.empty() || tab->is_ephemeral) {
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

    // Focus Queue is persistent product state. Never use it as a path from an
    // ephemeral/private session back into a persistent tab or URL.
    if (session.ActiveTabId().has_value()) {
        const auto* active_tab = session.FindTab(*session.ActiveTabId());
        if (active_tab != nullptr && active_tab->is_ephemeral) {
            return false;
        }
    }

    if (item->tab_id.has_value()) {
        const auto* tab = session.FindTab(*item->tab_id);
        if (tab != nullptr && !tab->is_ephemeral && tab->lifecycle != TabLifecycle::Discarded) {
            return session.ActivateTab(*item->tab_id);
        }
    }

    // Tab was closed or not yet opened; open new persistent tab and bind tab_id.
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
        .is_ephemeral = false,
    }, true);

    if (opened) {
        static_cast<void>(queue.SetTabId(item_id, new_tab_id));
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
                static_cast<void>(queue.SetTabId(item.id, std::nullopt));
            }
        }
    }
}

bool FocusSessionController::IsActiveTabInFocusQueue(
    const BrowserSession& session,
    const FocusQueue& queue) {
    const auto& active_id = session.ActiveTabId();
    if (!active_id.has_value()) {
        return false;
    }

    const auto* tab = session.FindTab(*active_id);
    if (tab != nullptr && tab->is_ephemeral) {
        return false;
    }

    for (const auto& item : queue.Items()) {
        if (item.tab_id.has_value() && *item.tab_id == *active_id) {
            return true;
        }
        if (tab != nullptr && !tab->url.empty() && item.url == tab->url) {
            return true;
        }
    }
    return false;
}

bool FocusSessionController::RecordSprintTick(
    FocusSprint& sprint,
    const BrowserSession& session,
    const FocusQueue& queue,
    const uint32_t delta_seconds) {
    const bool is_focused = IsActiveTabInFocusQueue(session, queue);
    return sprint.Tick(delta_seconds, is_focused);
}

}  // namespace openbrowser::core
