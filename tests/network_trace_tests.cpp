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

void TestAdvancedNetworkLabQueryAndDetails() {
    using openbrowser::devtools::network::Header;
    using openbrowser::devtools::network::NetworkEventType;
    using openbrowser::devtools::network::NetworkTraceBuffer;
    using openbrowser::devtools::network::NetworkTraceFilter;

    NetworkTraceBuffer buffer;

    // Request 1: GET 200 with headers
    auto req1_start = MakeEvent("req-1");
    req1_start.method = "GET";
    req1_start.url = "https://api.example.com/v1/users";
    req1_start.headers = {Header{.name = "Accept", .value = "application/json"}};
    buffer.Add(req1_start);

    auto req1_res = MakeEvent("req-1");
    req1_res.type = NetworkEventType::ResponseReceived;
    req1_res.status = 200;
    req1_res.headers = {Header{.name = "Content-Type", .value = "application/json; charset=utf-8"}};
    buffer.Add(req1_res);

    auto req1_fin = MakeEvent("req-1");
    req1_fin.type = NetworkEventType::RequestFinished;
    buffer.Add(req1_fin);

    // Request 2: POST 404 with body preview
    auto req2_start = MakeEvent("req-2");
    req2_start.method = "POST";
    req2_start.url = "https://api.example.com/v1/login";
    req2_start.body_preview = "{\"username\":\"samuel\",\"remember\":true}";
    req2_start.headers = {Header{.name = "Content-Type", .value = "application/json"}};
    buffer.Add(req2_start);

    auto req2_res = MakeEvent("req-2");
    req2_res.type = NetworkEventType::ResponseReceived;
    req2_res.status = 404;
    buffer.Add(req2_res);

    auto req2_fin = MakeEvent("req-2");
    req2_fin.type = NetworkEventType::RequestFinished;
    buffer.Add(req2_fin);

    // 1. FindRequest & Detail Integrity
    const auto details1 = buffer.FindRequest("req-1");
    Require(details1.has_value(), "FindRequest finds req-1");
    Require(details1->request_headers.size() == 1, "req-1 has 1 request header");
    Require(details1->request_headers[0].name == "Accept", "request header name");
    Require(details1->response_headers.size() == 1, "req-1 has 1 response header");
    Require(details1->response_headers[0].name == "Content-Type", "response header name");
    Require(details1->body_preview.empty(), "req-1 has empty body preview");

    const auto details2 = buffer.FindRequest("req-2");
    Require(details2.has_value(), "FindRequest finds req-2");
    Require(details2->body_preview == "{\"username\":\"samuel\",\"remember\":true}", "req-2 preserves body preview");

    // 2. Method filtering
    const auto post_only = buffer.QueryRequests(NetworkTraceFilter{.method_filter = "POST"});
    Require(post_only.size() == 1, "method filter POST returns 1 result");
    Require(post_only[0].request_id == "req-2", "post filter matches req-2");

    const auto get_only = buffer.QueryRequests(NetworkTraceFilter{.method_filter = "GET"});
    Require(get_only.size() == 1, "method filter GET returns 1 result");
    Require(get_only[0].request_id == "req-1", "get filter matches req-1");

    // 3. Status filtering
    const auto status_2xx = buffer.QueryRequests(NetworkTraceFilter{.status_filter = "2XX"});
    Require(status_2xx.size() == 1, "status 2XX returns 1 result");
    Require(status_2xx[0].request_id == "req-1", "status 2XX matches req-1");

    const auto status_err = buffer.QueryRequests(NetworkTraceFilter{.status_filter = "ERR"});
    Require(status_err.size() == 1, "status ERR matches 404 response");
    Require(status_err[0].request_id == "req-2", "status ERR matches req-2");

    // 4. Search query on URL, header, and body preview
    const auto search_url = buffer.QueryRequests(NetworkTraceFilter{.search_query = "users"});
    Require(search_url.size() == 1 && search_url[0].request_id == "req-1", "search by URL substring");

    const auto search_body = buffer.QueryRequests(NetworkTraceFilter{.search_query = "samuel"});
    Require(search_body.size() == 1 && search_body[0].request_id == "req-2", "search by body preview snippet");

    const auto search_hdr = buffer.QueryRequests(NetworkTraceFilter{.search_query = "charset"});
    Require(search_hdr.size() == 1 && search_hdr[0].request_id == "req-1", "search by response header value");

    // 5. Header name filter
    const auto filter_hdr = buffer.QueryRequests(NetworkTraceFilter{.header_name = "Accept"});
    Require(filter_hdr.size() == 1 && filter_hdr[0].request_id == "req-1", "header name filter matches");
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
    TestAdvancedNetworkLabQueryAndDetails();

    if (failures != 0) {
        std::cerr << failures << " Network Lab test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser Network Lab trace invariants: PASS\n";
    return 0;
}

