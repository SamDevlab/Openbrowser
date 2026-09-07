#include "core/storage/json_helper.h"

#include <cctype>
#include <charconv>
#include <cstdio>
#include <string>

namespace openbrowser::core::storage {

const JsonValue* JsonValue::Find(const std::string_view key) const noexcept {
    if (type != Type::Object) return nullptr;
    for (const auto& [k, v] : obj_val) {
        if (k == key) return &v;
    }
    return nullptr;
}

std::string JsonValue::GetString(const std::string_view key, std::string default_val) const {
    const auto* v = Find(key);
    return (v && v->type == Type::String) ? v->str_val : default_val;
}

std::optional<std::string> JsonValue::GetOptionalString(const std::string_view key) const {
    const auto* v = Find(key);
    if (v && v->type == Type::String) return v->str_val;
    return std::nullopt;
}

bool JsonValue::GetBool(const std::string_view key, const bool default_val) const noexcept {
    const auto* v = Find(key);
    return (v && v->type == Type::Bool) ? v->bool_val : default_val;
}

double JsonValue::GetDouble(const std::string_view key, const double default_val) const noexcept {
    const auto* v = Find(key);
    return (v && v->type == Type::Number) ? v->num_val : default_val;
}

std::int64_t JsonValue::GetInt64(const std::string_view key, const std::int64_t default_val) const noexcept {
    const auto* v = Find(key);
    return (v && v->type == Type::Number) ? static_cast<std::int64_t>(v->num_val) : default_val;
}

std::size_t JsonValue::GetSizeT(const std::string_view key, const std::size_t default_val) const noexcept {
    const auto* v = Find(key);
    if (v && v->type == Type::Number && v->num_val >= 0.0) {
        return static_cast<std::size_t>(v->num_val);
    }
    return default_val;
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

namespace {

class JsonParser {
public:
    explicit JsonParser(const std::string_view input) : input_(input) {}

    std::optional<JsonValue> Parse() {
        SkipWhitespace();
        if (pos_ >= input_.size()) return std::nullopt;
        auto val = ParseValue();
        SkipWhitespace();
        if (pos_ != input_.size()) return std::nullopt;
        return val;
    }

private:
    void SkipWhitespace() {
        while (pos_ < input_.size() &&
               (input_[pos_] == ' ' || input_[pos_] == '\t' ||
                input_[pos_] == '\n' || input_[pos_] == '\r')) {
            ++pos_;
        }
    }

    std::optional<JsonValue> ParseValue() {
        SkipWhitespace();
        if (pos_ >= input_.size()) return std::nullopt;

        const char c = input_[pos_];
        if (c == '"') return ParseStringValue();
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == 't' || c == 'f') return ParseBool();
        if (c == 'n') return ParseNull();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber();

        return std::nullopt;
    }

    static void AppendUtf8Codepoint(const unsigned int codepoint, std::string& out) {
        if (codepoint <= 0x7F) {
            out += static_cast<char>(codepoint);
        } else if (codepoint <= 0x7FF) {
            out += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= 0xFFFF) {
            out += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= 0x10FFFF) {
            out += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
            out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }

    std::optional<std::string> ParseString() {
        if (pos_ >= input_.size() || input_[pos_] != '"') return std::nullopt;
        ++pos_;
        std::string result;

        while (pos_ < input_.size()) {
            const char c = input_[pos_++];
            if (c == '"') return result;
            if (c == '\\') {
                if (pos_ >= input_.size()) return std::nullopt;
                const char esc = input_[pos_++];
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 > input_.size()) return std::nullopt;
                        unsigned int codepoint = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = input_[pos_++];
                            codepoint <<= 4;
                            if (h >= '0' && h <= '9') codepoint |= static_cast<unsigned int>(h - '0');
                            else if (h >= 'a' && h <= 'f') codepoint |= static_cast<unsigned int>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') codepoint |= static_cast<unsigned int>(h - 'A' + 10);
                            else return std::nullopt;
                        }
                        AppendUtf8Codepoint(codepoint, result);
                        break;
                    }
                    default:
                        result += esc;
                        break;
                }
            } else {
                result += c;
            }
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseStringValue() {
        auto str = ParseString();
        if (!str.has_value()) return std::nullopt;
        JsonValue val;
        val.type = JsonValue::Type::String;
        val.str_val = std::move(*str);
        return val;
    }

    std::optional<JsonValue> ParseObject() {
        if (pos_ >= input_.size() || input_[pos_] != '{') return std::nullopt;
        ++pos_;

        JsonValue obj;
        obj.type = JsonValue::Type::Object;

        SkipWhitespace();
        if (pos_ < input_.size() && input_[pos_] == '}') {
            ++pos_;
            return obj;
        }

        while (pos_ < input_.size()) {
            SkipWhitespace();
            auto key = ParseString();
            if (!key.has_value()) return std::nullopt;

            SkipWhitespace();
            if (pos_ >= input_.size() || input_[pos_] != ':') return std::nullopt;
            ++pos_;

            auto val = ParseValue();
            if (!val.has_value()) return std::nullopt;

            obj.obj_val.emplace_back(std::move(*key), std::move(*val));

            SkipWhitespace();
            if (pos_ >= input_.size()) return std::nullopt;
            if (input_[pos_] == '}') {
                ++pos_;
                return obj;
            }
            if (input_[pos_] == ',') {
                ++pos_;
            } else {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseArray() {
        if (pos_ >= input_.size() || input_[pos_] != '[') return std::nullopt;
        ++pos_;

        JsonValue arr;
        arr.type = JsonValue::Type::Array;

        SkipWhitespace();
        if (pos_ < input_.size() && input_[pos_] == ']') {
            ++pos_;
            return arr;
        }

        while (pos_ < input_.size()) {
            auto val = ParseValue();
            if (!val.has_value()) return std::nullopt;
            arr.arr_val.push_back(std::move(*val));

            SkipWhitespace();
            if (pos_ >= input_.size()) return std::nullopt;
            if (input_[pos_] == ']') {
                ++pos_;
                return arr;
            }
            if (input_[pos_] == ',') {
                ++pos_;
            } else {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseBool() {
        if (input_.substr(pos_).starts_with("true")) {
            pos_ += 4;
            JsonValue val;
            val.type = JsonValue::Type::Bool;
            val.bool_val = true;
            return val;
        }
        if (input_.substr(pos_).starts_with("false")) {
            pos_ += 5;
            JsonValue val;
            val.type = JsonValue::Type::Bool;
            val.bool_val = false;
            return val;
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseNull() {
        if (input_.substr(pos_).starts_with("null")) {
            pos_ += 4;
            JsonValue val;
            val.type = JsonValue::Type::Null;
            return val;
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseNumber() {
        const std::size_t start = pos_;
        if (pos_ < input_.size() && input_[pos_] == '-') ++pos_;
        while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        if (pos_ < input_.size() && input_[pos_] == '.') {
            ++pos_;
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        }
        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) ++pos_;
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        }

        const auto num_str = input_.substr(start, pos_ - start);
        double d = 0.0;
        try {
            d = std::stod(std::string(num_str));
        } catch (...) {
            return std::nullopt;
        }

        JsonValue val;
        val.type = JsonValue::Type::Number;
        val.num_val = d;
        return val;
    }

    std::string_view input_;
    std::size_t pos_{0};
};

}  // namespace

std::optional<JsonValue> ParseJson(const std::string_view json) {
    JsonParser parser(json);
    return parser.Parse();
}

}  // namespace openbrowser::core::storage
