#include "core/network/filter_decision_log.h"

#include <algorithm>

namespace openbrowser::core {

const char* FilterDecisionLayerToString(FilterDecisionLayer layer) noexcept {
    switch (layer) {
        case FilterDecisionLayer::ContentFilter:   return "ContentFilter";
        case FilterDecisionLayer::CapabilityPolicy: return "CapabilityPolicy";
        case FilterDecisionLayer::WorkspacePolicy: return "WorkspacePolicy";
        case FilterDecisionLayer::UserAgentPolicy: return "UserAgentPolicy";
    }
    return "Unknown";
}

FilterDecisionLog::FilterDecisionLog(std::size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity) {}

void FilterDecisionLog::AddDecision(FilterDecisionRecord record) {
    if (entries_.size() >= capacity_) {
        entries_.pop_front();
        ++dropped_count_;
    }
    if (sink_ != nullptr) {
        sink_->OnFilterDecision(record);
    }
    entries_.push_back(std::move(record));
}

void FilterDecisionLog::SetSink(FilterDecisionSink* sink) noexcept {
    sink_ = sink;
}

std::vector<FilterDecisionRecord> FilterDecisionLog::All() const {
    return {entries_.begin(), entries_.end()};
}

std::optional<FilterDecisionRecord>
FilterDecisionLog::FindByRequestId(const std::string& request_id) const {
    // Search from newest to oldest.
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->request_id == request_id) {
            return *it;
        }
    }
    return std::nullopt;
}

std::size_t FilterDecisionLog::Size() const noexcept {
    return entries_.size();
}

std::size_t FilterDecisionLog::Capacity() const noexcept {
    return capacity_;
}

std::size_t FilterDecisionLog::DroppedCount() const noexcept {
    return dropped_count_;
}

void FilterDecisionLog::Clear() noexcept {
    entries_.clear();
    dropped_count_ = 0;
}

}  // namespace openbrowser::core
