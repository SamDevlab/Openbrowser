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

    return "https://" + input;
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

    if (!has_spaces_or_cntrl && !HasExplicitScheme(input) && LooksLikeHostOrDomain(input)) {
        return "https://" + input;
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
