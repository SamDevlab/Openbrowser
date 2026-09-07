#pragma once

#include "core/network/network_attribution.h"
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
    // M7.1 — connection / transport diagnostics
    DnsResolved,
    ConnectionEstablished,
    TlsHandshaked,
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
    std::string body_preview;
    core::NetworkAttribution attribution{core::NetworkAttribution::Page};
    // M7.1 — cross-reference to ConnectionRegistry
    std::optional<std::string> connection_id;
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
    std::vector<Header> request_headers;
    std::vector<Header> response_headers;
    std::string body_preview;
    core::NetworkAttribution attribution{core::NetworkAttribution::Page};
    // M7.1 — cross-reference to ConnectionRegistry
    std::optional<std::string> connection_id;
    // M7.2 — last filter decision for this request (set by FilterDecisionLog)
    bool filter_blocked{false};
    std::string filter_rule_source;
    std::string filter_layer;
};

struct NetworkTraceFilter {
    std::optional<core::TabId> tab_id{std::nullopt};
    std::string search_query{};
    std::string method_filter{};   // "ALL", "GET", "POST", etc. Empty means ALL.
    std::string status_filter{};   // "ALL", "2XX", "3XX", "4XX", "5XX", "ERR". Empty means ALL.
    std::string header_name{};     // Header name substring or match if non-empty.
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

    [[nodiscard]] std::vector<NetworkRequestSummary> QueryRequests(
        const NetworkTraceFilter& filter) const;

    [[nodiscard]] std::optional<NetworkRequestSummary> FindRequest(
        const std::string& request_id) const;

    [[nodiscard]] static std::string ExportToHar(
        const std::vector<NetworkRequestSummary>& requests);

    // M7.3 — export to versioned NDJSON .obtrace format
    [[nodiscard]] static std::string ExportToObtrace(
        const std::vector<NetworkRequestSummary>& requests);

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
