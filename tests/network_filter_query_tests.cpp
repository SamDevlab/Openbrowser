#include "core/network/network_filter_query.h"

#include <iostream>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

void TestStatusRangeAndComparisonQueries() {
    using namespace openbrowser::core;
    using namespace openbrowser::devtools::network;

    NetworkRequestSummary r200;
    r200.status = 200;
    r200.method = "GET";

    NetworkRequestSummary r404;
    r404.status = 404;
    r404.method = "GET";

    NetworkRequestSummary r500;
    r500.status = 500;
    r500.method = "POST";

    NetworkFilterQuery q_gte("status:>=400");
    Require(!q_gte.Matches(r200), "200 does not match status:>=400");
    Require(q_gte.Matches(r404), "404 matches status:>=400");
    Require(q_gte.Matches(r500), "500 matches status:>=400");

    NetworkFilterQuery q_range("status:200..299");
    Require(q_range.Matches(r200), "200 matches status:200..299");
    Require(!q_range.Matches(r404), "404 does not match status:200..299");

    NetworkFilterQuery q_neg("-status:200");
    Require(!q_neg.Matches(r200), "200 rejected by -status:200");
    Require(q_neg.Matches(r404), "404 matches -status:200");
}

void TestMethodAndHostQueries() {
    using namespace openbrowser::core;
    using namespace openbrowser::devtools::network;

    NetworkRequestSummary req;
    req.method = "POST";
    req.url = "https://api.example.com/v1/auth";
    req.status = 201;

    NetworkFilterQuery q1("method:POST host:api.example.com");
    Require(q1.Matches(req), "Matches method:POST host:api.example.com");

    NetworkFilterQuery q2("method:GET");
    Require(!q2.Matches(req), "Does not match method:GET");

    NetworkFilterQuery q3("auth");
    Require(q3.Matches(req), "Free text search matches url path");
}

void TestStateAndAttributionQueries() {
    using namespace openbrowser::core;
    using namespace openbrowser::devtools::network;

    NetworkRequestSummary req_failed;
    req_failed.state = RequestState::Failed;
    req_failed.error = "ERR_BLOCKED_BY_CLIENT";
    req_failed.attribution = NetworkAttribution::Page;

    NetworkFilterQuery q_blocked("is:blocked");
    Require(q_blocked.Matches(req_failed), "Matches is:blocked when error contains block");

    NetworkFilterQuery q_failed("is:failed");
    Require(q_failed.Matches(req_failed), "Matches is:failed");

    NetworkFilterQuery q_transfer("attribution:transfer");
    Require(!q_transfer.Matches(req_failed), "Page request does not match attribution:transfer");

    NetworkRequestSummary req_transfer;
    req_transfer.attribution = NetworkAttribution::Transfer;
    Require(q_transfer.Matches(req_transfer), "Matches attribution:transfer");
}

} // namespace

int main() {
    TestStatusRangeAndComparisonQueries();
    TestMethodAndHostQueries();
    TestStateAndAttributionQueries();

    std::cout << "All NetworkFilterQuery tests passed successfully." << std::endl;
    return 0;
}
