#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace openbrowser::core::storage {

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type{Type::Null};
    bool bool_val{false};
    double num_val{0.0};
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::vector<std::pair<std::string, JsonValue>> obj_val;

    [[nodiscard]] const JsonValue* Find(std::string_view key) const noexcept;
    [[nodiscard]] std::string GetString(std::string_view key, std::string default_val = "") const;
    [[nodiscard]] std::optional<std::string> GetOptionalString(std::string_view key) const;
    [[nodiscard]] bool GetBool(std::string_view key, bool default_val = false) const noexcept;
    [[nodiscard]] double GetDouble(std::string_view key, double default_val = 0.0) const noexcept;
    [[nodiscard]] std::int64_t GetInt64(std::string_view key, std::int64_t default_val = 0) const noexcept;
    [[nodiscard]] std::size_t GetSizeT(std::string_view key, std::size_t default_val = 0) const noexcept;
};

// Escapes special characters for a JSON string literal and appends to out, surrounded by quotes.
void EscapeJsonString(std::string_view str, std::string& out);

// Parses a UTF-8 JSON string into a JsonValue tree. Returns std::nullopt if malformed.
[[nodiscard]] std::optional<JsonValue> ParseJson(std::string_view json);

}  // namespace openbrowser::core::storage
