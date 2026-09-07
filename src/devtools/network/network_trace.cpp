#include "devtools/network/network_trace.h"
#include "core/network/filter_decision_log.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace openbrowser::devtools::network {
namespace {

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::int64_t NowEpochMs() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string FormatIso8601Utc(const std::int64_t epoch_ms) {
    if (epoch_ms <= 0) {
        return {};
    }

    const std::time_t seconds = static_cast<std::time_t>(epoch_ms / 1000);
    const std::tm* utc = std::gmtime(&seconds);
    if (utc == nullptr) {
        return {};
    }

    std::ostringstream out;
    out << std::put_time(utc, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << (epoch_ms % 1000) << 'Z';
    return out.str();
}

void EscapeJsonString(const std::string_view str, std::string& out) {
    out += '"';
    for (const char c : str) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    out += '"';
}

bool IsSensitiveQueryKey(const std::string_view raw_key) {
    static constexpr std::array sensitive_keys{
        "token",
        "access_token",
        "id_token",
        "refresh_token",
        "api_key",
        "apikey",
        "auth",
        "authorization",
        "password",
        "passwd",
        "secret",
    };

    std::string key(raw_key);
    key = LowerAscii(std::move(key));
    return std::find(sensitive_keys.begin(), sensitive_keys.end(), key) != sensitive_keys.end();
}

std::vector<Header> SafeHeaders(std::vector<Header> headers) {
    NetworkTraceBuffer::RedactSensitiveHeaders(headers);
    return headers;
}

}  // namespace

NetworkTraceBuffer::NetworkTraceBuffer(CapturePolicy policy) : policy_(std::move(policy)) {
    if (policy_.max_events == 0) {
        policy_.max_events = 1;
    }
}

void NetworkTraceBuffer::AddObserver(NetworkTraceObserver* observer) {
    if (observer == nullptr) {
        return;
    }
    if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end()) {
        observers_.push_back(observer);
    }
}

void NetworkTraceBuffer::RemoveObserver(NetworkTraceObserver* observer) noexcept {
    observers_.erase(
        std::remove(observers_.begin(), observers_.end(), observer),
        observers_.end());
}

void NetworkTraceBuffer::OnNetworkEvent(NetworkEvent event) {
    Add(std::move(event));
}

void NetworkTraceBuffer::Add(NetworkEvent event) {
    if (policy_.redact_sensitive_headers) {
        RedactSensitiveHeaders(event.headers);
    }

    if (event.observed_at_ms <= 0) {
        event.observed_at_ms = NowEpochMs();
    }
    event.sequence = next_sequence_++;

    while (events_.size() >= policy_.max_events) {
        events_.pop_front();
        ++dropped_event_count_;
    }

    events_.push_back(event);
    NotifyEventAppended(event);
}

void NetworkTraceBuffer::Clear() noexcept {
    events_.clear();
    next_sequence_ = 1;
    dropped_event_count_ = 0;
    NotifyTraceCleared();
}

const std::deque<NetworkEvent>& NetworkTraceBuffer::Events() const noexcept {
    return events_;
}

std::size_t NetworkTraceBuffer::DroppedEventCount() const noexcept {
    return dropped_event_count_;
}

const CapturePolicy& NetworkTraceBuffer::Policy() const noexcept {
    return policy_;
}

