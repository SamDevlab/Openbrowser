#include "core/focus_queue/focus_sprint.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace openbrowser::core {

FocusSprint::FocusSprint(const uint32_t default_duration_seconds) noexcept
    : default_duration_seconds_(default_duration_seconds),
      duration_seconds_(default_duration_seconds),
      remaining_seconds_(default_duration_seconds) {}

void FocusSprint::Start(const uint32_t duration_seconds) {
    if (duration_seconds > 0) {
        duration_seconds_ = duration_seconds;
    } else if (duration_seconds_ == 0) {
        duration_seconds_ = default_duration_seconds_;
    }
    remaining_seconds_ = duration_seconds_;
    state_ = SprintState::Running;
}

void FocusSprint::Pause() {
    if (state_ == SprintState::Running) {
        state_ = SprintState::Paused;
    }
}

void FocusSprint::Resume() {
    if (state_ == SprintState::Paused) {
        state_ = SprintState::Running;
    }
}

void FocusSprint::Reset() {
    state_ = SprintState::Idle;
    remaining_seconds_ = duration_seconds_;
}

bool FocusSprint::Tick(const uint32_t delta_seconds, const bool is_on_focus_task) {
    if (delta_seconds == 0) {
        return false;
    }

    if (is_on_focus_task) {
        metrics_.focused_seconds += delta_seconds;
    } else {
        metrics_.distracted_seconds += delta_seconds;
    }

    if (state_ != SprintState::Running) {
        return false;
    }

    if (remaining_seconds_ <= delta_seconds) {
        remaining_seconds_ = 0;
        state_ = SprintState::Completed;
        metrics_.completed_sprints += 1;
        return true;
    }

    remaining_seconds_ -= delta_seconds;
    return false;
}

std::string FocusSprint::FormattedTime() const {
    const uint32_t minutes = remaining_seconds_ / 60;
    const uint32_t seconds = remaining_seconds_ % 60;

    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << minutes << ':'
        << std::setw(2) << std::setfill('0') << seconds;
    return oss.str();
}

std::string FocusSprint::FormattedMetrics() const {
    const uint64_t focus_min = metrics_.focused_seconds / 60;
    const uint64_t distract_min = metrics_.distracted_seconds / 60;
    const int percentage = static_cast<int>(std::round(metrics_.FocusRatio() * 100.0));

    std::ostringstream oss;
    oss << "Focus: " << focus_min << "m | Off-task: " << distract_min << "m ("
        << percentage << "%) | Sprints: " << metrics_.completed_sprints;
    return oss.str();
}

}  // namespace openbrowser::core
