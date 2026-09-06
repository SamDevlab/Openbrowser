#include "devtools/network/network_trace.h"

#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

openbrowser::devtools::network::NetworkEvent MakeEvent(const std::string& request_id) {
    return openbrowser::devtools::network::NetworkEvent{
        .sequence = 0,
        .request_id = request_id,
        .tab_id = std::nullopt,
        .type = openbrowser::devtools::network::NetworkEventType::RequestStarted,
        .url = "https://example.test/api",
        .method = "GET",
        .status = std::nullopt,
        .protocol = "h2",
        .headers = {},
        .transferred_bytes = 0,
        .error = {},
    };
}

void TestSensitiveHeadersAreRedactedByDefault() {
    using openbrowser::devtools::network::Header;
    using openbrowser::devtools::network::NetworkTraceBuffer;

    NetworkTraceBuffer buffer;
    auto event = MakeEvent("request-1");
    event.headers = {
        Header{.name = "Authorization", .value = "Bearer secret"},
        Header{.name = "cookie", .value = "session=secret"},
        Header{.name = "X-Csrf-Token", .value = "csrf-secret"},
        Header{.name = "Content-Type", .value = "application/json"},
    };

    buffer.Add(std::move(event));

    const auto& headers = buffer.Events().front().headers;
    Require(headers[0].value == "<redacted>", "authorization is redacted case-insensitively");
    Require(headers[1].value == "<redacted>", "cookie is redacted");
    Require(headers[2].value == "<redacted>", "CSRF token is redacted");
    Require(headers[3].value == "application/json", "non-sensitive header is preserved");
}

void TestExplicitRawHeaderPolicyIsPossible() {
    using openbrowser::devtools::network::CapturePolicy;
    using openbrowser::devtools::network::Header;
    using openbrowser::devtools::network::NetworkTraceBuffer;

    NetworkTraceBuffer buffer(CapturePolicy{.max_events = 8, .redact_sensitive_headers = false});
    auto event = MakeEvent("request-raw");
    event.headers = {Header{.name = "X-Api-Key", .value = "explicit-secret"}};

    buffer.Add(std::move(event));

    Require(
        buffer.Events().front().headers.front().value == "explicit-secret",
        "explicit raw-header policy preserves sensitive values");
}

void TestBufferOwnsMonotonicSequence() {
    using openbrowser::devtools::network::NetworkTraceBuffer;

    NetworkTraceBuffer buffer;
    auto first = MakeEvent("request-1");
    first.sequence = 999;
    auto second = MakeEvent("request-2");
    second.sequence = 42;

    buffer.Add(std::move(first));
    buffer.Add(std::move(second));

    Require(buffer.Events()[0].sequence == 1, "adapter-provided sequence is ignored");
    Require(buffer.Events()[1].sequence == 2, "trace buffer owns monotonic sequence");
}

void TestBufferIsBoundedAndReportsDroppedEvents() {
    using openbrowser::devtools::network::CapturePolicy;
    using openbrowser::devtools::network::NetworkTraceBuffer;

    NetworkTraceBuffer buffer(CapturePolicy{.max_events = 2, .redact_sensitive_headers = true});
    buffer.Add(MakeEvent("request-1"));
    buffer.Add(MakeEvent("request-2"));
    buffer.Add(MakeEvent("request-3"));

    Require(buffer.Events().size() == 2, "trace buffer obeys configured event bound");
    Require(buffer.DroppedEventCount() == 1, "trace buffer counts evicted events");
    Require(buffer.Events().front().request_id == "request-2", "oldest event is evicted first");
    Require(buffer.Events().front().sequence == 2, "assigned sequence remains monotonic after eviction");
    Require(buffer.Events().back().sequence == 3, "latest event receives next sequence");
}

void TestZeroCapacityIsNormalizedToOne() {
    using openbrowser::devtools::network::CapturePolicy;
    using openbrowser::devtools::network::NetworkTraceBuffer;

    NetworkTraceBuffer buffer(CapturePolicy{.max_events = 0, .redact_sensitive_headers = true});
    Require(buffer.Policy().max_events == 1, "zero event capacity normalizes to one");

    buffer.Add(MakeEvent("request-1"));
    buffer.Add(MakeEvent("request-2"));
    Require(buffer.Events().size() == 1, "normalized capacity remains bounded");
    Require(buffer.Events().front().request_id == "request-2", "latest event survives single-slot buffer");
}

