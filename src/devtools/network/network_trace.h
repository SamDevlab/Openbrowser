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

enum class RequestState {
    Active,
    Finished,
    Failed,
};

struct NetworkRequestSummary {
    std::string request_id;
    std::optional<core::TabId> tab_id;
    std::string method{"GET"};
    std::string url;
    std::optional<int> status;
    std::string protocol;
    std::size_t transferred_bytes{0};
    RequestState state{RequestState::Active};
    std::string error;
};

class NetworkTraceObserver {
public:
    virtual ~NetworkTraceObserver() = default;
    virtual void OnTraceEventAppended(const NetworkEvent& event) = 0;
    virtual void OnTraceCleared() = 0;
};

class NetworkTraceBuffer final : public NetworkObservationSink {
public:
    explicit NetworkTraceBuffer(CapturePolicy policy = {});
    ~NetworkTraceBuffer() override = default;

    void AddObserver(NetworkTraceObserver* observer);
    void RemoveObserver(NetworkTraceObserver* observer) noexcept;

    void OnNetworkEvent(NetworkEvent event) override;
    void Add(NetworkEvent event);
    void Clear() noexcept;

    [[nodiscard]] const std::deque<NetworkEvent>& Events() const noexcept;
    [[nodiscard]] std::size_t DroppedEventCount() const noexcept;
    [[nodiscard]] const CapturePolicy& Policy() const noexcept;

    [[nodiscard]] std::vector<NetworkRequestSummary> AggregateRequests(
        const std::optional<core::TabId>& filter_tab_id = std::nullopt) const;

    [[nodiscard]] static bool IsSensitiveHeaderName(const std::string& name);
    static void RedactSensitiveHeaders(std::vector<Header>& headers);

private:
    void NotifyEventAppended(const NetworkEvent& event);
    void NotifyTraceCleared();

    CapturePolicy policy_;
    std::deque<NetworkEvent> events_;
    std::vector<NetworkTraceObserver*> observers_;
    std::uint64_t next_sequence_{1};
    std::size_t dropped_event_count_{0};
};

}  // namespace openbrowser::devtools::network
