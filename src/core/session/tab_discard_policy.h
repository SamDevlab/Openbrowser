#pragma once

#include "core/focus_queue/focus_queue.h"
#include "core/session/browser_session.h"
#include "core/tabs/tab.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openbrowser::core {

enum class MemoryPressureLevel {
    None,
    Moderate,
    Critical,
};

struct TabDiscardConfig {
    std::size_t max_alive_background_tabs{4};
    bool protect_focus_queue_now{true};
    bool protect_playing_audio{true};
};

class TabDiscardPolicy {
public:
    explicit TabDiscardPolicy(TabDiscardConfig config = {}) noexcept;

    // Evaluates which background tabs should be discarded based on memory pressure level and policy limits.
    // Candidates are returned in order of eviction (least recently activated first).
    [[nodiscard]] std::vector<TabId> SelectDiscardCandidates(
        const BrowserSession& session,
        const FocusQueue* focus_queue = nullptr,
        MemoryPressureLevel pressure_level = MemoryPressureLevel::None) const;

    // Executes DiscardTab on the session for all selected candidates.
    // Returns the number of tabs successfully discarded.
    std::size_t Enforce(
        BrowserSession& session,
        const FocusQueue* focus_queue = nullptr,
        MemoryPressureLevel pressure_level = MemoryPressureLevel::None) const;

    [[nodiscard]] const TabDiscardConfig& Config() const noexcept { return config_; }
    void SetConfig(TabDiscardConfig config) noexcept { config_ = config; }

private:
    TabDiscardConfig config_;
};

}  // namespace openbrowser::core