void TestClearResetsCaptureSessionCounters() {
    using openbrowser::devtools::network::CapturePolicy;
    using openbrowser::devtools::network::NetworkTraceBuffer;

    NetworkTraceBuffer buffer(CapturePolicy{.max_events = 1, .redact_sensitive_headers = true});
    buffer.Add(MakeEvent("request-1"));
    buffer.Add(MakeEvent("request-2"));
    Require(buffer.DroppedEventCount() == 1, "precondition: one event dropped");

    buffer.Clear();
    Require(buffer.Events().empty(), "clear removes trace events");
    Require(buffer.DroppedEventCount() == 0, "clear resets dropped count");

    buffer.Add(MakeEvent("request-3"));
    Require(buffer.Events().front().sequence == 1, "clear starts a new local trace sequence");
}

class TestObserver final : public openbrowser::devtools::network::NetworkTraceObserver {
public:
    void OnTraceEventAppended(const openbrowser::devtools::network::NetworkEvent& event) override {
        ++appended_count;
        last_request_id = event.request_id;
    }
    void OnTraceCleared() override {
        ++cleared_count;
    }
    int appended_count{0};
    int cleared_count{0};
    std::string last_request_id;
};

void TestObserverNotifications() {
    using openbrowser::devtools::network::NetworkTraceBuffer;

    NetworkTraceBuffer buffer;
    TestObserver observer;
    buffer.AddObserver(&observer);

    buffer.Add(MakeEvent("req-1"));
    Require(observer.appended_count == 1, "observer notified of appended event");
    Require(observer.last_request_id == "req-1", "observer received correct request ID");

    buffer.Clear();
    Require(observer.cleared_count == 1, "observer notified of trace clear");

    buffer.RemoveObserver(&observer);
    buffer.Add(MakeEvent("req-2"));
    Require(observer.appended_count == 1, "unregistered observer does not receive events");
}

void TestAggregateRequests() {
    using openbrowser::devtools::network::NetworkEventType;
    using openbrowser::devtools::network::NetworkTraceBuffer;
    using openbrowser::devtools::network::RequestState;

    NetworkTraceBuffer buffer;

    auto e1 = MakeEvent("req-1");
    e1.tab_id = "tab-a";
    e1.method = "GET";
    e1.url = "https://test.com/api";
    buffer.Add(e1);

    auto e2 = MakeEvent("req-1");
    e2.tab_id = "tab-a";
    e2.type = NetworkEventType::ResponseReceived;
    e2.status = 200;
    e2.protocol = "h2";
    buffer.Add(e2);

    auto e3 = MakeEvent("req-1");
    e3.tab_id = "tab-a";
    e3.type = NetworkEventType::DataReceived;
    e3.transferred_bytes = 1024;
    buffer.Add(e3);

    auto e4 = MakeEvent("req-1");
    e4.tab_id = "tab-a";
    e4.type = NetworkEventType::DataReceived;
    e4.transferred_bytes = 512;
    buffer.Add(e4);

    auto e5 = MakeEvent("req-1");
    e5.tab_id = "tab-a";
    e5.type = NetworkEventType::RequestFinished;
    buffer.Add(e5);

    auto e6 = MakeEvent("req-2");
    e6.tab_id = "tab-b";
    e6.method = "POST";
    e6.url = "https://other.com/submit";
    e6.type = NetworkEventType::RequestFailed;
    e6.error = "ERR_CONNECTION_REFUSED";
    buffer.Add(e6);

    const auto all = buffer.AggregateRequests();
    Require(all.size() == 2, "aggregates 2 requests");
    Require(all[0].request_id == "req-1", "req-1 id");
    Require(all[0].status == std::optional<int>{200}, "req-1 status 200");
    Require(all[0].transferred_bytes == 1536, "req-1 total transferred bytes 1536");
    Require(all[0].state == RequestState::Finished, "req-1 finished");

    Require(all[1].request_id == "req-2", "req-2 id");
    Require(all[1].method == "POST", "req-2 method POST");
    Require(all[1].state == RequestState::Failed, "req-2 state failed");
    Require(all[1].error == "ERR_CONNECTION_REFUSED", "req-2 error string");

    const auto filtered = buffer.AggregateRequests("tab-a");
    Require(filtered.size() == 1, "filtered by tab-a returns 1 request");
    Require(filtered[0].request_id == "req-1", "filtered request is req-1");
}

}  // namespace

int main() {
    TestSensitiveHeadersAreRedactedByDefault();
    TestExplicitRawHeaderPolicyIsPossible();
    TestBufferOwnsMonotonicSequence();
    TestBufferIsBoundedAndReportsDroppedEvents();
    TestZeroCapacityIsNormalizedToOne();
    TestClearResetsCaptureSessionCounters();
    TestObserverNotifications();
    TestAggregateRequests();

    if (failures != 0) {
        std::cerr << failures << " Network Lab test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser Network Lab trace invariants: PASS\n";
    return 0;
}
