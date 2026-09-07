#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace openbrowser::core {

enum class WaterfallPhase {
    Queue,
    Dns,
    Connect,
    Tls,
    Request,
    Ttfb,
    Download
};

const char* WaterfallPhaseToString(WaterfallPhase phase) noexcept;

struct WaterfallSegment {
    WaterfallPhase phase = WaterfallPhase::Queue;
    double start_ms = 0.0;
    double duration_ms = 0.0;
    bool available = true;
};

class WaterfallTimeline {
public:
    explicit WaterfallTimeline(std::string request_id = "", std::string url = "");
    ~WaterfallTimeline() = default;

    [[nodiscard]] const std::string& GetRequestId() const noexcept;
    [[nodiscard]] const std::string& GetUrl() const noexcept;

    void AddSegment(WaterfallPhase phase, double start_ms, double duration_ms, bool available = true);
    [[nodiscard]] const std::vector<WaterfallSegment>& GetSegments() const noexcept;

    [[nodiscard]] double GetTotalDurationMs() const;
    [[nodiscard]] double GetPhaseDurationMs(WaterfallPhase phase) const;
    [[nodiscard]] bool HasPhase(WaterfallPhase phase) const;

    [[nodiscard]] std::string ToAsciiChart(std::size_t chart_width = 40) const;

private:
    std::string request_id_;
    std::string url_;
    std::vector<WaterfallSegment> segments_;
};

} // namespace openbrowser::core
