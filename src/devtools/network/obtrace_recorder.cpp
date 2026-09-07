#include "devtools/network/obtrace_recorder.h"

#include <cctype>
#include <cstdio>
#include <utility>

namespace openbrowser::devtools::network {

namespace {

// Minimal JSON string escaper — mirrors the one in network_trace.cpp.
void EscapeStr(const std::string_view sv, std::string& out) {
    out += '"';
    for (const char c : sv) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                        static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    out += '"';
}

const char* EventTypeName(NetworkEventType t) noexcept {
    switch (t) {
        case NetworkEventType::RequestStarted:       return "request_started";
        case NetworkEventType::Redirect:             return "redirect";
        case NetworkEventType::ResponseReceived:     return "response_received";
        case NetworkEventType::DataReceived:         return "data_received";
        case NetworkEventType::RequestFinished:      return "request_finished";
        case NetworkEventType::RequestFailed:        return "request_failed";
        case NetworkEventType::WebSocketOpened:      return "ws_opened";
        case NetworkEventType::WebSocketFrame:       return "ws_frame";
        case NetworkEventType::WebSocketClosed:      return "ws_closed";
        case NetworkEventType::DnsResolved:          return "dns_resolved";
        case NetworkEventType::ConnectionEstablished: return "connection_established";
        case NetworkEventType::TlsHandshaked:        return "tls_handshaked";
    }
    return "unknown";
}

} // namespace

ObtraceRecorder::~ObtraceRecorder() {
    StopRecording();
}

bool ObtraceRecorder::StartRecording(const std::string& path) {
    if (recording_) {
        return false;
    }
    file_.open(path, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        return false;
    }
    path_        = path;
    event_count_ = 0;
    recording_   = true;

    // Write schema header record.
    file_ << R"({"schema":"obtrace/1","type":"trace_start"})" << '\n';
    file_.flush();
    return true;
}

void ObtraceRecorder::StopRecording() {
    if (!recording_) {
        return;
    }
    recording_ = false;
    // Write sentinel.
    file_ << "{\"schema\":\"obtrace/1\",\"type\":\"trace_end\",\"total_events\":"
          << event_count_ << "}\n";
    file_.flush();
    file_.close();
}

bool ObtraceRecorder::IsRecording() const noexcept {
    return recording_;
}

std::size_t ObtraceRecorder::GetRecordedEventCount() const noexcept {
    return event_count_;
}

const std::string& ObtraceRecorder::GetFilePath() const noexcept {
    return path_;
}

void ObtraceRecorder::OnTraceEventAppended(const NetworkEvent& event) {
    if (!recording_) {
        return;
    }
    WriteEvent(event);
}

void ObtraceRecorder::OnTraceCleared() {
    // Clearing while recording emits a marker but keeps the file open.
    if (!recording_) {
        return;
    }
    file_ << R"({"schema":"obtrace/1","type":"trace_cleared"})" << '\n';
    file_.flush();
}

void ObtraceRecorder::WriteEvent(const NetworkEvent& event) {
    std::string line;
    line.reserve(256);

    line += "{\"schema\":\"obtrace/1\",\"type\":\"";
    line += EventTypeName(event.type);
    line += "\",\"seq\":";
    line += std::to_string(event.sequence);
    line += ",\"request_id\":";
    EscapeStr(event.request_id, line);
    line += ",\"url\":";
    EscapeStr(event.url, line);
    line += ",\"method\":";
    EscapeStr(event.method, line);

    if (event.status.has_value()) {
        line += ",\"status\":";
        line += std::to_string(*event.status);
    }
    if (!event.protocol.empty()) {
        line += ",\"protocol\":";
        EscapeStr(event.protocol, line);
    }
    if (event.transferred_bytes > 0) {
        line += ",\"transferred_bytes\":";
        line += std::to_string(event.transferred_bytes);
    }
    if (!event.error.empty()) {
        line += ",\"error\":";
        EscapeStr(event.error, line);
    }
    if (event.connection_id.has_value()) {
        line += ",\"connection_id\":";
        EscapeStr(*event.connection_id, line);
    }
    // Headers (redacted if policy requires).
    if (!event.headers.empty()) {
        line += ",\"headers\":[";
        for (std::size_t i = 0; i < event.headers.size(); ++i) {
            if (i > 0) { line += ','; }
            line += "{\"name\":";
            EscapeStr(event.headers[i].name, line);
            line += ",\"value\":";
            const bool sensitive = redact_headers_ &&
                NetworkTraceBuffer::IsSensitiveHeaderName(event.headers[i].name);
            EscapeStr(sensitive ? "<redacted>" : event.headers[i].value, line);
            line += '}';
        }
        line += ']';
    }
    line += "}\n";

    file_ << line;
    file_.flush();
    ++event_count_;
}

} // namespace openbrowser::devtools::network
