#include "core/network/waterfall_timeline.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace openbrowser::core {

const char* WaterfallPhaseToString(WaterfallPhase phase) noexcept {
    switch (phase) {
    case WaterfallPhase::Queue:
        return "Queue";
    case WaterfallPhase::Dns:
        return "DNS";
    case WaterfallPhase::Connect:
        return "Connect";
    case WaterfallPhase::Tls:
        return "TLS";
    case WaterfallPhase::Request:
        return "Request";
    case WaterfallPhase::Ttfb:
        return "TTFB";
    case WaterfallPhase::Download:
        return "Download";
    }
    return "Unknown";
}

WaterfallTimeline::WaterfallTimeline(std::string request_id, std::string url)
    : request_id_(std::move(request_id)), url_(std::move(url)) {}

const std::string& WaterfallTimeline::GetRequestId() const noexcept {
    return request_id_;
}

const std::string& WaterfallTimeline::GetUrl() const noexcept {
    return url_;
}

void WaterfallTimeline::AddSegment(
    WaterfallPhase phase,
    double start_ms,
    double duration_ms,
    bool available) {

    WaterfallSegment seg;
    seg.phase = phase;
    seg.start_ms = std::max(0.0, start_ms);
    seg.duration_ms = std::max(0.0, duration_ms);
    seg.available = available;
    segments_.push_back(seg);
}

const std::vector<WaterfallSegment>& WaterfallTimeline::GetSegments() const noexcept {
    return segments_;
}

double WaterfallTimeline::GetTotalDurationMs() const {
    double total = 0.0;
    for (const auto& s : segments_) {
        if (s.available) {
            total = std::max(total, s.start_ms + s.duration_ms);
        }
    }
    return total;
}

double WaterfallTimeline::GetPhaseDurationMs(WaterfallPhase phase) const {
    for (const auto& s : segments_) {
        if (s.phase == phase && s.available) {
            return s.duration_ms;
        }
    }
    return 0.0;
}

bool WaterfallTimeline::HasPhase(WaterfallPhase phase) const {
    for (const auto& s : segments_) {
        if (s.phase == phase) {
            return true;
        }
    }
    return false;
}

std::string WaterfallTimeline::ToAsciiChart(std::size_t chart_width) const {
    std::ostringstream oss;
    const double total_ms = GetTotalDurationMs();
    if (total_ms <= 0.0 || chart_width == 0) {
        oss << "[Empty Timeline]\n";
        return oss.str();
    }

    oss << "Waterfall for " << (url_.empty() ? request_id_ : url_)
        << " (Total: " << std::fixed << std::setprecision(1) << total_ms << " ms)\n";

    for (const auto& s : segments_) {
        oss << std::left << std::setw(10) << WaterfallPhaseToString(s.phase) << ": ";
        if (!s.available) {
            oss << "[unavailable]\n";
            continue;
        }

        const auto offset_chars = static_cast<std::size_t>((s.start_ms / total_ms) * static_cast<double>(chart_width));
        auto width_chars = static_cast<std::size_t>((s.duration_ms / total_ms) * static_cast<double>(chart_width));
        if (width_chars == 0) {
            width_chars = 1;
        }

        oss << std::string(offset_chars, ' ')
            << std::string(width_chars, '#')
            << " " << std::fixed << std::setprecision(1) << s.duration_ms << " ms\n";
    }

    return oss.str();
}

} // namespace openbrowser::core
