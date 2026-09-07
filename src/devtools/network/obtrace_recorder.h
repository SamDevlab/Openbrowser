#pragma once

// M7.3: Local .obtrace Trace Recorder
//
// Subscribes to a NetworkTraceBuffer and streams events to a versioned
// NDJSON .obtrace file on disk.  Each line is a self-contained JSON object
// with a "schema":"obtrace/1" field so files remain readable even if
// partially written.

#include "devtools/network/network_trace.h"

#include <cstddef>
#include <fstream>
#include <string>

namespace openbrowser::devtools::network {

class ObtraceRecorder final : public NetworkTraceObserver {
public:
    ObtraceRecorder() = default;
    ~ObtraceRecorder() override;

    ObtraceRecorder(const ObtraceRecorder&) = delete;
    ObtraceRecorder& operator=(const ObtraceRecorder&) = delete;

    // Open the file at 'path' and begin recording.  Returns false if the
    // file could not be created or the recorder is already running.
    [[nodiscard]] bool StartRecording(const std::string& path);

    // Flush, write the trace_end sentinel, and close the file.
    void StopRecording();

    [[nodiscard]] bool IsRecording() const noexcept;
    [[nodiscard]] std::size_t GetRecordedEventCount() const noexcept;
    [[nodiscard]] const std::string& GetFilePath() const noexcept;

    // NetworkTraceObserver
    void OnTraceEventAppended(const NetworkEvent& event) override;
    void OnTraceCleared() override;

private:
    void WriteEvent(const NetworkEvent& event);

    std::ofstream file_;
    std::string path_;
    bool recording_{false};
    std::size_t event_count_{0};
    bool redact_headers_{true};
};

} // namespace openbrowser::devtools::network
