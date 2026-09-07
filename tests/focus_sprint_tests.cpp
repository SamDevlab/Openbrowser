#include "core/focus_queue/focus_queue.h"
#include "core/focus_queue/focus_sprint.h"
#include "core/session/browser_session.h"
#include "core/session/focus_session_controller.h"
#include "fakes/fake_browser_engine.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestFocusSprintStateAndTiming() {
    using openbrowser::core::FocusSprint;
    using openbrowser::core::SprintState;

    FocusSprint sprint(1500);
    Require(sprint.State() == SprintState::Idle, "starts in Idle state");
    Require(sprint.RemainingSeconds() == 1500, "initial remaining is 1500s");
    Require(sprint.FormattedTime() == "25:00", "formatted time is 25:00");

    sprint.Start(60);
    Require(sprint.State() == SprintState::Running, "state is Running after Start");
    Require(sprint.RemainingSeconds() == 60, "remaining is 60s");
    Require(sprint.FormattedTime() == "01:00", "formatted time is 01:00");

    sprint.Pause();
    Require(sprint.State() == SprintState::Paused, "state is Paused after Pause");

    // While paused, tick does not decrease remaining_seconds, but still tracks dwell
    const bool completed_while_paused = sprint.Tick(10, true);
    Require(!completed_while_paused, "tick while paused does not complete sprint");
    Require(sprint.RemainingSeconds() == 60, "remaining stays 60s while paused");
    Require(sprint.Metrics().focused_seconds == 10, "tracks dwell even when paused");

    sprint.Resume();
    Require(sprint.State() == SprintState::Running, "state is Running after Resume");

    // Tick 30 seconds on focus task
    sprint.Tick(30, true);
    Require(sprint.RemainingSeconds() == 30, "remaining is 30s");
    Require(sprint.FormattedTime() == "00:30", "formatted time is 00:30");
    Require(sprint.Metrics().focused_seconds == 40, "focused seconds is 40");

    // Tick 20 seconds off focus task
    sprint.Tick(20, false);
    Require(sprint.RemainingSeconds() == 10, "remaining is 10s");
    Require(sprint.Metrics().distracted_seconds == 20, "distracted seconds is 20");
    Require(sprint.Metrics().FocusRatio() == (40.0 / 60.0), "focus ratio is 40/60");

    // Tick remaining 10 seconds -> completion
    const bool completed = sprint.Tick(10, true);
    Require(completed, "tick completing remaining time returns true");
    Require(sprint.State() == SprintState::Completed, "state is Completed");
    Require(sprint.RemainingSeconds() == 0, "remaining is 0");
    Require(sprint.FormattedTime() == "00:00", "formatted time is 00:00");
    Require(sprint.Metrics().completed_sprints == 1, "completed sprints count is 1");

    // Reset
    sprint.Reset();
    Require(sprint.State() == SprintState::Idle, "state is Idle after Reset");
    Require(sprint.RemainingSeconds() == 60, "remaining restored to duration after Reset");
}

void TestFocusSessionDwellTracking() {
    using openbrowser::core::BrowserSession;
    using openbrowser::core::FocusQueue;
    using openbrowser::core::FocusSessionController;
    using openbrowser::core::FocusSprint;

    openbrowser::tests::FakeBrowserEngine engine;
    BrowserSession session(engine);
    FocusQueue queue;
    FocusSprint sprint(1500);
    sprint.Start();

    session.OpenTab({
        .id = "work-tab",
        .url = "https://work.example.com",
        .title = "Work",
    });

    session.OpenTab({
        .id = "distract-tab",
        .url = "https://distraction.example.com",
        .title = "Distraction",
    }, false);

    // Enqueue work tab
    queue.Enqueue({
        .id = "item-1",
        .url = "https://work.example.com",
        .tab_id = "work-tab",
    });

    // Active tab is work-tab (which is in queue)
    Require(FocusSessionController::IsActiveTabInFocusQueue(session, queue), "work-tab is in focus queue");
    FocusSessionController::RecordSprintTick(sprint, session, queue, 5);
    Require(sprint.Metrics().focused_seconds == 5, "5 seconds attributed to focused");
    Require(sprint.Metrics().distracted_seconds == 0, "0 seconds distracted");

    // Switch active tab to distract-tab (not in queue)
    session.ActivateTab("distract-tab");
    Require(!FocusSessionController::IsActiveTabInFocusQueue(session, queue), "distract-tab is not in focus queue");
    FocusSessionController::RecordSprintTick(sprint, session, queue, 5);
    Require(sprint.Metrics().focused_seconds == 5, "focused seconds remains 5");
    Require(sprint.Metrics().distracted_seconds == 5, "5 seconds attributed to distracted");
    Require(sprint.Metrics().FocusRatio() == 0.5, "focus ratio is 50%");
}

}  // namespace

int main() {
    TestFocusSprintStateAndTiming();
    TestFocusSessionDwellTracking();

    if (failures != 0) {
        std::cerr << failures << " focus sprint test failure(s)\n";
        return 1;
    }

    std::cout << "Openbrowser focus sprint tests: PASS\n";
    return 0;
}
