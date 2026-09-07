#include "core/network/waterfall_timeline.h"

#include <iostream>
#include <string>

namespace {
void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

void TestWaterfallPhaseDurationAndTotal() {
    using namespace openbrowser::core;

    WaterfallTimeline timeline("req-42", "https://example.com/data.json");
    Require(timeline.GetRequestId() == "req-42", "Request ID matches");
    Require(timeline.GetUrl() == "https://example.com/data.json", "URL matches");

    timeline.AddSegment(WaterfallPhase::Queue, 0.0, 5.0);
    timeline.AddSegment(WaterfallPhase::Dns, 5.0, 15.0);
    timeline.AddSegment(WaterfallPhase::Connect, 20.0, 25.0);
    timeline.AddSegment(WaterfallPhase::Tls, 45.0, 30.0);
    timeline.AddSegment(WaterfallPhase::Request, 75.0, 2.0);
    timeline.AddSegment(WaterfallPhase::Ttfb, 77.0, 40.0);
    timeline.AddSegment(WaterfallPhase::Download, 117.0, 20.0);

    Require(timeline.HasPhase(WaterfallPhase::Dns), "Has DNS phase");
    Require(timeline.GetPhaseDurationMs(WaterfallPhase::Dns) == 15.0, "DNS duration is 15 ms");
    Require(timeline.GetPhaseDurationMs(WaterfallPhase::Ttfb) == 40.0, "TTFB duration is 40 ms");

    // Total duration should be max(start + duration) = 117 + 20 = 137.0 ms
    Require(timeline.GetTotalDurationMs() == 137.0, "Total duration is 137 ms");
}

void TestUnavailablePhases() {
    using namespace openbrowser::core;

    WaterfallTimeline timeline("req-cache", "https://example.com/cached.png");
    timeline.AddSegment(WaterfallPhase::Queue, 0.0, 2.0);
    timeline.AddSegment(WaterfallPhase::Dns, 2.0, 0.0, false); // DNS not performed for cache hit
    timeline.AddSegment(WaterfallPhase::Download, 2.0, 5.0);

    Require(timeline.HasPhase(WaterfallPhase::Dns), "DNS phase registered");
    Require(timeline.GetPhaseDurationMs(WaterfallPhase::Dns) == 0.0, "Unavailable phase duration is 0");
    Require(timeline.GetTotalDurationMs() == 7.0, "Total duration ignores unavailable phase");

    std::string chart = timeline.ToAsciiChart(20);
    Require(chart.find("[unavailable]") != std::string::npos, "Chart indicates unavailable phase");
}

} // namespace

int main() {
    TestWaterfallPhaseDurationAndTotal();
    TestUnavailablePhases();

    std::cout << "All WaterfallTimeline tests passed successfully." << std::endl;
    return 0;
}
