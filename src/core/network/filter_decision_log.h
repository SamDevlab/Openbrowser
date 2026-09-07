#pragma once

// M7.2: Privacy/Filter Decision Log & Explanation Engine
//
// Records per-request block/allow decisions from all policy layers so the
// Network Lab can display a human-readable explanation for every decision.
//
// Note: core::FilterDecision (the Allow/Block enum) already exists in
// content_filter.h.  This file defines the *record* type
// FilterDecisionRecord and the supporting infrastructure.

#include <cstddef>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace openbrowser::core {

// Which policy layer produced a decision.
enum class FilterDecisionLayer {
    ContentFilter,   // tracker/ad block rules
    CapabilityPolicy, // browser/page capability gate
    WorkspacePolicy, // workspace container isolation
    UserAgentPolicy, // UA / compatibility mitigation
};

[[nodiscard]] const char* FilterDecisionLayerToString(FilterDecisionLayer layer) noexcept;

// Full record of a single policy evaluation for one request.
struct FilterDecisionRecord {
    std::string request_id;
    std::string url;
    bool blocked{false};              // true → request was blocked/denied
    FilterDecisionLayer layer{FilterDecisionLayer::ContentFilter};
    std::string rule_source;          // e.g. "easylist#3412", "capability:camera"
    std::string scope;                // "global", "workspace:<id>", "site:<origin>"
    std::string human_explanation;    // human-readable one-line summary
};

// Observer interface — implemented by NetworkLabPanel to react to decisions.
class FilterDecisionSink {
public:
    virtual ~FilterDecisionSink() = default;
    virtual void OnFilterDecision(const FilterDecisionRecord& decision) = 0;
};

// Bounded circular buffer of FilterDecisionRecord.
// Thread-safe: may be called from network/IO threads.
class FilterDecisionLog {
public:
    static constexpr std::size_t kDefaultCapacity = 2000;

    explicit FilterDecisionLog(std::size_t capacity = kDefaultCapacity);
    ~FilterDecisionLog() = default;

    FilterDecisionLog(const FilterDecisionLog&) = delete;
    FilterDecisionLog& operator=(const FilterDecisionLog&) = delete;

    // Append a decision record; oldest entry evicted if at capacity.
    void AddDecision(FilterDecisionRecord record);

    // Set an optional live observer (non-owning pointer, may be nullptr).
    void SetSink(FilterDecisionSink* sink) noexcept;

    [[nodiscard]] std::vector<FilterDecisionRecord> All() const;

    [[nodiscard]] std::optional<FilterDecisionRecord>
    FindByRequestId(const std::string& request_id) const;

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] std::size_t Capacity() const noexcept;
    [[nodiscard]] std::size_t DroppedCount() const noexcept;

    void Clear() noexcept;

private:
    std::size_t capacity_;
    std::deque<FilterDecisionRecord> entries_;
    std::size_t dropped_count_{0};
    FilterDecisionSink* sink_{nullptr};
    mutable bool mutex_guard_{false}; // simple reentrancy guard (single-thread core)
};

}  // namespace openbrowser::core
