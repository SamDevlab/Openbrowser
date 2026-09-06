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

}  // namespace

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

}  // namespace openbrowser::core::navigation
