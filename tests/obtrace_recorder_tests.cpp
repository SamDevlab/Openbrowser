// M7.3 ObtraceRecorder Tests
// Tests for ObtraceRecorder NDJSON file output and ExportToObtrace static method.

#include "devtools/network/obtrace_recorder.h"
#include "devtools/network/network_trace.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace openbrowser::devtools::network;

// Minimal helper: count lines that start with '{' in a string.
static std::size_t CountJsonLines(const std::string& content) {
    std::size_t count = 0;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line[0] == '{') {
            ++count;
        }
    }
    return count;
}

static std::string ReadFile(const std::string& path) {
    std::ifstream f(path);
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

static void TestStartStopRecording() {
    const std::string path = "test_obtrace_start_stop.obtrace";
    ObtraceRecorder recorder;
    assert(!recorder.IsRecording());
    assert(recorder.StartRecording(path));
    assert(recorder.IsRecording());
    assert(recorder.GetFilePath() == path);
    recorder.StopRecording();
    assert(!recorder.IsRecording());
    assert(std::filesystem::exists(path));
    std::filesystem::remove(path);
    std::cout << "PASS: TestStartStopRecording\n";
}

static void TestDoubleStartReturnsFalse() {
    const std::string path = "test_obtrace_double.obtrace";
    ObtraceRecorder recorder;
    assert(recorder.StartRecording(path));
    assert(!recorder.StartRecording("other.obtrace")); // should fail — already recording
    recorder.StopRecording();
    std::filesystem::remove(path);
    std::cout << "PASS: TestDoubleStartReturnsFalse\n";
}

static void TestEventWrittenToFile() {
    const std::string path = "test_obtrace_events.obtrace";
    NetworkTraceBuffer buf;
    ObtraceRecorder recorder;
    buf.AddObserver(&recorder);
    assert(recorder.StartRecording(path));

    NetworkEvent ev;
    ev.request_id = "req-abc";
    ev.url        = "https://example.com/api";
    ev.method     = "GET";
    ev.type       = NetworkEventType::RequestStarted;
    buf.Add(ev);

    ev.type   = NetworkEventType::ResponseReceived;
    ev.status = 200;
    buf.Add(ev);

    assert(recorder.GetRecordedEventCount() == 2);
    recorder.StopRecording();
    buf.RemoveObserver(&recorder);

    const auto content = ReadFile(path);
    // Expect: trace_start + 2 events + trace_end = 4 JSON lines
    assert(CountJsonLines(content) == 4);
    // Content should include the request_id.
    assert(content.find("req-abc") != std::string::npos);
    assert(content.find("trace_end") != std::string::npos);
    std::filesystem::remove(path);
    std::cout << "PASS: TestEventWrittenToFile\n";
}

static void TestSentinelContainsTotalEvents() {
    const std::string path = "test_obtrace_sentinel.obtrace";
    ObtraceRecorder recorder;
    assert(recorder.StartRecording(path));
    recorder.StopRecording();

    const auto content = ReadFile(path);
    assert(content.find("total_events") != std::string::npos);
    assert(content.find("trace_end") != std::string::npos);
    std::filesystem::remove(path);
    std::cout << "PASS: TestSentinelContainsTotalEvents\n";
}

static void TestExportToObtraceStatic() {
    NetworkRequestSummary req;
    req.request_id         = "r1";
    req.url                = "https://test.example.com/data";
    req.method             = "POST";
    req.status             = 201;
    req.protocol           = "h2";
    req.transferred_bytes  = 512;
    req.state              = RequestState::Finished;
    req.filter_blocked     = false;

    NetworkRequestSummary req2;
    req2.request_id      = "r2";
    req2.url             = "https://tracker.example/collect";
    req2.method          = "GET";
    req2.filter_blocked  = true;
    req2.filter_layer    = "ContentFilter";
    req2.filter_rule_source = "tracker.example";
    req2.state           = RequestState::Failed;

    const auto result = NetworkTraceBuffer::ExportToObtrace({req, req2});

    // Schema/version check
    assert(result.find("\"schema\":\"obtrace/1\"") != std::string::npos);
    assert(result.find("trace_start") != std::string::npos);
    assert(result.find("trace_end") != std::string::npos);
    assert(result.find("r1") != std::string::npos);
    assert(result.find("r2") != std::string::npos);
    assert(result.find("filter_blocked\":true") != std::string::npos);
    assert(result.find("filter_blocked\":false") != std::string::npos);
    assert(CountJsonLines(result) == 4); // header + 2 entries + sentinel
    std::cout << "PASS: TestExportToObtraceStatic\n";
}

static void TestObtraceRedactsSensitiveHeaders() {
    const std::string path = "test_obtrace_redact.obtrace";
    NetworkTraceBuffer buf;
    ObtraceRecorder recorder;
    buf.AddObserver(&recorder);
    assert(recorder.StartRecording(path));

    NetworkEvent ev;
    ev.request_id = "req-sensitive";
    ev.url        = "https://api.example.com/auth";
    ev.method     = "POST";
    ev.type       = NetworkEventType::RequestStarted;
    ev.headers.push_back({"Authorization", "Bearer secret-token"});
    ev.headers.push_back({"Content-Type", "application/json"});
    buf.Add(ev);

    recorder.StopRecording();
    buf.RemoveObserver(&recorder);

    const auto content = ReadFile(path);
    // Authorization should be redacted.
    assert(content.find("secret-token") == std::string::npos);
    assert(content.find("<redacted>") != std::string::npos);
    // Non-sensitive headers preserved.
    assert(content.find("application/json") != std::string::npos);
    std::filesystem::remove(path);
    std::cout << "PASS: TestObtraceRedactsSensitiveHeaders\n";
}

static void TestConnectionIdFieldInObtrace() {
    NetworkRequestSummary req;
    req.request_id    = "r-conn";
    req.url           = "https://h2.example.com/";
    req.connection_id = "conn-42";
    req.state         = RequestState::Finished;

    const auto result = NetworkTraceBuffer::ExportToObtrace({req});
    assert(result.find("conn-42") != std::string::npos);
    std::cout << "PASS: TestConnectionIdFieldInObtrace\n";
}

int main() {
    TestStartStopRecording();
    TestDoubleStartReturnsFalse();
    TestEventWrittenToFile();
    TestSentinelContainsTotalEvents();
    TestExportToObtraceStatic();
    TestObtraceRedactsSensitiveHeaders();
    TestConnectionIdFieldInObtrace();
    std::cout << "All obtrace_recorder_tests PASSED.\n";
    return 0;
}
