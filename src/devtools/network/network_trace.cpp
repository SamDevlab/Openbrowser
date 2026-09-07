#include "devtools/network/network_trace.h"

#include <algorithm>
#include <array>
#include <cctype>
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
                .protocol = event.protocol,
                .transferred_bytes = event.transferred_bytes,
                .state = RequestState::Active,
                .error = event.error,
                .request_headers = std::move(req_headers),
                .response_headers = std::move(res_headers),
                .body_preview = event.body_preview,
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
            if (!event.url.empty()) {
                summary.url = event.url;
            }
            if (!event.method.empty()) {
                if (summary.method == "GET" || event.type == NetworkEventType::RequestStarted) {
                    summary.method = event.method;
                }
            }
            if (event.status.has_value()) {
                summary.status = event.status;
            }
            if (!event.protocol.empty()) {
                summary.protocol = event.protocol;
            }
            if (!event.tab_id.has_value() && summary.tab_id.has_value()) {
                // keep tab_id
            } else if (event.tab_id.has_value()) {
                summary.tab_id = event.tab_id;
            }
            summary.transferred_bytes += event.transferred_bytes;

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

    return summaries;
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
