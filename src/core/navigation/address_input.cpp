#include "core/navigation/address_input.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace openbrowser::core::navigation {
namespace {

bool IsAsciiSpace(const unsigned char character) {
    return std::isspace(character) != 0;
}

void TrimAsciiWhitespace(std::string& value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](const unsigned char character) {
        return IsAsciiSpace(character);
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char character) {
        return IsAsciiSpace(character);
    }).base();

    if (first >= last) {
        value.clear();
        return;
    }

    value = std::string(first, last);
}

char LowerAscii(const char character) {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character - 'A' + 'a');
    }
    return character;
}

bool StartsWithCaseInsensitive(const std::string_view value, const std::string_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }

    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (LowerAscii(value[index]) != LowerAscii(prefix[index])) {
            return false;
        }
    }
    return true;
}

bool HasExplicitScheme(const std::string_view value) {
    if (value.empty() || !std::isalpha(static_cast<unsigned char>(value.front()))) {
        return false;
    }

    for (std::size_t index = 1; index < value.size(); ++index) {
        const char character = value[index];
        if (character == ':') {
            return true;
        }
        if (!(std::isalnum(static_cast<unsigned char>(character)) || character == '+' || character == '-' || character == '.')) {
            return false;
        }
    }
    return false;
}

bool IsLoopbackAddress(const std::string_view value) {
    if (StartsWithCaseInsensitive(value, "localhost") &&
        (value.size() == 9 || value[9] == ':' || value[9] == '/' || value[9] == '?' || value[9] == '#')) {
        return true;
    }

    if (value.starts_with("127.")) {
        return true;
    }

    return value.starts_with("[::1]");
}

bool LooksLikeIpv4Literal(const std::string_view value) {
    const std::size_t host_end = value.find_first_of("/?#:");
    const std::string_view host = (host_end == std::string_view::npos) ? value : value.substr(0, host_end);
    if (host.empty()) {
        return false;
    }

    int dots = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= host.size(); ++i) {
        if (i == host.size() || host[i] == '.') {
            const std::string_view octet = host.substr(start, i - start);
            if (octet.empty() || octet.size() > 3) {
                return false;
            }
            int val = 0;
            for (const char ch : octet) {
                if (!std::isdigit(static_cast<unsigned char>(ch))) {
                    return false;
                }
                val = val * 10 + (ch - '0');
            }
            if (val < 0 || val > 255) {
                return false;
            }
            if (i < host.size()) {
                ++dots;
                start = i + 1;
            }
        }
    }
    if (dots != 3) {
        return false;
    }

    if (host_end != std::string_view::npos && value[host_end] == ':') {
        const std::size_t port_end = value.find_first_of("/?#", host_end + 1);
        const std::string_view port = (port_end == std::string_view::npos)
            ? value.substr(host_end + 1)
            : value.substr(host_end + 1, port_end - (host_end + 1));
        if (port.empty() || port.size() > 5) {
            return false;
        }
        for (const char ch : port) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                return false;
            }
        }
    }

    return true;
}

bool LooksLikeIpv6Literal(const std::string_view value) {
    if (value.empty() || value.front() != '[') {
        return false;
    }
    const std::size_t closing = value.find(']');
    if (closing == std::string_view::npos || closing < 2) {
        return false;
    }
    const std::string_view inside = value.substr(1, closing - 1);
    bool has_colon = false;
    for (const char ch : inside) {
        if (ch == ':') {
            has_colon = true;
        } else if (!std::isxdigit(static_cast<unsigned char>(ch)) && ch != '.') {
            return false;
        }
    }
    if (!has_colon) {
        return false;
    }

    if (closing + 1 < value.size()) {
        const char next = value[closing + 1];
        if (next == ':') {
            const std::size_t port_end = value.find_first_of("/?#", closing + 2);
            const std::string_view port = (port_end == std::string_view::npos)
                ? value.substr(closing + 2)
                : value.substr(closing + 2, port_end - (closing + 2));
            if (port.empty() || port.size() > 5) {
                return false;
            }
            for (const char ch : port) {
                if (!std::isdigit(static_cast<unsigned char>(ch))) {
                    return false;
                }
            }
        } else if (next != '/' && next != '?' && next != '#') {
            return false;
        }
    }

    return true;
}

bool LooksLikeHostOrDomain(const std::string_view value) {
    const std::size_t host_end = value.find_first_of("/?#:");
    const std::string_view host = (host_end == std::string_view::npos) ? value : value.substr(0, host_end);
    if (host.empty()) {
        return false;
    }

    const std::size_t dot_pos = host.rfind('.');
    if (dot_pos == std::string_view::npos || dot_pos == 0 || dot_pos == host.size() - 1) {
        return false;
    }

    const std::string_view tld = host.substr(dot_pos + 1);
    if (tld.size() < 2) {
        return false;
    }
    for (const char c : tld) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

}  // namespace

std::string UrlEncode(const std::string_view value) {
    std::string encoded;
    encoded.reserve(value.size() * 3 / 2);
    static const char hex_chars[] = "0123456789ABCDEF";

    for (const char ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(ch);
        } else if (c == ' ') {
            encoded.push_back('+');
        } else {
            encoded.push_back('%');
            encoded.push_back(hex_chars[(c >> 4) & 0x0F]);
            encoded.push_back(hex_chars[c & 0x0F]);
        }
    }
    return encoded;
}

std::optional<std::string> NormalizeAddressInput(std::string input) {
    TrimAsciiWhitespace(input);
    if (input.empty()) {
        return std::nullopt;
    }

    if (std::any_of(input.begin(), input.end(), [](const unsigned char character) {
            return std::isspace(character) != 0 || std::iscntrl(character) != 0;
        })) {
        return std::nullopt;
    }

    if (StartsWithCaseInsensitive(input, "https://") || StartsWithCaseInsensitive(input, "http://")) {
        return input;
    }

    if (IsLoopbackAddress(input)) {
        return "http://" + input;
    }

    if (HasExplicitScheme(input)) {
        return std::nullopt;
    }

    if (LooksLikeIpv4Literal(input) || LooksLikeIpv6Literal(input) || LooksLikeHostOrDomain(input)) {
        return "https://" + input;
    }

    return std::nullopt;
}

std::optional<std::string> ResolveAddressInput(
    std::string input,
    const SearchProvider& provider) {
    TrimAsciiWhitespace(input);
    if (input.empty()) {
        return std::nullopt;
    }

    if (StartsWithCaseInsensitive(input, "https://") ||
        StartsWithCaseInsensitive(input, "http://") ||
        StartsWithCaseInsensitive(input, "about:")) {
        return input;
    }

    if (IsLoopbackAddress(input)) {
        return "http://" + input;
    }

    const bool has_spaces_or_cntrl = std::any_of(input.begin(), input.end(), [](const unsigned char character) {
        return std::isspace(character) != 0 || std::iscntrl(character) != 0;
    });

    if (!has_spaces_or_cntrl && !HasExplicitScheme(input)) {
        if (LooksLikeIpv4Literal(input) || LooksLikeIpv6Literal(input) || LooksLikeHostOrDomain(input)) {
            return "https://" + input;
        }
    }

    const std::string encoded_query = UrlEncode(input);
    std::string search_url = provider.search_url_template;
    const std::size_t pos = search_url.find("%s");
    if (pos != std::string::npos) {
        search_url.replace(pos, 2, encoded_query);
    } else {
        search_url += encoded_query;
    }
    return search_url;
}

}  // namespace openbrowser::core::navigation
