#include "core/network/network_filter_query.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace openbrowser::core {

namespace {
std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::vector<std::string> SplitWhitespace(const std::string& str) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}
int ParseIntSafe(const std::string& str, int default_val = 0) noexcept {
    try {
        return std::stoi(str);
    } catch (...) {
        return default_val;
    }
}
} // namespace

NetworkFilterQuery::NetworkFilterQuery(const std::string& query_string) {
    Parse(query_string);
}

bool NetworkFilterQuery::Parse(const std::string& query_string) {
    raw_query_ = query_string;
    status_clauses_.clear();
    term_clauses_.clear();

    const auto tokens = SplitWhitespace(query_string);
    for (std::string token : tokens) {
        bool negate = false;
        if (!token.empty() && (token[0] == '-' || token[0] == '!')) {
            negate = true;
            token = token.substr(1);
        }
        if (token.empty()) {
            continue;
        }

        const auto colon = token.find(':');
        if (colon != std::string::npos) {
            std::string field = ToLower(token.substr(0, colon));
            std::string val = token.substr(colon + 1);

            if (field == "status") {
                StatusClause sc;
                sc.negate = negate;

                if (val.rfind(">=", 0) == 0) {
                    sc.comp = FilterComparator::GreaterThanOrEqual;
                    sc.min_val = ParseIntSafe(val.substr(2));
                } else if (val.rfind(">", 0) == 0) {
                    sc.comp = FilterComparator::GreaterThan;
                    sc.min_val = ParseIntSafe(val.substr(1));
                } else if (val.rfind("<=", 0) == 0) {
                    sc.comp = FilterComparator::LessThanOrEqual;
                    sc.min_val = ParseIntSafe(val.substr(2));
                } else if (val.rfind("<", 0) == 0) {
                    sc.comp = FilterComparator::LessThan;
                    sc.min_val = ParseIntSafe(val.substr(1));
                } else if (const auto dots = val.find(".."); dots != std::string::npos) {
                    sc.comp = FilterComparator::Range;
                    sc.min_val = ParseIntSafe(val.substr(0, dots));
                    sc.max_val = ParseIntSafe(val.substr(dots + 2));
                } else {
                    sc.comp = FilterComparator::Equal;
                    sc.min_val = ParseIntSafe(val);
                }
                status_clauses_.push_back(sc);
            } else {
                TermClause tc;
                tc.field = field;
                tc.value = ToLower(val);
                tc.negate = negate;
                term_clauses_.push_back(tc);
            }
        } else {
            TermClause tc;
            tc.field = ""; // free text
            tc.value = ToLower(token);
            tc.negate = negate;
            term_clauses_.push_back(tc);
        }
    }

    return true;
}

const std::string& NetworkFilterQuery::GetRawQuery() const noexcept {
    return raw_query_;
}

bool NetworkFilterQuery::IsEmpty() const noexcept {
    return status_clauses_.empty() && term_clauses_.empty();
}

bool NetworkFilterQuery::EvaluateStatus(const std::optional<int>& status) const {
    for (const auto& sc : status_clauses_) {
        bool match = false;
        if (status.has_value()) {
            const int s = *status;
            switch (sc.comp) {
            case FilterComparator::Equal:
                match = (s == sc.min_val);
                break;
            case FilterComparator::NotEqual:
                match = (s != sc.min_val);
                break;
            case FilterComparator::GreaterThan:
                match = (s > sc.min_val);
                break;
            case FilterComparator::GreaterThanOrEqual:
                match = (s >= sc.min_val);
                break;
            case FilterComparator::LessThan:
                match = (s < sc.min_val);
                break;
            case FilterComparator::LessThanOrEqual:
                match = (s <= sc.min_val);
                break;
            case FilterComparator::Range:
                match = (s >= sc.min_val && s <= sc.max_val);
                break;
            }
        }
        if (sc.negate) {
            match = !match;
        }
        if (!match) {
            return false;
        }
    }
    return true;
}

bool NetworkFilterQuery::EvaluateTerm(
    const TermClause& tc,
    const devtools::network::NetworkRequestSummary& req) const {

    bool match = false;
    if (tc.field == "method") {
        match = (ToLower(req.method) == tc.value);
    } else if (tc.field == "host") {
        match = (ToLower(req.url).find(tc.value) != std::string::npos);
    } else if (tc.field == "protocol") {
        match = (ToLower(req.protocol).find(tc.value) != std::string::npos);
    } else if (tc.field == "is") {
        if (tc.value == "failed") {
            match = (req.state == devtools::network::RequestState::Failed);
        } else if (tc.value == "active") {
            match = (req.state == devtools::network::RequestState::Active);
        } else if (tc.value == "finished") {
            match = (req.state == devtools::network::RequestState::Finished);
        } else if (tc.value == "blocked") {
            match = (!req.error.empty() && ToLower(req.error).find("block") != std::string::npos);
        }
    } else if (tc.field == "has") {
        if (tc.value == "body") {
            match = !req.body_preview.empty();
        } else if (tc.value == "error") {
            match = !req.error.empty();
        }
    } else if (tc.field == "attribution") {
        if (tc.value == "page") {
            match = (req.attribution == NetworkAttribution::Page);
        } else if (tc.value == "transfer") {
            match = (req.attribution == NetworkAttribution::Transfer);
        } else if (tc.value == "telemetry") {
            match = (req.attribution == NetworkAttribution::Telemetry);
        } else if (tc.value == "update") {
            match = (req.attribution == NetworkAttribution::UpdateCheck);
        } else if (tc.value == "filter") {
            match = (req.attribution == NetworkAttribution::FilterListSync);
        }
    } else if (tc.field.empty()) {
        // Free-text search
        const std::string text = tc.value;
        if (ToLower(req.url).find(text) != std::string::npos ||
            ToLower(req.body_preview).find(text) != std::string::npos ||
            ToLower(req.error).find(text) != std::string::npos) {
            match = true;
        } else {
            for (const auto& h : req.request_headers) {
                if (ToLower(h.name).find(text) != std::string::npos ||
                    ToLower(h.value).find(text) != std::string::npos) {
                    match = true;
                    break;
                }
            }
            if (!match) {
                for (const auto& h : req.response_headers) {
                    if (ToLower(h.name).find(text) != std::string::npos ||
                        ToLower(h.value).find(text) != std::string::npos) {
                        match = true;
                        break;
                    }
                }
            }
        }
    }

    return tc.negate ? !match : match;
}

bool NetworkFilterQuery::Matches(const devtools::network::NetworkRequestSummary& req) const {
    if (!EvaluateStatus(req.status)) {
        return false;
    }
    for (const auto& tc : term_clauses_) {
        if (!EvaluateTerm(tc, req)) {
            return false;
        }
    }
    return true;
}

std::vector<devtools::network::NetworkRequestSummary> NetworkFilterQuery::Filter(
    const std::vector<devtools::network::NetworkRequestSummary>& requests) const {

    if (IsEmpty()) {
        return requests;
    }

    std::vector<devtools::network::NetworkRequestSummary> result;
    for (const auto& r : requests) {
        if (Matches(r)) {
            result.push_back(r);
        }
    }
    return result;
}

} // namespace openbrowser::core
