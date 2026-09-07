#include "core/session/tab_discard_policy.h"

#include <algorithm>
#include <utility>

namespace openbrowser::core {

TabDiscardPolicy::TabDiscardPolicy(TabDiscardConfig config) noexcept
    : config_(std::move(config)) {}

std::vector<TabId> TabDiscardPolicy::SelectDiscardCandidates(
    const BrowserSession& session,
    const FocusQueue* focus_queue,
    const MemoryPressureLevel pressure_level) const {
    const auto& tabs = session.Tabs();
    const auto& active_id = session.ActiveTabId();

    struct CandidateEntry {
        TabId id;
        std::uint64_t lru_seq;
        std::size_t original_index;
    };

    std::vector<CandidateEntry> eligible;
    std::size_t total_alive_background = 0;

    for (std::size_t i = 0; i < tabs.size(); ++i) {
        const auto& tab = tabs[i];
        if (tab.lifecycle == TabLifecycle::Discarded) {
            continue;
        }
        if (active_id.has_value() && *active_id == tab.id) {
            continue;
        }

        ++total_alive_background;

        if (config_.protect_playing_audio && tab.is_playing_audio) {
            continue;
        }

        if (config_.protect_focus_queue_now && focus_queue != nullptr) {
            bool is_active_focus = false;
            for (const auto& item : focus_queue->Items()) {
                if (item.state == FocusState::Now) {
                    if (item.tab_id.has_value() && *item.tab_id == tab.id) {
                        is_active_focus = true;
                        break;
                    }
                    if (!item.url.empty() && item.url == tab.url) {
                        is_active_focus = true;
                        break;
                    }
                }
            }
            if (is_active_focus) {
                continue;
            }
        }

        eligible.push_back({tab.id, tab.last_activated_sequence, i});
    }

    if (eligible.empty()) {
        return {};
    }

    std::sort(eligible.begin(), eligible.end(), [](const CandidateEntry& a, const CandidateEntry& b) {
        if (a.lru_seq != b.lru_seq) {
            return a.lru_seq < b.lru_seq;
        }
        return a.original_index < b.original_index;
    });

    std::size_t discard_count = 0;
    switch (pressure_level) {
        case MemoryPressureLevel::Critical:
            discard_count = eligible.size();
            break;
        case MemoryPressureLevel::Moderate: {
            const std::size_t target_max = config_.max_alive_background_tabs / 2;
            if (total_alive_background > target_max) {
                discard_count = std::min(eligible.size(), total_alive_background - target_max);
            }
            break;
        }
        case MemoryPressureLevel::None:
            if (total_alive_background > config_.max_alive_background_tabs) {
                discard_count = std::min(eligible.size(), total_alive_background - config_.max_alive_background_tabs);
            }
            break;
    }

    std::vector<TabId> selected;
    selected.reserve(discard_count);
    for (std::size_t i = 0; i < discard_count; ++i) {
        selected.push_back(std::move(eligible[i].id));
    }
    return selected;
}

std::size_t TabDiscardPolicy::Enforce(
    BrowserSession& session,
    const FocusQueue* focus_queue,
    const MemoryPressureLevel pressure_level) const {
    const auto candidates = SelectDiscardCandidates(session, focus_queue, pressure_level);
    std::size_t discarded = 0;
    for (const auto& tab_id : candidates) {
        if (session.DiscardTab(tab_id)) {
            ++discarded;
        }
    }
    return discarded;
}

}  // namespace openbrowser::core
