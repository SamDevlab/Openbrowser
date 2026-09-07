// MVP-6 + MVP-7 Network Lab stabilization tests

#include "core/network/filter_decision_log.h"
#include "core/network/network_filter_query.h"
#include "devtools/network/connection_diagnostics.h"
#include "devtools/network/network_trace.h"
#include "devtools/network/obtrace_recorder.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace openbrowser;
using namespace openbrowser::devtools::network;
using namespace openbrowser::core;

static void TestConnectionRegistryCrossReferencedInSummary() {
    NetworkRequestSummary req;
    req.request_id = "req-cross";
    req.url = "https://example.com/";
    req.connection_id = "conn-17";
    req.state = RequestState::Finished;

    const auto result = NetworkTraceBuffer::ExportToObtrace({req});
    assert(result.find("conn-17") != std::string::npos);
    assert(result.find("req-cross") != std::string::npos);
    std::cout << "PASS: TestConnectionRegistryCrossReferencedInSummary\n";
}

static void TestFilterDecisionLogIntegration() {
    FilterDecisionLog log(100);

    FilterDecisionRecord rec;
    rec.request_id = "req-decision";
    rec.url = "https://blocked.tracker.example/pixel";
    rec.blocked = true;
    rec.layer = FilterDecisionLayer::ContentFilter;
    rec.rule_source = "blocked.tracker.example";
    rec.scope = "global";
    rec.human_explanation = "Blocked by tracker rule: blocked.tracker.example";
    log.AddDecision(rec);

    assert(log.Size() == 1);
    const auto found = log.FindByRequestId("req-decision");
    assert(found.has_value());
    assert(found->blocked);
    assert(found->layer == FilterDecisionLayer::ContentFilter);
    std::cout << "PASS: TestFilterDecisionLogIntegration\n";
}

static void TestFilterDecisionInObtrace() {
    NetworkRequestSummary req;
    req.request_id = "req-dec";
    req.url = "https://tracker.example/pixel";
    req.filter_blocked = true;
    req.filter_layer = "ContentFilter";
    req.filter_rule_source = "tracker.example";
    req.state = RequestState::Failed;

    const auto result = NetworkTraceBuffer::ExportToObtrace({req});
    assert(result.find("filter_blocked\":true") != std::string::npos);
    assert(result.find("ContentFilter") != std::string::npos);
    std::cout << "PASS: TestFilterDecisionInObtrace\n";
}

static void TestObtraceRecorderObservesThroughBuffer() {
    NetworkTraceBuffer buf;
    ObtraceRecorder recorder;
    buf.AddObserver(&recorder);

    NetworkEvent ev;
    ev.request_id = "r-silent";
    ev.url = "https://no.recording/yet";
    ev.type = NetworkEventType::RequestStarted;
    buf.Add(ev);

    assert(recorder.GetRecordedEventCount() == 0);
    buf.RemoveObserver(&recorder);
    std::cout << "PASS: TestObtraceRecorderObservesThroughBuffer\n";
}

static void TestConnectionRegistryAllEmpty() {
    ConnectionRegistry registry;
    assert(registry.All().empty());
    assert(registry.Size() == 0);
    std::cout << "PASS: TestConnectionRegistryAllEmpty\n";
}

static void TestNewNetworkEventTypes() {
    NetworkEvent ev;
    ev.type = NetworkEventType::DnsResolved;
    assert(ev.type == NetworkEventType::DnsResolved);
    ev.type = NetworkEventType::ConnectionEstablished;
    assert(ev.type == NetworkEventType::ConnectionEstablished);
    ev.type = NetworkEventType::TlsHandshaked;
    assert(ev.type == NetworkEventType::TlsHandshaked);
    std::cout << "PASS: TestNewNetworkEventTypes\n";
}

static void TestNetworkRequestSummaryFilterFields() {
    NetworkRequestSummary req;
    req.request_id = "r1";
    req.filter_blocked = false;
    req.filter_layer = "";
    req.filter_rule_source = "";
    req.connection_id = std::nullopt;

    assert(!req.filter_blocked);
    assert(!req.connection_id.has_value());

    req.filter_blocked = true;
    req.connection_id = "conn-99";
    assert(req.filter_blocked);
    assert(req.connection_id.has_value());
    assert(*req.connection_id == "conn-99");
    std::cout << "PASS: TestNetworkRequestSummaryFilterFields\n";
}

