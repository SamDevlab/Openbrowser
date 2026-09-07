#include "core/session/session_persistence.h"

#include "core/session/focus_session_controller.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

namespace openbrowser::core {
namespace {

void EscapeString(const std::string_view str, std::string& out) {
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
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    out += '"';
}

std::string LifecycleToString(const TabLifecycle lifecycle) {
    switch (lifecycle) {
        case TabLifecycle::Active: return "Active";
        case TabLifecycle::Background: return "Background";
        case TabLifecycle::Suspended: return "Suspended";
        case TabLifecycle::Discarded: return "Discarded";
    }
    return "Background";
}

TabLifecycle StringToLifecycle(const std::string_view str) {
    if (str == "Active") return TabLifecycle::Active;
    if (str == "Suspended") return TabLifecycle::Suspended;
    if (str == "Discarded") return TabLifecycle::Discarded;
    return TabLifecycle::Background;
}

std::string FocusStateToString(const FocusState state) {
    switch (state) {
        case FocusState::Now: return "Now";
        case FocusState::Next: return "Next";
        case FocusState::Later: return "Later";
        case FocusState::Paused: return "Paused";
    }
    return "Next";
}

FocusState StringToFocusState(const std::string_view str) {
    if (str == "Now") return FocusState::Now;
    if (str == "Later") return FocusState::Later;
    if (str == "Paused") return FocusState::Paused;
    return FocusState::Next;
}

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type{Type::Null};
    bool bool_val{false};
    double num_val{0.0};
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::vector<std::pair<std::string, JsonValue>> obj_val;

    [[nodiscard]] const JsonValue* Find(const std::string_view key) const {
        if (type != Type::Object) return nullptr;
        for (const auto& [k, v] : obj_val) {
            if (k == key) return &v;
        }
        return nullptr;
    }

    [[nodiscard]] std::string GetString(const std::string_view key, std::string def = "") const {
        if (const auto* v = Find(key); v && v->type == Type::String) return v->str_val;
        return def;
    }

    [[nodiscard]] std::optional<std::string> GetOptionalString(const std::string_view key) const {
        if (const auto* v = Find(key); v && v->type == Type::String) return v->str_val;
        return std::nullopt;
    }

    [[nodiscard]] bool GetBool(const std::string_view key, const bool def = false) const {
        if (const auto* v = Find(key); v && v->type == Type::Bool) return v->bool_val;
        return def;
    }

    [[nodiscard]] std::size_t GetSizeT(const std::string_view key, const std::size_t def = 0) const {
        if (const auto* v = Find(key); v && v->type == Type::Number && v->num_val >= 0.0) {
            return static_cast<std::size_t>(v->num_val);
        }
        return def;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string_view input) : input_(input) {}

    std::optional<JsonValue> Parse() {
        SkipWhitespace();
        if (pos_ >= input_.size()) return std::nullopt;
        auto val = ParseValue();
        SkipWhitespace();
        return val;
    }

private:
    void SkipWhitespace() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    std::optional<JsonValue> ParseValue() {
        SkipWhitespace();
        if (pos_ >= input_.size()) return std::nullopt;

        const char c = input_[pos_];
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return ParseStringValue();
        if (c == 't' || c == 'f') return ParseBool();
        if (c == 'n') return ParseNull();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber();

        return std::nullopt;
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
            if (pos_ >= input_.size() || input_[pos_] != '"') return std::nullopt;

            auto key = ParseRawString();
            if (!key.has_value()) return std::nullopt;

            SkipWhitespace();
            if (pos_ >= input_.size() || input_[pos_] != ':') return std::nullopt;
            ++pos_;

            auto val = ParseValue();
            if (!val.has_value()) return std::nullopt;

            obj.obj_val.emplace_back(std::move(*key), std::move(*val));

            SkipWhitespace();
            if (pos_ < input_.size() && input_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < input_.size() && input_[pos_] == '}') {
                ++pos_;
                return obj;
            }
            break;
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
            if (pos_ < input_.size() && input_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < input_.size() && input_[pos_] == ']') {
                ++pos_;
                return arr;
            }
            break;
        }

        return std::nullopt;
    }

    std::optional<std::string> ParseRawString() {
        if (pos_ >= input_.size() || input_[pos_] != '"') return std::nullopt;
        ++pos_;

        std::string result;
        while (pos_ < input_.size()) {
            const char c = input_[pos_++];
            if (c == '"') {
                return result;
            }
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
                    default: result += esc; break;
                }
            } else {
                result += c;
            }
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseStringValue() {
        auto str = ParseRawString();
        if (!str.has_value()) return std::nullopt;
        JsonValue val;
        val.type = JsonValue::Type::String;
        val.str_val = std::move(*str);
        return val;
    }

