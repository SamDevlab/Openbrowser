#include "devtools/network/network_trace.h"

#include <algorithm>
#include <array>
#include <cctype>
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

void NetworkTraceBuffer::Add(NetworkEvent event) {
    if (policy_.redact_sensitive_headers) {
        RedactSensitiveHeaders(event.headers);
    }

    event.sequence = next_sequence_++;

    while (events_.size() >= policy_.max_events) {
        events_.pop_front();
        ++dropped_event_count_;
    }

    events_.push_back(std::move(event));
}

void NetworkTraceBuffer::Clear() noexcept {
    events_.clear();
    next_sequence_ = 1;
    dropped_event_count_ = 0;
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

}  // namespace openbrowser::devtools::network