std::vector<NetworkRequestSummary> NetworkTraceBuffer::AggregateRequests(
    const std::optional<core::TabId>& filter_tab_id) const {
    std::vector<NetworkRequestSummary> summaries;
    std::unordered_map<std::string, std::size_t> request_index_map;

    for (const auto& event : events_) {
        if (filter_tab_id.has_value() && event.tab_id.has_value() && *event.tab_id != *filter_tab_id) {
            continue;
        }

        auto it = request_index_map.find(event.request_id);
        if (it == request_index_map.end()) {
            std::vector<Header> req_headers;
            std::vector<Header> res_headers;
            if (event.type == NetworkEventType::RequestStarted) {
                req_headers = event.headers;
            } else if (event.type == NetworkEventType::ResponseReceived ||
                       event.type == NetworkEventType::Redirect) {
                res_headers = event.headers;
            }

            NetworkRequestSummary summary{
                .request_id = event.request_id,
                .tab_id = event.tab_id,
                .method = event.method.empty() ? "GET" : event.method,
                .url = event.url,
                .status = event.status,
                .protocol = event.type == NetworkEventType::ResponseReceived ? event.protocol : std::string{},
                .transferred_bytes = event.transferred_bytes,
                .state = RequestState::Active,
                .error = event.error,
                .request_headers = std::move(req_headers),
                .response_headers = std::move(res_headers),
                .body_preview = event.body_preview,
                .attribution = event.attribution,
                .connection_id = event.connection_id,
                .filter_blocked = false,
                .filter_rule_source = {},
                .filter_layer = {},
                .started_at_ms = event.observed_at_ms,
                .ended_at_ms = event.observed_at_ms,
                .observed_duration_ms = 0,
            };

            if (event.type == NetworkEventType::RequestFinished) {
                summary.state = RequestState::Finished;
            } else if (event.type == NetworkEventType::RequestFailed) {
                summary.state = RequestState::Failed;
            }

            request_index_map[event.request_id] = summaries.size();
            summaries.push_back(std::move(summary));
        } else {
            auto& summary = summaries[it->second];
            if (event.attribution != core::NetworkAttribution::Page) {
                summary.attribution = event.attribution;
            }
            if (event.connection_id.has_value()) {
                summary.connection_id = event.connection_id;
            }
            if (!event.url.empty() && (summary.url.empty() || event.type == NetworkEventType::Redirect)) {
                summary.url = event.url;
            }
            if (!event.method.empty() && (summary.method.empty() || event.type == NetworkEventType::RequestStarted)) {
                summary.method = event.method;
            }
            if (event.status.has_value()) {
                summary.status = event.status;
            }
            // CEF request/redirect/failure callbacks do not expose negotiated
            // transport protocol. Only a response event is allowed to publish
            // protocol data into the aggregate summary.
            if (event.type == NetworkEventType::ResponseReceived && !event.protocol.empty()) {
                summary.protocol = event.protocol;
            }
            if (!event.tab_id.has_value() && summary.tab_id.has_value()) {
                // keep tab_id
            } else if (event.tab_id.has_value()) {
                summary.tab_id = event.tab_id;
            }
            summary.transferred_bytes += event.transferred_bytes;

            if (event.observed_at_ms > 0) {
                if (summary.started_at_ms <= 0 || event.observed_at_ms < summary.started_at_ms) {
                    summary.started_at_ms = event.observed_at_ms;
                }
                if (event.observed_at_ms > summary.ended_at_ms) {
                    summary.ended_at_ms = event.observed_at_ms;
                }
                summary.observed_duration_ms = std::max<std::int64_t>(
                    0, summary.ended_at_ms - summary.started_at_ms);
            }

            if (event.type == NetworkEventType::RequestStarted && !event.headers.empty()) {
                summary.request_headers = event.headers;
            } else if ((event.type == NetworkEventType::ResponseReceived ||
                        event.type == NetworkEventType::Redirect) &&
                       !event.headers.empty()) {
                summary.response_headers = event.headers;
            }

            if (!event.body_preview.empty()) {
                summary.body_preview = event.body_preview;
            }

            if (event.type == NetworkEventType::RequestFinished) {
                summary.state = RequestState::Finished;
            } else if (event.type == NetworkEventType::RequestFailed) {
                summary.state = RequestState::Failed;
                summary.error = event.error;
            }
        }
    }

    if (decision_log_ != nullptr) {
        for (auto& summary : summaries) {
            if (const auto dec = decision_log_->FindByRequestId(summary.request_id); dec.has_value()) {
                summary.filter_blocked = dec->blocked;
                summary.filter_rule_source = dec->rule_source;
                summary.filter_layer = core::FilterDecisionLayerToString(dec->layer);
            }
        }
    }

    return summaries;
}

void NetworkTraceBuffer::SetDecisionLog(const core::FilterDecisionLog* log) noexcept {
    decision_log_ = log;
}

const core::FilterDecisionLog* NetworkTraceBuffer::DecisionLog() const noexcept {
    return decision_log_;
}

