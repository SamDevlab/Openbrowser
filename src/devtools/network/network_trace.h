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

namespace openbrowser::core {
class FilterDecisionLog;
}

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
    // Cross-reference to ConnectionRegistry when one is actually observed.
    std::optional<std::string> connection_id;
    // Wall-clock time when Openbrowser accepted this observation. This is not
    // transport-phase telemetry and must not be presented as DNS/TLS timing.
    std::int64_t observed_at_ms{0};
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
    std::optional<std::string> connection_id;
    bool filter_blocked{false};
    std::string filter_rule_source;
    std::string filter_layer;
    // Span between first and last callbacks observed for this request.
    std::int64_t started_at_ms{0};
    std::int64_t ended_at_ms{0};
    std::int64_t observed_duration_ms{0};
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

    // Safe-by-default diagnostic exports. Headers, credential-like URL query
    // values and captured body previews are redacted defensively here even if
    // the caller supplies summaries that did not originate in the default
    // redacting capture policy.
    [[nodiscard]] static std::string ExportToHar(
        const std::vector<NetworkRequestSummary>& requests);

    [[nodiscard]] static std::string ExportToObtrace(
        const std::vector<NetworkRequestSummary>& requests);

    void SetDecisionLog(const core::FilterDecisionLog* log) noexcept;
    [[nodiscard]] const core::FilterDecisionLog* DecisionLog() const noexcept;

    [[nodiscard]] static bool IsSensitiveHeaderName(const std::string& name);
    static void RedactSensitiveHeaders(std::vector<Header>& headers);
    [[nodiscard]] static std::string RedactSensitiveUrl(std::string url);

private:
    void NotifyEventAppended(const NetworkEvent& event);
    void NotifyTraceCleared();

    CapturePolicy policy_;
    std::deque<NetworkEvent> events_;
    std::vector<NetworkTraceObserver*> observers_;
    const core::FilterDecisionLog* decision_log_{nullptr};
    std::uint64_t next_sequence_{1};
    std::size_t dropped_event_count_{0};
};

}  // namespace openbrowser::devtools::network
