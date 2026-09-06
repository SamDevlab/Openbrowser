#pragma once

#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session.h"

#include <string>

namespace openbrowser::core {

class FocusSessionController {
public:
    // Enqueues the active tab of the session into the focus queue.
    // If item_id is empty, a generated unique ID based on the tab is used.
    [[nodiscard]] static bool EnqueueActiveTab(
        const BrowserSession& session,
        FocusQueue& queue,
        std::string item_id = {},
        FocusState initial_state = FocusState::Next);

    // Activates the focus item:
    // If item.tab_id is valid in the session, activates that tab.
    // Otherwise, opens a new tab for item.url and binds its tab_id to the queue item.
    [[nodiscard]] static bool ActivateFocusItem(
        BrowserSession& session,
        FocusQueue& queue,
        const std::string& item_id);

    // Clears the tab_id for any focus items whose underlying tabs have been closed in the session.
    static void SynchronizeTabClosures(
        const BrowserSession& session,
        FocusQueue& queue);
};

}  // namespace openbrowser::core
