#include "devtools/network/network_trace.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>
#include <utility>

namespace openbrowser::devtools::network {
namespace {

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

}  // namespace

NetworkTraceBuffer::NetworkTraceBuffer(CapturePolicy policy) : policy_(std::move(policy)) {
    if (policy_.max_events == 0) {
        policy_.max_events = 1;
    }
}

void NetworkTraceBuffer::AddObserver(NetworkTraceObserver* observer) {
    if (observer == nullptr) {
        return;
    }
    if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end()) {
        observers_.push_back(observer);
    }
}

void NetworkTraceBuffer::RemoveObserver(NetworkTraceObserver* observer) noexcept {
    observers_.erase(
        std::remove(observers_.begin(), observers_.end(), observer),
        observers_.end());
}

void NetworkTraceBuffer::OnNetworkEvent(NetworkEvent event) {
    Add(std::move(event));
}

void NetworkTraceBuffer::Add(NetworkEvent event) {
    if (policy_.redact_sensitive_headers) {
        RedactSensitiveHeaders(event.headers);
    }

    event.sequence = next_sequence_++;

    while (events_.size() >= policy_.max_events) {
        events_.pop_front();
        ++dropped_event_count_;
    }

    events_.push_back(event);
    NotifyEventAppended(event);
}

void NetworkTraceBuffer::Clear() noexcept {
    events_.clear();
    next_sequence_ = 1;
    dropped_event_count_ = 0;
    NotifyTraceCleared();
}

const std::deque<NetworkEvent>& NetworkTraceBuffer::Events() const noexcept {
    return events_;
}

std::size_t NetworkTraceBuffer::DroppedEventCount() const noexcept {
    return dropped_event_count_;
}

const CapturePolicy& NetworkTraceBuffer::Policy() const noexcept {
    return policy_;
}

std::vector<NetworkRequestSummary> NetworkTraceBuffer::AggregateRequests(
    const std::optional<core::TabId>& filter_tab_id) const {
    std::vector<NetworkRequestSummary> summaries;
    std::unordered_map<std::string, std::size_t> request_index_map;

    for (const auto& event : events_) {
        if (filter_tab_id.has_value() && event.tab_id.has_value() && *event.tab_id != *filter_tab_id) {
            continue;
        }

        auto it = request_index_map.find(event.request_id);
        if (it == request_index_map.end()) {
            NetworkRequestSummary summary{
                .request_id = event.request_id,
                .tab_id = event.tab_id,
                .method = event.method.empty() ? "GET" : event.method,
                .url = event.url,
                .status = event.status,
                .protocol = event.protocol,
                .transferred_bytes = event.transferred_bytes,
                .state = RequestState::Active,
                .error = event.error,
            };

            if (event.type == NetworkEventType::RequestFinished) {
                summary.state = RequestState::Finished;
            } else if (event.type == NetworkEventType::RequestFailed) {
                summary.state = RequestState::Failed;
            }

            request_index_map[event.request_id] = summaries.size();
            summaries.push_back(std::move(summary));
        } else {
            auto& summary = summaries[it->second];
            if (!event.url.empty()) {
                summary.url = event.url;
            }
            if (!event.method.empty()) {
                summary.method = event.method;
            }
            if (event.status.has_value()) {
                summary.status = event.status;
            }
            if (!event.protocol.empty()) {
                summary.protocol = event.protocol;
            }
            if (!event.tab_id.has_value() && summary.tab_id.has_value()) {
                // keep tab_id
            } else if (event.tab_id.has_value()) {
                summary.tab_id = event.tab_id;
            }
            summary.transferred_bytes += event.transferred_bytes;

            if (event.type == NetworkEventType::RequestFinished) {
                summary.state = RequestState::Finished;
            } else if (event.type == NetworkEventType::RequestFailed) {
                summary.state = RequestState::Failed;
                summary.error = event.error;
            }
        }
    }

    return summaries;
}

bool NetworkTraceBuffer::IsSensitiveHeaderName(const std::string& name) {
    static constexpr std::array sensitive_names{
        "authorization",
        "proxy-authorization",
        "cookie",
        "set-cookie",
        "x-api-key",
        "x-auth-token",
        "x-csrf-token",
        "x-xsrf-token",
    };

    const auto normalized = LowerAscii(name);
    return std::find(sensitive_names.begin(), sensitive_names.end(), normalized) != sensitive_names.end();
}

void NetworkTraceBuffer::RedactSensitiveHeaders(std::vector<Header>& headers) {
    for (auto& header : headers) {
        if (IsSensitiveHeaderName(header.name)) {
            header.value = "<redacted>";
        }
    }
}

void NetworkTraceBuffer::NotifyEventAppended(const NetworkEvent& event) {
    const auto observers_copy = observers_;
    for (auto* observer : observers_copy) {
        if (observer != nullptr) {
            observer->OnTraceEventAppended(event);
        }
    }
}

void NetworkTraceBuffer::NotifyTraceCleared() {
    const auto observers_copy = observers_;
    for (auto* observer : observers_copy) {
        if (observer != nullptr) {
            observer->OnTraceCleared();
        }
    }
}

}  // namespace openbrowser::devtools::network
