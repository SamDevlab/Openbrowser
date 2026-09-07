#pragma once

#include <cstdint>
#include <string>

namespace openbrowser::core {

enum class SprintState {
    Idle,
    Running,
    Paused,
    Completed,
};

struct AttentionMetrics {
    uint64_t focused_seconds{0};
    uint64_t distracted_seconds{0};
    uint32_t completed_sprints{0};

    [[nodiscard]] double FocusRatio() const noexcept {
        const uint64_t total = focused_seconds + distracted_seconds;
        if (total == 0) {
            return 1.0;
        }
        return static_cast<double>(focused_seconds) / static_cast<double>(total);
    }
};

class FocusSprint {
public:
    explicit FocusSprint(uint32_t default_duration_seconds = 1500) noexcept;

    void Start(uint32_t duration_seconds = 0);
    void Pause();
    void Resume();
    void Reset();

    // Advances time by delta_seconds. Returns true if sprint just completed on this tick.
    bool Tick(uint32_t delta_seconds, bool is_on_focus_task);

    [[nodiscard]] SprintState State() const noexcept { return state_; }
    [[nodiscard]] uint32_t DurationSeconds() const noexcept { return duration_seconds_; }
    [[nodiscard]] uint32_t RemainingSeconds() const noexcept { return remaining_seconds_; }
    [[nodiscard]] const AttentionMetrics& Metrics() const noexcept { return metrics_; }
    [[nodiscard]] AttentionMetrics& MutableMetrics() noexcept { return metrics_; }

    [[nodiscard]] std::string FormattedTime() const;
    [[nodiscard]] std::string FormattedMetrics() const;

private:
    SprintState state_{SprintState::Idle};
    uint32_t default_duration_seconds_{1500};
    uint32_t duration_seconds_{1500};
    uint32_t remaining_seconds_{1500};
    AttentionMetrics metrics_{};
};

}  // namespace openbrowser::core
