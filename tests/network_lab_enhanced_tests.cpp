// M7.4 Network Lab Enhanced Tests
// Tests for the desktop-layer integration of M7 subsystems:
// ConnectionRegistry wiring, FilterDecisionLog wiring, and
// ExportToObtrace availability via NetworkTraceBuffer.

#include "devtools/network/connection_diagnostics.h"
#include "devtools/network/network_trace.h"
#include "devtools/network/obtrace_recorder.h"
#include "core/network/filter_decision_log.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace openbrowser;
using namespace openbrowser::devtools::network;
using namespace openbrowser::core;

static void TestConnectionRegistryCrossReferencedInSummary() {
    // Simulate setting connection_id on a NetworkRequestSummary to verify
    // that the field is propagated through ExportToObtrace.
    NetworkRequestSummary req;
    req.request_id    = "req-cross";
    req.url           = "https://example.com/";
    req.connection_id = "conn-17";
    req.state         = RequestState::Finished;

    const auto result = NetworkTraceBuffer::ExportToObtrace({req});
    assert(result.find("conn-17") != std::string::npos);
    assert(result.find("req-cross") != std::string::npos);
    std::cout << "PASS: TestConnectionRegistryCrossReferencedInSummary\n";
}

static void TestFilterDecisionLogIntegration() {
    FilterDecisionLog log(100);

    FilterDecisionRecord rec;
    rec.request_id       = "req-decision";
    rec.url              = "https://blocked.tracker.example/pixel";
    rec.blocked          = true;
    rec.layer            = FilterDecisionLayer::ContentFilter;
    rec.rule_source      = "blocked.tracker.example";
    rec.scope            = "global";
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
    req.request_id       = "req-dec";
    req.url              = "https://tracker.example/pixel";
    req.filter_blocked   = true;
    req.filter_layer     = "ContentFilter";
    req.filter_rule_source = "tracker.example";
    req.state            = RequestState::Failed;

    const auto result = NetworkTraceBuffer::ExportToObtrace({req});
    assert(result.find("filter_blocked\":true") != std::string::npos);
    assert(result.find("ContentFilter") != std::string::npos);
    std::cout << "PASS: TestFilterDecisionInObtrace\n";
}

static void TestObtraceRecorderObservesThroughBuffer() {
    NetworkTraceBuffer buf;
    ObtraceRecorder recorder;
    buf.AddObserver(&recorder);

    // Don't start recording — events should be silently dropped without crash.
    NetworkEvent ev;
    ev.request_id = "r-silent";
    ev.url        = "https://no.recording/yet";
    ev.type       = NetworkEventType::RequestStarted;
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
    // Verify that the new M7.1 event types compile and can be compared.
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
    req.request_id       = "r1";
    req.filter_blocked   = false;
    req.filter_layer     = "";
    req.filter_rule_source = "";
    req.connection_id    = std::nullopt;

    assert(!req.filter_blocked);
    assert(!req.connection_id.has_value());

    req.filter_blocked = true;
    req.connection_id  = "conn-99";
    assert(req.filter_blocked);
    assert(req.connection_id.has_value());
    assert(*req.connection_id == "conn-99");
    std::cout << "PASS: TestNetworkRequestSummaryFilterFields\n";
}

int main() {
    TestConnectionRegistryCrossReferencedInSummary();
    TestFilterDecisionLogIntegration();
    TestFilterDecisionInObtrace();
    TestObtraceRecorderObservesThroughBuffer();
    TestConnectionRegistryAllEmpty();
    TestNewNetworkEventTypes();
    TestNetworkRequestSummaryFilterFields();
    std::cout << "All network_lab_enhanced_tests PASSED.\n";
    return 0;
}