std::vector<NetworkRequestSummary> NetworkTraceBuffer::QueryRequests(
    const NetworkTraceFilter& filter) const {
    const auto all = AggregateRequests(filter.tab_id);
    if (filter.search_query.empty() &&
        (filter.method_filter.empty() || filter.method_filter == "ALL") &&
        (filter.status_filter.empty() || filter.status_filter == "ALL") &&
        filter.header_name.empty()) {
        return all;
    }

    const std::string lower_query = LowerAscii(filter.search_query);
    const std::string lower_method = LowerAscii(filter.method_filter);
    const std::string lower_header = LowerAscii(filter.header_name);
    const std::string upper_status = filter.status_filter;

    std::vector<NetworkRequestSummary> matched;
    for (const auto& req : all) {
        if (!lower_method.empty() && lower_method != "all") {
            if (LowerAscii(req.method) != lower_method) {
                continue;
            }
        }

        if (!upper_status.empty() && upper_status != "ALL") {
            if (upper_status == "ERR") {
                const bool is_err = req.state == RequestState::Failed ||
                                    (req.status.has_value() && *req.status >= 400);
                if (!is_err) {
                    continue;
                }
            } else if (upper_status == "2XX") {
                if (!req.status.has_value() || *req.status < 200 || *req.status >= 300) {
                    continue;
                }
            } else if (upper_status == "3XX") {
                if (!req.status.has_value() || *req.status < 300 || *req.status >= 400) {
                    continue;
                }
            } else if (upper_status == "4XX") {
                if (!req.status.has_value() || *req.status < 400 || *req.status >= 500) {
                    continue;
                }
            } else if (upper_status == "5XX") {
                if (!req.status.has_value() || *req.status < 500 || *req.status >= 600) {
                    continue;
                }
            }
        }

        if (!lower_header.empty()) {
            bool has_header = false;
            for (const auto& h : req.request_headers) {
                if (LowerAscii(h.name).find(lower_header) != std::string::npos) {
                    has_header = true;
                    break;
                }
            }
            if (!has_header) {
                for (const auto& h : req.response_headers) {
                    if (LowerAscii(h.name).find(lower_header) != std::string::npos) {
                        has_header = true;
                        break;
                    }
                }
            }
            if (!has_header) {
                continue;
            }
        }

        if (!lower_query.empty()) {
            bool query_matched = false;
            if (LowerAscii(req.url).find(lower_query) != std::string::npos) {
                query_matched = true;
            } else if (LowerAscii(req.method).find(lower_query) != std::string::npos) {
                query_matched = true;
            } else if (req.status.has_value() &&
                       std::to_string(*req.status).find(lower_query) != std::string::npos) {
                query_matched = true;
            } else if (LowerAscii(req.error).find(lower_query) != std::string::npos) {
                query_matched = true;
            } else if (LowerAscii(req.body_preview).find(lower_query) != std::string::npos) {
                query_matched = true;
            } else {
                for (const auto& h : req.request_headers) {
                    if (LowerAscii(h.name).find(lower_query) != std::string::npos ||
                        LowerAscii(h.value).find(lower_query) != std::string::npos) {
                        query_matched = true;
                        break;
                    }
                }
                if (!query_matched) {
                    for (const auto& h : req.response_headers) {
                        if (LowerAscii(h.name).find(lower_query) != std::string::npos ||
                            LowerAscii(h.value).find(lower_query) != std::string::npos) {
                            query_matched = true;
                            break;
                        }
                    }
                }
            }

            if (!query_matched) {
                continue;
            }
        }

        matched.push_back(req);
    }

    return matched;
}

std::optional<NetworkRequestSummary> NetworkTraceBuffer::FindRequest(
    const std::string& request_id) const {
    const auto summaries = AggregateRequests();
    for (const auto& s : summaries) {
        if (s.request_id == request_id) {
            return s;
        }
    }
    return std::nullopt;
}

