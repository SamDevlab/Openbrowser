#pragma once

#include "core/tabs/tab.h"
#include "devtools/network/network_observation_sink.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace openbrowser::devtools::network {

enum class NetworkEventType {
    RequestStarted,
    Redirect,
    ResponseReceived,
    DataReceived,
    RequestFinished,
    RequestFailed,
    WebSocketOpened,
    WebSocketFrame,
    WebSocketClosed,
};

struct Header {
    std::string name;
    std::string value;
};

struct NetworkEvent {
    std::uint64_t sequence{0};
    std::string request_id;
    std::optional<core::TabId> tab_id;
    NetworkEventType type{NetworkEventType::RequestStarted};
    std::string url;
    std::string method;
    std::optional<int> status;
    std::string protocol;
    std::vector<Header> headers;
    std::size_t transferred_bytes{0};
    std::string error;
};

struct CapturePolicy {
    std::size_t max_events{5000};
    bool redact_sensitive_headers{true};
};

class NetworkTraceBuffer final : public NetworkObservationSink {
public:
    explicit NetworkTraceBuffer(CapturePolicy policy = {});

    void OnNetworkEvent(NetworkEvent event) override;
    void Add(NetworkEvent event);
    void Clear() noexcept;

    [[nodiscard]] const std::deque<NetworkEvent>& Events() const noexcept;
    [[nodiscard]] std::size_t DroppedEventCount() const noexcept;
    [[nodiscard]] const CapturePolicy& Policy() const noexcept;

    [[nodiscard]] static bool IsSensitiveHeaderName(const std::string& name);
    static void RedactSensitiveHeaders(std::vector<Header>& headers);

private:
    CapturePolicy policy_;
    std::deque<NetworkEvent> events_;
    std::uint64_t next_sequence_{1};
    std::size_t dropped_event_count_{0};
};

}  // namespace openbrowser::devtools::network