    std::optional<JsonValue> ParseBool() {
        if (pos_ + 4 <= input_.size() && input_.substr(pos_, 4) == "true") {
            pos_ += 4;
            JsonValue val;
            val.type = JsonValue::Type::Bool;
            val.bool_val = true;
            return val;
        }
        if (pos_ + 5 <= input_.size() && input_.substr(pos_, 5) == "false") {
            pos_ += 5;
            JsonValue val;
            val.type = JsonValue::Type::Bool;
            val.bool_val = false;
            return val;
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseNull() {
        if (pos_ + 4 <= input_.size() && input_.substr(pos_, 4) == "null") {
            pos_ += 4;
            JsonValue val;
            val.type = JsonValue::Type::Null;
            return val;
        }
        return std::nullopt;
    }

    std::optional<JsonValue> ParseNumber() {
        const std::size_t start = pos_;
        if (pos_ < input_.size() && input_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < input_.size() && (std::isdigit(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '.')) {
            ++pos_;
        }

        if (pos_ == start) {
            return std::nullopt;
        }

        const auto num_str = std::string(input_.substr(start, pos_ - start));
        char* end = nullptr;
        const double d = std::strtod(num_str.c_str(), &end);
        if (end == num_str.c_str()) {
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

SessionSnapshot SessionPersistence::CaptureSnapshot(
    const BrowserSession& session,
    const FocusQueue& queue,
    const bool clean_shutdown) {
    SessionSnapshot snapshot;
    snapshot.schema_version = 1;
    snapshot.clean_shutdown = clean_shutdown;

    for (const auto& tab : session.Tabs()) {
        if (tab.is_ephemeral) {
            continue;
        }
        snapshot.tabs.push_back({
            .id = tab.id,
            .url = tab.url,
            .title = tab.title,
            .workspace_id = tab.workspace_id,
            .lifecycle = tab.lifecycle,
        });
    }

    if (session.ActiveTabId().has_value()) {
        const auto* active_tab = session.FindTab(*session.ActiveTabId());
        if (active_tab != nullptr && !active_tab->is_ephemeral) {
            snapshot.active_tab_id = session.ActiveTabId();
        } else if (!snapshot.tabs.empty()) {
            snapshot.active_tab_id = snapshot.tabs.front().id;
        } else {
            snapshot.active_tab_id = std::nullopt;
        }
    }

    for (const auto& item : queue.Items()) {
        if (item.tab_id.has_value()) {
            const auto* tab = session.FindTab(*item.tab_id);
            if (tab != nullptr && tab->is_ephemeral) {
                continue;
            }
        }
        snapshot.focus_items.push_back({
            .id = item.id,
            .url = item.url,
            .tab_id = item.tab_id,
            .workspace_id = item.workspace_id,
            .state = item.state,
        });
    }

    return snapshot;
}

bool SessionPersistence::RestoreSession(
    BrowserSession& session,
    FocusQueue& queue,
    const SessionSnapshot& snapshot) {
    // 1. Restore focus items
    for (const auto& item : snapshot.focus_items) {
        if (!queue.Contains(item.id)) {
            static_cast<void>(queue.Enqueue({
                .id = item.id,
                .url = item.url,
                .tab_id = item.tab_id,
                .workspace_id = item.workspace_id,
                .state = item.state,
            }));
        }
    }

    // 2. Restore tabs
    if (snapshot.tabs.empty()) {
        return true;
    }

    const std::string* target_active_id = nullptr;
    if (snapshot.active_tab_id.has_value()) {
        for (const auto& tab : snapshot.tabs) {
            if (tab.id == *snapshot.active_tab_id) {
                target_active_id = &tab.id;
                break;
            }
        }
    }
    if (target_active_id == nullptr && !snapshot.tabs.empty()) {
        target_active_id = &snapshot.tabs.front().id;
    }

    for (const auto& tab : snapshot.tabs) {
        if (session.FindTab(tab.id) != nullptr) {
            continue;
        }

        const bool is_active = (target_active_id != nullptr && tab.id == *target_active_id);
        Tab t;
        t.id = tab.id;
        t.url = tab.url;
        t.title = tab.title.empty() ? tab.url : tab.title;
        t.lifecycle = is_active ? TabLifecycle::Active : TabLifecycle::Background;
        t.workspace_id = tab.workspace_id;

        static_cast<void>(session.OpenTab(std::move(t), is_active));
    }

    if (target_active_id != nullptr) {
        static_cast<void>(session.ActivateTab(*target_active_id));
    }

    // 3. Synchronize tab closures with focus queue to ensure tab_id consistency
    FocusSessionController::SynchronizeTabClosures(session, queue);

    return true;
}

std::string SessionPersistence::Serialize(const SessionSnapshot& snapshot) {
    std::string out;
    out.reserve(1024);

    out += "{\n";
    out += "  \"schema_version\": " + std::to_string(snapshot.schema_version) + ",\n";
    out += "  \"clean_shutdown\": " + std::string(snapshot.clean_shutdown ? "true" : "false") + ",\n";

    out += "  \"active_tab_id\": ";
    if (snapshot.active_tab_id.has_value()) {
        EscapeString(*snapshot.active_tab_id, out);
    } else {
        out += "null";
    }
    out += ",\n";

    // Tabs
    out += "  \"tabs\": [\n";
    for (std::size_t i = 0; i < snapshot.tabs.size(); ++i) {
        const auto& tab = snapshot.tabs[i];
        out += "    {\n";
        out += "      \"id\": "; EscapeString(tab.id, out); out += ",\n";
        out += "      \"url\": "; EscapeString(tab.url, out); out += ",\n";
        out += "      \"title\": "; EscapeString(tab.title, out); out += ",\n";
        out += "      \"workspace_id\": ";
        if (tab.workspace_id.has_value()) {
            EscapeString(*tab.workspace_id, out);
        } else {
            out += "null";
        }
        out += ",\n";
        out += "      \"lifecycle\": "; EscapeString(LifecycleToString(tab.lifecycle), out); out += "\n";
        out += "    }";
        if (i + 1 < snapshot.tabs.size()) {
            out += ",";
        }
        out += "\n";
    }
    out += "  ],\n";

    // Focus items
    out += "  \"focus_items\": [\n";
    for (std::size_t i = 0; i < snapshot.focus_items.size(); ++i) {
        const auto& item = snapshot.focus_items[i];
        out += "    {\n";
        out += "      \"id\": "; EscapeString(item.id, out); out += ",\n";
        out += "      \"url\": "; EscapeString(item.url, out); out += ",\n";
        out += "      \"tab_id\": ";
        if (item.tab_id.has_value()) {
            EscapeString(*item.tab_id, out);
        } else {
            out += "null";
        }
        out += ",\n";
        out += "      \"workspace_id\": ";
        if (item.workspace_id.has_value()) {
            EscapeString(*item.workspace_id, out);
        } else {
            out += "null";
        }
        out += ",\n";
        out += "      \"state\": "; EscapeString(FocusStateToString(item.state), out); out += "\n";
        out += "    }";
        if (i + 1 < snapshot.focus_items.size()) {
            out += ",";
        }
        out += "\n";
    }
    out += "  ]\n";
    out += "}\n";

    return out;
}

std::optional<SessionSnapshot> SessionPersistence::Deserialize(const std::string_view json) {
    JsonParser parser(json);
    auto root = parser.Parse();
    if (!root.has_value() || root->type != JsonValue::Type::Object) {
        return std::nullopt;
    }

    SessionSnapshot snapshot;
    snapshot.schema_version = root->GetSizeT("schema_version", 1);
    snapshot.clean_shutdown = root->GetBool("clean_shutdown", false);
    snapshot.active_tab_id = root->GetOptionalString("active_tab_id");

    if (const auto* tabs_val = root->Find("tabs"); tabs_val && tabs_val->type == JsonValue::Type::Array) {
        for (const auto& item : tabs_val->arr_val) {
            if (item.type != JsonValue::Type::Object) continue;
            SessionTabRecord record;
            record.id = item.GetString("id");
            record.url = item.GetString("url");
            record.title = item.GetString("title");
            record.workspace_id = item.GetOptionalString("workspace_id");
            record.lifecycle = StringToLifecycle(item.GetString("lifecycle"));
            if (!record.id.empty()) {
                snapshot.tabs.push_back(std::move(record));
            }
        }
    }

    if (const auto* focus_val = root->Find("focus_items"); focus_val && focus_val->type == JsonValue::Type::Array) {
        for (const auto& item : focus_val->arr_val) {
            if (item.type != JsonValue::Type::Object) continue;
            FocusItemRecord record;
            record.id = item.GetString("id");
            record.url = item.GetString("url");
            record.tab_id = item.GetOptionalString("tab_id");
            record.workspace_id = item.GetOptionalString("workspace_id");
            record.state = StringToFocusState(item.GetString("state"));
            if (!record.id.empty()) {
                snapshot.focus_items.push_back(std::move(record));
            }
        }
    }

    return snapshot;
}

bool SessionPersistence::SaveToFile(
    const std::filesystem::path& file_path,
    const SessionSnapshot& snapshot) {
    std::error_code ec;
    const auto parent = file_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    const auto json = Serialize(snapshot);
    const auto temp_path = file_path.string() + ".tmp";

    {
        std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }
        file << json;
        file.flush();
        if (!file.good()) {
            return false;
        }
    }

    if (std::filesystem::exists(file_path, ec)) {
        std::filesystem::remove(file_path, ec);
    }

    std::filesystem::rename(temp_path, file_path, ec);
    if (ec) {
        std::filesystem::copy_file(
            temp_path, file_path,
            std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(temp_path, ec);
        if (ec) {
            return false;
        }
    }

    return true;
}

std::optional<SessionSnapshot> SessionPersistence::LoadFromFile(
    const std::filesystem::path& file_path) {
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec) || !std::filesystem::is_regular_file(file_path, ec)) {
        return std::nullopt;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return Deserialize(buffer.str());
}

bool SessionPersistence::WasLastShutdownClean(
    const std::filesystem::path& file_path) {
    const auto snapshot = LoadFromFile(file_path);
    if (!snapshot.has_value()) {
        return true;
    }
    return snapshot->clean_shutdown;
}

}  // namespace openbrowser::core