std::string NetworkTraceBuffer::ExportToHar(
    const std::vector<NetworkRequestSummary>& requests) {
    std::string out;
    out.reserve(requests.size() * 640 + 256);

    out += "{\n  \"log\": {\n    \"version\": \"1.2\",\n";
    out += "    \"creator\": {\n      \"name\": \"Openbrowser Network Lab\",\n      \"version\": \"1.0\"\n    },\n";
    out += "    \"entries\": [";

    for (std::size_t i = 0; i < requests.size(); ++i) {
        const auto& req = requests[i];
        const auto request_headers = SafeHeaders(req.request_headers);
        const auto response_headers = SafeHeaders(req.response_headers);
        const auto safe_url = RedactSensitiveUrl(req.url);
        const auto start_time = FormatIso8601Utc(req.started_at_ms);
        const auto protocol = req.protocol.empty() ? std::string{"unknown"} : req.protocol;

        if (i > 0) {
            out += ",";
        }
        out += "\n      {\n";
        out += "        \"startedDateTime\": ";
        EscapeJsonString(start_time, out);
        out += ",\n        \"time\": " + std::to_string(std::max<std::int64_t>(0, req.observed_duration_ms)) + ",\n";
        out += "        \"_openbrowserTimingBasis\": \"observed callback span; transport phases unavailable\",\n";
        out += "        \"_attribution\": ";
        EscapeJsonString(core::AttributionToString(req.attribution), out);
        out += ",\n";
        out += "        \"_filterBlocked\": ";
        out += req.filter_blocked ? "true" : "false";
        out += ",\n";
        out += "        \"request\": {\n";
        out += "          \"method\": ";
        EscapeJsonString(req.method, out);
        out += ",\n          \"url\": ";
        EscapeJsonString(safe_url, out);
        out += ",\n          \"httpVersion\": ";
        EscapeJsonString(protocol, out);
        out += ",\n          \"headers\": [";
        for (std::size_t h = 0; h < request_headers.size(); ++h) {
            if (h > 0) {
                out += ",";
            }
            out += "\n            {\"name\": ";
            EscapeJsonString(request_headers[h].name, out);
            out += ", \"value\": ";
            EscapeJsonString(request_headers[h].value, out);
            out += "}";
        }
        out += "\n          ],\n";
        out += "          \"headersSize\": -1,\n";
        out += "          \"bodySize\": -1\n";
        out += "        },\n";
        out += "        \"response\": {\n";
        out += "          \"status\": " + std::to_string(req.status.value_or(0)) + ",\n";
        out += "          \"statusText\": ";
        EscapeJsonString(req.error, out);
        out += ",\n          \"httpVersion\": ";
        EscapeJsonString(protocol, out);
        out += ",\n          \"headers\": [";
        for (std::size_t h = 0; h < response_headers.size(); ++h) {
            if (h > 0) {
                out += ",";
            }
            out += "\n            {\"name\": ";
            EscapeJsonString(response_headers[h].name, out);
            out += ", \"value\": ";
            EscapeJsonString(response_headers[h].value, out);
            out += "}";
        }
        out += "\n          ],\n";
        out += "          \"content\": {\n";
        out += "            \"size\": " + std::to_string(req.transferred_bytes) + ",\n";
        out += "            \"mimeType\": \"\"\n";
        out += "          },\n";
        out += "          \"headersSize\": -1,\n";
        out += "          \"bodySize\": " + std::to_string(req.transferred_bytes) + "\n";
        out += "        },\n";
        if (!req.body_preview.empty()) {
            out += "        \"_openbrowserBodyPreview\": \"<redacted>\",\n";
        }
        out += "        \"cache\": {},\n";
        out += "        \"timings\": {\"send\": -1, \"wait\": -1, \"receive\": -1}\n";
        out += "      }";
    }

    out += "\n    ]\n  }\n}\n";
    return out;
}

std::string NetworkTraceBuffer::ExportToObtrace(
    const std::vector<NetworkRequestSummary>& requests) {
    std::string out;
    out += "{\"schema\":\"obtrace/1\",\"type\":\"trace_start\",\"total_requests\":";
    out += std::to_string(requests.size());
    out += "}\n";

    for (const auto& req : requests) {
        out += "{\"type\":\"request\",\"schema\":\"obtrace/1\",\"request_id\":";
        EscapeJsonString(req.request_id, out);
        out += ",\"method\":";
        EscapeJsonString(req.method, out);
        out += ",\"url\":";
        EscapeJsonString(RedactSensitiveUrl(req.url), out);
        out += ",\"status\":";
        out += req.status.has_value() ? std::to_string(*req.status) : "null";
        out += ",\"protocol\":";
        EscapeJsonString(req.protocol.empty() ? "unknown" : req.protocol, out);
        out += ",\"transferred_bytes\":";
        out += std::to_string(req.transferred_bytes);
        out += ",\"observed_started_at_ms\":" + std::to_string(req.started_at_ms);
        out += ",\"observed_duration_ms\":" + std::to_string(std::max<std::int64_t>(0, req.observed_duration_ms));
        out += ",\"state\":\"";
        switch (req.state) {
            case RequestState::Active:   out += "active";   break;
            case RequestState::Finished: out += "finished"; break;
            case RequestState::Failed:   out += "failed";   break;
        }
        out += "\"";
        out += ",\"attribution\":";
        EscapeJsonString(core::AttributionToString(req.attribution), out);
        if (req.connection_id.has_value()) {
            out += ",\"connection_id\":";
            EscapeJsonString(*req.connection_id, out);
        }
        if (req.filter_blocked) {
            out += ",\"filter_blocked\":true,\"filter_layer\":";
            EscapeJsonString(req.filter_layer, out);
            out += ",\"filter_rule_source\":";
            EscapeJsonString(req.filter_rule_source, out);
        } else {
            out += ",\"filter_blocked\":false";
        }
        if (!req.body_preview.empty()) {
            out += ",\"body_preview\":\"<redacted>\"";
        }
        out += "}\n";
    }

    out += "{\"type\":\"trace_end\",\"schema\":\"obtrace/1\",\"total_events\":";
    out += std::to_string(requests.size());
    out += "}\n";
    return out;
}

