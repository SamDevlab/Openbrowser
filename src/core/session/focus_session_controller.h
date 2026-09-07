#pragma once

#include "core/focus_queue/focus_queue.h"
#include "core/focus_queue/focus_sprint.h"
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

    // Checks whether the currently active tab in the browser session matches an item in the focus queue.
    [[nodiscard]] static bool IsActiveTabInFocusQueue(
        const BrowserSession& session,
        const FocusQueue& queue);

    // Records dwell time onto the sprint: attributes delta_seconds to focused if active tab is in queue, else distracted.
    static bool RecordSprintTick(
        FocusSprint& sprint,
        const BrowserSession& session,
        const FocusQueue& queue,
        uint32_t delta_seconds = 1);
};

}  // namespace openbrowser::core
