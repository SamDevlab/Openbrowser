// M7.2 Filter Decision Log Tests
// Tests for FilterDecisionRecord, FilterDecisionLog, blocked: clause in NetworkFilterQuery.

#include "core/network/filter_decision_log.h"
#include "core/network/network_filter_query.h"
#include "core/filters/content_filter.h"
#include "devtools/network/network_trace.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace openbrowser::core;
using namespace openbrowser::devtools::network;

static void TestAddAndAll() {
    FilterDecisionLog log;
    assert(log.Size() == 0);

    FilterDecisionRecord rec;
    rec.request_id       = "req-1";
    rec.url              = "https://tracker.example/collect";
    rec.blocked          = true;
    rec.layer            = FilterDecisionLayer::ContentFilter;
    rec.rule_source      = "tracker.example";
    rec.scope            = "global";
    rec.human_explanation = "Blocked by tracker rule: tracker.example";
    log.AddDecision(rec);

    assert(log.Size() == 1);
    const auto all = log.All();
    assert(all.size() == 1);
    assert(all[0].request_id == "req-1");
    assert(all[0].blocked);
    std::cout << "PASS: TestAddAndAll\n";
}

static void TestFindByRequestId() {
    FilterDecisionLog log;
    for (int i = 0; i < 5; ++i) {
        FilterDecisionRecord rec;
        rec.request_id = "req-" + std::to_string(i);
        rec.blocked = (i % 2 == 0);
        log.AddDecision(rec);
    }
    const auto found = log.FindByRequestId("req-3");
    assert(found.has_value());
    assert(!found->blocked);

    const auto missing = log.FindByRequestId("req-999");
    assert(!missing.has_value());
    std::cout << "PASS: TestFindByRequestId\n";
}

static void TestCapacityEviction() {
    FilterDecisionLog log(5);
    for (int i = 0; i < 10; ++i) {
        FilterDecisionRecord rec;
        rec.request_id = "req-" + std::to_string(i);
        log.AddDecision(rec);
    }
    assert(log.Size() == 5);
    assert(log.DroppedCount() == 5);
    // The 5 most recent should be retained (req-5..req-9).
    assert(log.FindByRequestId("req-0") == std::nullopt);
    assert(log.FindByRequestId("req-9").has_value());
    std::cout << "PASS: TestCapacityEviction\n";
}

static void TestClear() {
    FilterDecisionLog log;
    FilterDecisionRecord rec;
    rec.request_id = "x";
    log.AddDecision(rec);
    assert(log.Size() == 1);
    log.Clear();
    assert(log.Size() == 0);
    assert(log.DroppedCount() == 0);
    std::cout << "PASS: TestClear\n";
}

static void TestSinkCallback() {
    FilterDecisionLog log;
    int callback_count = 0;

    class TestSink : public FilterDecisionSink {
    public:
        explicit TestSink(int& count) : count_(count) {}
        void OnFilterDecision(const FilterDecisionRecord& /*dec*/) override {
            ++count_;
        }
    private:
        int& count_;
    } sink(callback_count);

    log.SetSink(&sink);

    FilterDecisionRecord rec;
    rec.request_id = "r1";
    log.AddDecision(rec);
    rec.request_id = "r2";
    log.AddDecision(rec);

    assert(callback_count == 2);

    // Detach sink — no more callbacks.
    log.SetSink(nullptr);
    rec.request_id = "r3";
    log.AddDecision(rec);
    assert(callback_count == 2);
    std::cout << "PASS: TestSinkCallback\n";
}

static void TestFilterDecisionLayerToString() {
    assert(std::string(FilterDecisionLayerToString(FilterDecisionLayer::ContentFilter)) == "ContentFilter");
    assert(std::string(FilterDecisionLayerToString(FilterDecisionLayer::CapabilityPolicy)) == "CapabilityPolicy");
    assert(std::string(FilterDecisionLayerToString(FilterDecisionLayer::WorkspacePolicy)) == "WorkspacePolicy");
    assert(std::string(FilterDecisionLayerToString(FilterDecisionLayer::UserAgentPolicy)) == "UserAgentPolicy");
    std::cout << "PASS: TestFilterDecisionLayerToString\n";
}

static void TestContentFilterEmitsDecisions() {
    FilterDecisionLog log;
    ContentFilter filter;
    filter.SetDecisionLog(&log);

    // Default rules block doubleclick.net.
    const auto result = filter.EvaluateWithId(
        "https://doubleclick.net/pixel", "req-blocked", "global");
    assert(result == FilterDecision::Block);
    assert(log.Size() == 1);
    const auto found = log.FindByRequestId("req-blocked");
    assert(found.has_value());
    assert(found->blocked);
    assert(!found->rule_source.empty());

    // Allowed request.
    const auto ok = filter.EvaluateWithId(
        "https://example.com/page", "req-allowed", "global");
    assert(ok == FilterDecision::Allow);
    assert(log.Size() == 2);
    const auto allowed_found = log.FindByRequestId("req-allowed");
    assert(allowed_found.has_value());
    assert(!allowed_found->blocked);
    std::cout << "PASS: TestContentFilterEmitsDecisions\n";
}

static void TestBlockedFilterClause() {
    // Build two summaries — one blocked, one allowed.
    NetworkRequestSummary blocked_req;
    blocked_req.request_id   = "r-blocked";
    blocked_req.url           = "https://tracker.example/pixel";
    blocked_req.filter_blocked = true;
    blocked_req.filter_layer  = "ContentFilter";

    NetworkRequestSummary allowed_req;
    allowed_req.request_id   = "r-allowed";
    allowed_req.url           = "https://good.example.com/page";
    allowed_req.filter_blocked = false;

    const std::vector<NetworkRequestSummary> requests = {blocked_req, allowed_req};

    // blocked:true should match only the blocked one.
    NetworkFilterQuery q_blocked("blocked:true");
    const auto filtered_blocked = q_blocked.Filter(requests);
    assert(filtered_blocked.size() == 1);
    assert(filtered_blocked[0].request_id == "r-blocked");

    // blocked:false should match only the allowed one.
    NetworkFilterQuery q_allowed("blocked:false");
    const auto filtered_allowed = q_allowed.Filter(requests);
    assert(filtered_allowed.size() == 1);
    assert(filtered_allowed[0].request_id == "r-allowed");

    // decision:block
    NetworkFilterQuery q_dec_block("decision:block");
    const auto dec_blocked = q_dec_block.Filter(requests);
    assert(dec_blocked.size() == 1);
    assert(dec_blocked[0].request_id == "r-blocked");

    // decision:allow
    NetworkFilterQuery q_dec_allow("decision:allow");
    const auto dec_allowed = q_dec_allow.Filter(requests);
    assert(dec_allowed.size() == 1);
    assert(dec_allowed[0].request_id == "r-allowed");

    std::cout << "PASS: TestBlockedFilterClause\n";
}

int main() {
    TestAddAndAll();
    TestFindByRequestId();
    TestCapacityEviction();
    TestClear();
    TestSinkCallback();
    TestFilterDecisionLayerToString();
    TestContentFilterEmitsDecisions();
    TestBlockedFilterClause();
    std::cout << "All filter_decision_log_tests PASSED.\n";
    return 0;
}
