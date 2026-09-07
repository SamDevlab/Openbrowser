#pragma once

#include "devtools/network/network_trace.h"

#include <string>
#include <vector>

namespace openbrowser::core {

enum class FilterComparator {
    Equal,
    NotEqual,
    GreaterThan,
    GreaterThanOrEqual,
    LessThan,
    LessThanOrEqual,
    Range
};

struct StatusClause {
    FilterComparator comp = FilterComparator::Equal;
    int min_val = 0;
    int max_val = 0;
    bool negate = false;
};

struct TermClause {
    std::string field; // "method", "host", "protocol", "is", "has", "attribution", or "" (free text)
    std::string value;
    bool negate = false;
};

class NetworkFilterQuery {
public:
    explicit NetworkFilterQuery(const std::string& query_string = "");
    ~NetworkFilterQuery() = default;

    bool Parse(const std::string& query_string);
    [[nodiscard]] const std::string& GetRawQuery() const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;

    [[nodiscard]] bool Matches(const devtools::network::NetworkRequestSummary& req) const;

    [[nodiscard]] std::vector<devtools::network::NetworkRequestSummary> Filter(
        const std::vector<devtools::network::NetworkRequestSummary>& requests) const;

private:
    std::string raw_query_;
    std::vector<StatusClause> status_clauses_;
    std::vector<TermClause> term_clauses_;

    [[nodiscard]] bool EvaluateStatus(const std::optional<int>& status) const;
    [[nodiscard]] bool EvaluateTerm(const TermClause& term, const devtools::network::NetworkRequestSummary& req) const;
};

} // namespace openbrowser::core