bool NetworkTraceBuffer::IsSensitiveHeaderName(const std::string& name) {
    static constexpr std::array sensitive_names{
        "authorization",
        "proxy-authorization",
        "cookie",
        "set-cookie",
        "x-api-key",
        "x-auth-token",
        "x-csrf-token",
        "x-xsrf-token",
    };

    const auto normalized = LowerAscii(name);
    return std::find(sensitive_names.begin(), sensitive_names.end(), normalized) != sensitive_names.end();
}

void NetworkTraceBuffer::RedactSensitiveHeaders(std::vector<Header>& headers) {
    for (auto& header : headers) {
        if (IsSensitiveHeaderName(header.name)) {
            header.value = "<redacted>";
        }
    }
}

std::string NetworkTraceBuffer::RedactSensitiveUrl(std::string url) {
    // Redact URL userinfo (https://user:pass@example/...).
    const auto scheme_pos = url.find("://");
    if (scheme_pos != std::string::npos) {
        const auto authority_start = scheme_pos + 3;
        const auto authority_end = url.find_first_of("/?#", authority_start);
        const auto bounded_end = authority_end == std::string::npos ? url.size() : authority_end;
        const auto at = url.rfind('@', bounded_end);
        if (at != std::string::npos && at >= authority_start && at < bounded_end) {
            url.replace(authority_start, at - authority_start, "<redacted>");
        }
    }

    const auto query_pos = url.find('?');
    if (query_pos == std::string::npos) {
        return url;
    }

    const auto fragment_pos = url.find('#', query_pos + 1);
    const std::string fragment = fragment_pos == std::string::npos ? std::string{} : url.substr(fragment_pos);
    const auto query_end = fragment_pos == std::string::npos ? url.size() : fragment_pos;
    const std::string prefix = url.substr(0, query_pos + 1);
    const std::string query = url.substr(query_pos + 1, query_end - (query_pos + 1));

    std::string rebuilt;
    rebuilt.reserve(url.size() + 16);
    rebuilt += prefix;

    std::size_t cursor = 0;
    bool first = true;
    while (cursor <= query.size()) {
        const auto amp = query.find('&', cursor);
        const auto pair_end = amp == std::string::npos ? query.size() : amp;
        const std::string_view pair(query.data() + cursor, pair_end - cursor);

        if (!first) {
            rebuilt += '&';
        }
        first = false;

        const auto equals = pair.find('=');
        if (equals == std::string_view::npos) {
            rebuilt.append(pair.data(), pair.size());
        } else {
            const auto key = pair.substr(0, equals);
            rebuilt.append(key.data(), key.size());
            rebuilt += '=';
            if (IsSensitiveQueryKey(key)) {
                rebuilt += "<redacted>";
            } else {
                const auto value = pair.substr(equals + 1);
                rebuilt.append(value.data(), value.size());
            }
        }

        if (amp == std::string::npos) {
            break;
        }
        cursor = amp + 1;
    }

    rebuilt += fragment;
    return rebuilt;
}

void NetworkTraceBuffer::NotifyEventAppended(const NetworkEvent& event) {
    const auto observers_copy = observers_;
    for (auto* observer : observers_copy) {
        if (observer != nullptr) {
            observer->OnTraceEventAppended(event);
        }
    }
}

void NetworkTraceBuffer::NotifyTraceCleared() {
    const auto observers_copy = observers_;
    for (auto* observer : observers_copy) {
        if (observer != nullptr) {
            observer->OnTraceCleared();
        }
    }
}

}  // namespace openbrowser::devtools::network
