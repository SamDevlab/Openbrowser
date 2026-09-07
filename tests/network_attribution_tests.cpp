#include "core/network/network_attribution.h"
#include "devtools/network/network_trace.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestAttributionEnumToStringAndBack() {
    using openbrowser::core::AttributionToString;
    using openbrowser::core::NetworkAttribution;
    using openbrowser::core::StringToAttribution;

    Require(AttributionToString(NetworkAttribution::Page) == "Page", "Page string conversion");
    Require(AttributionToString(NetworkAttribution::UpdateCheck) == "UpdateCheck", "UpdateCheck string conversion");
    Require(AttributionToString(NetworkAttribution::FilterListSync) == "FilterListSync", "FilterListSync string conversion");
    Require(AttributionToString(NetworkAttribution::Telemetry) == "Telemetry", "Telemetry string conversion");
    Require(AttributionToString(NetworkAttribution::Transfer) == "Transfer", "Transfer string conversion");

    Require(StringToAttribution("Page") == NetworkAttribution::Page, "Page string parse");
    Require(StringToAttribution("UpdateCheck") == NetworkAttribution::UpdateCheck, "UpdateCheck string parse");
    Require(StringToAttribution("FilterListSync") == NetworkAttribution::FilterListSync, "FilterListSync string parse");
    Require(StringToAttribution("Telemetry") == NetworkAttribution::Telemetry, "Telemetry string parse");
    Require(StringToAttribution("Transfer") == NetworkAttribution::Transfer, "Transfer string parse");
    Require(StringToAttribution("UnknownValue") == NetworkAttribution::Page, "Fallback to Page");
}

void TestNetworkEventAttributionAggregation() {
    using openbrowser::core::NetworkAttribution;
    using openbrowser::devtools::network::NetworkEvent;
    using openbrowser::devtools::network::NetworkEventType;
    using openbrowser::devtools::network::NetworkTraceBuffer;

    NetworkTraceBuffer buffer;

    NetworkEvent ev1;
    ev1.sequence = 0;
    ev1.request_id = "req-1";
    ev1.tab_id = std::nullopt;
    ev1.type = NetworkEventType::RequestStarted;
    ev1.url = "https://example.test/app-update";
    ev1.method = "GET";
    ev1.status = std::nullopt;
    ev1.protocol = "HTTP/1.1";
    ev1.headers = {};
    ev1.transferred_bytes = 0;
    ev1.error = {};
    ev1.body_preview = {};
    ev1.attribution = NetworkAttribution::UpdateCheck;

    buffer.Add(ev1);

    NetworkEvent ev2;
    ev2.sequence = 0;
    ev2.request_id = "req-2";
    ev2.tab_id = std::nullopt;
    ev2.type = NetworkEventType::RequestStarted;
    ev2.url = "https://example.test/download.zip";
    ev2.method = "GET";
    ev2.status = std::nullopt;
    ev2.protocol = "HTTP/1.1";
    ev2.headers = {};
    ev2.transferred_bytes = 0;
    ev2.error = {};
    ev2.body_preview = {};
    ev2.attribution = NetworkAttribution::Transfer;

    buffer.Add(ev2);

    const auto summaries = buffer.AggregateRequests();
    Require(summaries.size() == 2, "2 aggregated requests");
    Require(summaries[0].attribution == NetworkAttribution::UpdateCheck, "req-1 has UpdateCheck attribution");
    Require(summaries[1].attribution == NetworkAttribution::Transfer, "req-2 has Transfer attribution");
}

void TestHarExportFormat() {
    using openbrowser::core::NetworkAttribution;
    using openbrowser::devtools::network::Header;
    using openbrowser::devtools::network::NetworkEvent;
    using openbrowser::devtools::network::NetworkEventType;
    using openbrowser::devtools::network::NetworkTraceBuffer;

    NetworkTraceBuffer buffer;

    NetworkEvent start;
    start.sequence = 0;
    start.request_id = "req-1";
    start.tab_id = std::nullopt;
    start.type = NetworkEventType::RequestStarted;
    start.url = "https://example.test/api/data";
    start.method = "POST";
    start.status = std::nullopt;
    start.protocol = "HTTP/1.1";
    start.headers = {Header{.name = "Content-Type", .value = "application/json"}};
    start.transferred_bytes = 0;
    start.error = {};
    start.body_preview = "{\"test\":true}";
    start.attribution = NetworkAttribution::Telemetry;
    buffer.Add(start);

    NetworkEvent finish;
    finish.sequence = 0;
    finish.request_id = "req-1";
    finish.tab_id = std::nullopt;
    finish.type = NetworkEventType::ResponseReceived;
    finish.url = "https://example.test/api/data";
    finish.method = "POST";
    finish.status = 200;
    finish.protocol = "HTTP/1.1";
    finish.headers = {Header{.name = "Server", .value = "nginx"}};
    finish.transferred_bytes = 1024;
    finish.error = {};
    finish.body_preview = "{\"ok\":true}";
    finish.attribution = NetworkAttribution::Telemetry;
    buffer.Add(finish);

    const auto summaries = buffer.AggregateRequests();
    const auto har = NetworkTraceBuffer::ExportToHar(summaries);

    Require(har.find("\"version\": \"1.2\"") != std::string::npos, "HAR has version 1.2");
    Require(har.find("\"creator\"") != std::string::npos, "HAR has creator");
    Require(har.find("\"Openbrowser Network Lab\"") != std::string::npos, "HAR has creator name");
    Require(har.find("\"_attribution\": \"Telemetry\"") != std::string::npos, "HAR has _attribution field");
    Require(har.find("\"method\": \"POST\"") != std::string::npos, "HAR has method");
    Require(har.find("\"url\": \"https://example.test/api/data\"") != std::string::npos, "HAR has url");
    Require(har.find("\"status\": 200") != std::string::npos, "HAR has response status 200");
    Require(har.find("\"size\": 1024") != std::string::npos, "HAR has transferred size 1024");
}

}  // namespace

int main() {
    TestAttributionEnumToStringAndBack();
    TestNetworkEventAttributionAggregation();
    TestHarExportFormat();

    if (failures != 0) {
        std::cerr << failures << " Network Attribution test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser Network Attribution & HAR Export invariants: PASS\n";
    return 0;
}