static void TestObservedCallbackDurationIsAggregated() {
    NetworkTraceBuffer buffer;

    NetworkEvent start;
    start.request_id = "req-time";
    start.type = NetworkEventType::RequestStarted;
    start.url = "https://timing.example/";
    start.method = "GET";
    start.observed_at_ms = 1000;
    buffer.Add(start);

    NetworkEvent finish;
    finish.request_id = "req-time";
    finish.type = NetworkEventType::RequestFinished;
    finish.url = "https://timing.example/";
    finish.method = "GET";
    finish.observed_at_ms = 1125;
    buffer.Add(finish);

    const auto requests = buffer.AggregateRequests();
    assert(requests.size() == 1);
    assert(requests.front().started_at_ms == 1000);
    assert(requests.front().ended_at_ms == 1125);
    assert(requests.front().observed_duration_ms == 125);
    std::cout << "PASS: TestObservedCallbackDurationIsAggregated\n";
}

static void TestSafeExportsRedactSensitiveData() {
    NetworkRequestSummary req;
    req.request_id = "req-safe";
    req.method = "POST";
    req.url = "https://user:pass@example.test/login?token=super-secret&safe=visible";
    req.status = 401;
    req.protocol = {};
    req.request_headers = {
        Header{.name = "Authorization", .value = "Bearer top-secret"},
        Header{.name = "Content-Type", .value = "application/json"},
    };
    req.response_headers = {
        Header{.name = "Set-Cookie", .value = "session=secret-cookie"},
    };
    req.body_preview = "password=do-not-export";
    req.started_at_ms = 1000;
    req.ended_at_ms = 1100;
    req.observed_duration_ms = 100;
    req.state = RequestState::Finished;

    const auto har = NetworkTraceBuffer::ExportToHar({req});
    const auto obtrace = NetworkTraceBuffer::ExportToObtrace({req});

    assert(har.find("super-secret") == std::string::npos);
    assert(har.find("top-secret") == std::string::npos);
    assert(har.find("secret-cookie") == std::string::npos);
    assert(har.find("do-not-export") == std::string::npos);
    assert(har.find("<redacted>") != std::string::npos);
    assert(har.find("\"send\": -1") != std::string::npos);
    assert(har.find("\"wait\": -1") != std::string::npos);
    assert(har.find("\"receive\": -1") != std::string::npos);
    assert(har.find("observed callback span") != std::string::npos);
    assert(har.find("\"httpVersion\": \"unknown\"") != std::string::npos);

    assert(obtrace.find("super-secret") == std::string::npos);
    assert(obtrace.find("do-not-export") == std::string::npos);
    assert(obtrace.find("<redacted>") != std::string::npos);
    assert(obtrace.find("\"protocol\":\"unknown\"") != std::string::npos);
    assert(obtrace.find("\"observed_duration_ms\":100") != std::string::npos);
    std::cout << "PASS: TestSafeExportsRedactSensitiveData\n";
}

static void TestStructuredQueryCombinesFields() {
    NetworkRequestSummary matching;
    matching.request_id = "req-match";
    matching.method = "POST";
    matching.url = "https://api.github.com/repos/example";
    matching.status = 503;
    matching.state = RequestState::Failed;
    matching.filter_blocked = true;

    NetworkRequestSummary other;
    other.request_id = "req-other";
    other.method = "GET";
    other.url = "https://example.test/";
    other.status = 200;
    other.state = RequestState::Finished;
    other.filter_blocked = false;

    const NetworkFilterQuery query("method:POST status:>=400 host:github.com blocked:true is:failed");
    const auto filtered = query.Filter({matching, other});
    assert(filtered.size() == 1);
    assert(filtered.front().request_id == "req-match");
    std::cout << "PASS: TestStructuredQueryCombinesFields\n";
}

int main() {
    TestConnectionRegistryCrossReferencedInSummary();
    TestFilterDecisionLogIntegration();
    TestFilterDecisionInObtrace();
    TestObtraceRecorderObservesThroughBuffer();
    TestConnectionRegistryAllEmpty();
    TestNewNetworkEventTypes();
    TestNetworkRequestSummaryFilterFields();
    TestObservedCallbackDurationIsAggregated();
    TestSafeExportsRedactSensitiveData();
    TestStructuredQueryCombinesFields();
    std::cout << "All network_lab_enhanced_tests PASSED.\n";
    return 0;
}
