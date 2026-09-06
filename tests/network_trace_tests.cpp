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
        Header{.name = "Content-Type", .value = "application/json"},
    };

    buffer.Add(std::move(event));

    const auto& headers = buffer.Events().front().headers;
    Require(headers[0].value == "<redacted>", "authorization is redacted case-insensitively");
    Require(headers[1].value == "<redacted>", "cookie is redacted");
    Require(headers[2].value == "application/json", "non-sensitive header is preserved");
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

}  // namespace

int main() {
    TestSensitiveHeadersAreRedactedByDefault();
    TestExplicitRawHeaderPolicyIsPossible();
    TestBufferIsBoundedAndReportsDroppedEvents();
    TestZeroCapacityIsNormalizedToOne();
    TestClearResetsCaptureSessionCounters();

    if (failures != 0) {
        std::cerr << failures << " Network Lab test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser Network Lab trace invariants: PASS\n";
    return 0;
}
