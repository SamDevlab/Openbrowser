#include "core/capabilities/capability_policy.h"

#include "core/storage/atomic_file_store.h"
#include "core/storage/json_helper.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace openbrowser::core {
namespace {

constexpr std::string_view CapabilityKey(const Capability capability) noexcept {
    switch (capability) {
        case Capability::PageNetwork: return "PageNetwork";
        case Capability::UpdateCheck: return "UpdateCheck";
        case Capability::FilterUpdate: return "FilterUpdate";
        case Capability::Sync: return "Sync";
        case Capability::Dns: return "Dns";
        case Capability::Torrent: return "Torrent";
        case Capability::CrashReport: return "CrashReport";
        case Capability::ExternalService: return "ExternalService";
        case Capability::Camera: return "Camera";
        case Capability::Microphone: return "Microphone";
        case Capability::Geolocation: return "Geolocation";
        case Capability::Notifications: return "Notifications";
        case Capability::ClipboardRead: return "ClipboardRead";
        case Capability::ClipboardWrite: return "ClipboardWrite";
        case Capability::PersistentStorage: return "PersistentStorage";
        case Capability::ThirdPartyStorage: return "ThirdPartyStorage";
        case Capability::Automation: return "Automation";
        case Capability::UserScript: return "UserScript";
    }
    return "Unknown";
}

std::optional<Capability> CapabilityFromKey(const std::string_view key) {
    if (key == "PageNetwork") return Capability::PageNetwork;
    if (key == "UpdateCheck") return Capability::UpdateCheck;
    if (key == "FilterUpdate") return Capability::FilterUpdate;
    if (key == "Sync") return Capability::Sync;
    if (key == "Dns") return Capability::Dns;
    if (key == "Torrent") return Capability::Torrent;
    if (key == "CrashReport") return Capability::CrashReport;
    if (key == "ExternalService") return Capability::ExternalService;
    if (key == "Camera") return Capability::Camera;
    if (key == "Microphone") return Capability::Microphone;
    if (key == "Geolocation") return Capability::Geolocation;
    if (key == "Notifications") return Capability::Notifications;
    if (key == "ClipboardRead") return Capability::ClipboardRead;
    if (key == "ClipboardWrite") return Capability::ClipboardWrite;
    if (key == "PersistentStorage") return Capability::PersistentStorage;
    if (key == "ThirdPartyStorage") return Capability::ThirdPartyStorage;
    if (key == "Automation") return Capability::Automation;
    if (key == "UserScript") return Capability::UserScript;
    return std::nullopt;
}

constexpr std::string_view DecisionKey(const CapabilityDecision decision) noexcept {
    switch (decision) {
        case CapabilityDecision::Allow: return "Allow";
        case CapabilityDecision::Deny: return "Deny";
        case CapabilityDecision::Ask: return "Ask";
    }
    return "Deny";
}

std::optional<CapabilityDecision> DecisionFromKey(const std::string_view key) {
    if (key == "Allow") return CapabilityDecision::Allow;
    if (key == "Deny") return CapabilityDecision::Deny;
    if (key == "Ask") return CapabilityDecision::Ask;
    return std::nullopt;
}

bool ValidateOriginRulesDocument(const std::string_view content) {
    const auto root = storage::ParseJson(content);
    if (!root.has_value() || root->type != storage::JsonValue::Type::Object ||
        root->GetSizeT("schema_version", 0) != 1) {
        return false;
    }

    const auto* origins = root->Find("origin_rules");
    if (origins == nullptr || origins->type != storage::JsonValue::Type::Array) {
        return false;
    }

    for (const auto& origin_item : origins->arr_val) {
        if (origin_item.type != storage::JsonValue::Type::Object ||
            origin_item.GetString("origin").empty()) {
            return false;
        }
        const auto* rules = origin_item.Find("rules");
        if (rules == nullptr || rules->type != storage::JsonValue::Type::Array) {
            return false;
        }
        for (const auto& rule : rules->arr_val) {
            if (rule.type != storage::JsonValue::Type::Object ||
                !CapabilityFromKey(rule.GetString("capability")).has_value() ||
                !DecisionFromKey(rule.GetString("decision")).has_value()) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

CapabilityPolicy CapabilityPolicy::CreateDefault() {
    CapabilityPolicy policy;
    policy.SetGlobal(Capability::PageNetwork, CapabilityDecision::Allow);
    policy.SetGlobal(Capability::UpdateCheck, CapabilityDecision::Allow);
    policy.SetGlobal(Capability::FilterUpdate, CapabilityDecision::Allow);
    policy.SetGlobal(Capability::Dns, CapabilityDecision::Allow);
    policy.SetGlobal(Capability::PersistentStorage, CapabilityDecision::Allow);

    policy.SetGlobal(Capability::ThirdPartyStorage, CapabilityDecision::Deny);
    policy.SetGlobal(Capability::ExternalService, CapabilityDecision::Deny);
    policy.SetGlobal(Capability::Automation, CapabilityDecision::Deny);
    policy.SetGlobal(Capability::Torrent, CapabilityDecision::Deny);
    policy.SetGlobal(Capability::CrashReport, CapabilityDecision::Deny);

    policy.SetGlobal(Capability::Camera, CapabilityDecision::Ask);
    policy.SetGlobal(Capability::Microphone, CapabilityDecision::Ask);
    policy.SetGlobal(Capability::Geolocation, CapabilityDecision::Ask);
    policy.SetGlobal(Capability::Notifications, CapabilityDecision::Ask);
    policy.SetGlobal(Capability::ClipboardRead, CapabilityDecision::Ask);
    policy.SetGlobal(Capability::ClipboardWrite, CapabilityDecision::Ask);

    return policy;
}

std::optional<std::string> CapabilityPolicy::ExtractScheme(const std::string_view url) {
    const auto scheme_end = url.find("://");
    if (scheme_end != std::string_view::npos && scheme_end > 0) {
        std::string scheme(url.substr(0, scheme_end));
        for (char& c : scheme) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return scheme;
    }

    const auto colon = url.find(':');
    if (colon != std::string_view::npos && colon > 0) {
        std::string scheme(url.substr(0, colon));
        for (char& c : scheme) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return scheme;
    }

    return std::nullopt;
}

std::optional<std::string> CapabilityPolicy::ExtractOrigin(const std::string_view url) {
    const auto scheme_end = url.find("://");
    if (scheme_end == std::string_view::npos || scheme_end == 0) {
        return std::nullopt;
    }

    const auto host_start = scheme_end + 3;
    const auto path_start = url.find_first_of("/?#", host_start);
    const auto origin_sv = (path_start == std::string_view::npos)
        ? url
        : url.substr(0, path_start);

    std::string origin(origin_sv);
    for (char& c : origin) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return origin;
}

void CapabilityPolicy::SetGlobal(const Capability capability, const CapabilityDecision decision) {
    global_rules_[capability] = decision;
}

void CapabilityPolicy::SetWorkspace(
    const WorkspaceId& workspace_id,
    const Capability capability,
    const CapabilityDecision decision) {
    workspace_rules_[workspace_id][capability] = decision;
}

void CapabilityPolicy::SetOrigin(
    const std::string& origin,
    const Capability capability,
    const CapabilityDecision decision) {
    if (origin.empty()) {
        return;
    }
    origin_rules_[origin][capability] = decision;
    TriggerAutoSave();
}

void CapabilityPolicy::SetSession(
    const std::string& session_id,
    const Capability capability,
    const CapabilityDecision decision) {
    session_rules_[session_id][capability] = decision;
}

std::map<Capability, CapabilityDecision> CapabilityPolicy::GetOriginRules(
    const std::string& origin) const {
    const auto it = origin_rules_.find(origin);
    if (it != origin_rules_.end()) {
        return it->second;
    }
    return {};
}

void CapabilityPolicy::ClearOriginRules(const std::string& origin) {
    if (origin_rules_.erase(origin) > 0) {
        TriggerAutoSave();
    }
}

void CapabilityPolicy::SetAutoSavePath(std::filesystem::path path) {
    auto_save_path_ = std::move(path);
}

const std::filesystem::path& CapabilityPolicy::AutoSavePath() const noexcept {
    return auto_save_path_;
}

void CapabilityPolicy::TriggerAutoSave() const {
    if (!auto_save_path_.empty()) {
        static_cast<void>(SaveOriginRulesToFile(auto_save_path_));
    }
}

bool CapabilityPolicy::SaveOriginRulesToFile(const std::filesystem::path& path) const {
    if (path.empty()) {
        return false;
    }

    std::string out;
    out += "{\n  \"schema_version\": 1,\n  \"origin_rules\": [\n";
    std::size_t origin_index = 0;
    for (const auto& [origin, rules] : origin_rules_) {
        out += "    {\n      \"origin\": ";
        storage::EscapeJsonString(origin, out);
        out += ",\n      \"rules\": [\n";

        std::size_t rule_index = 0;
        for (const auto& [capability, decision] : rules) {
            out += "        {\"capability\": ";
            storage::EscapeJsonString(CapabilityKey(capability), out);
            out += ", \"decision\": ";
            storage::EscapeJsonString(DecisionKey(decision), out);
            out += "}";
            if (++rule_index < rules.size()) {
                out += ",";
            }
            out += "\n";
        }

        out += "      ]\n    }";
        if (++origin_index < origin_rules_.size()) {
            out += ",";
        }
        out += "\n";
    }
    out += "  ]\n}\n";

    return storage::AtomicWriteFile(path, out, true).success;
}

bool CapabilityPolicy::LoadOriginRulesFromFile(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    const auto result = storage::ReadFileWithBackupRecovery(path, ValidateOriginRulesDocument);
    if (!result.success) {
        return false;
    }

    const auto root = storage::ParseJson(result.content);
    if (!root.has_value()) {
        return false;
    }

    std::map<std::string, RuleSet> loaded;
    const auto* origins = root->Find("origin_rules");
    for (const auto& origin_item : origins->arr_val) {
        const auto origin = origin_item.GetString("origin");
        RuleSet rules;
        const auto* rule_array = origin_item.Find("rules");
        for (const auto& rule : rule_array->arr_val) {
            const auto capability = CapabilityFromKey(rule.GetString("capability"));
            const auto decision = DecisionFromKey(rule.GetString("decision"));
            if (capability.has_value() && decision.has_value()) {
                rules[*capability] = *decision;
            }
        }
        loaded[origin] = std::move(rules);
    }

    origin_rules_ = std::move(loaded);
    return true;
}

CapabilityDecision CapabilityPolicy::Resolve(
    const Capability capability,
    const CapabilityContext& context) const {
    if (context.session_id.has_value()) {
        const auto scope = session_rules_.find(*context.session_id);
        if (scope != session_rules_.end()) {
            if (const auto decision = FindDecision(scope->second, capability); decision.has_value()) {
                return *decision;
            }
        }
    }

    if (context.origin.has_value()) {
        const auto scope = origin_rules_.find(*context.origin);
        if (scope != origin_rules_.end()) {
            if (const auto decision = FindDecision(scope->second, capability); decision.has_value()) {
                return *decision;
            }
        }
    }

    if (context.workspace_id.has_value()) {
        const auto scope = workspace_rules_.find(*context.workspace_id);
        if (scope != workspace_rules_.end()) {
            if (const auto decision = FindDecision(scope->second, capability); decision.has_value()) {
                return *decision;
            }
        }
    }

    if (const auto decision = FindDecision(global_rules_, capability); decision.has_value()) {
        return *decision;
    }

    return CapabilityDecision::Deny;
}

bool CapabilityPolicy::CanNavigate(
    const std::string& url,
    const CapabilityContext& context) const {
    if (url.empty()) {
        return false;
    }

    const auto scheme = ExtractScheme(url);
    if (!scheme.has_value()) {
        return false;
    }

    if (*scheme == "javascript" || *scheme == "vbscript") {
        return false;
    }

    if (*scheme == "about") {
        return true;
    }

    CapabilityContext effective_context = context;
    if (!effective_context.origin.has_value()) {
        effective_context.origin = ExtractOrigin(url);
    }

    const auto decision = Resolve(Capability::PageNetwork, effective_context);
    return decision == CapabilityDecision::Allow;
}

bool CapabilityPolicy::CanLoadResource(
    const std::string& url,
    const std::string& initiator,
    const CapabilityContext& context) const {
    (void)initiator;

    if (url.empty()) {
        return false;
    }

    const auto scheme = ExtractScheme(url);
    if (!scheme.has_value()) {
        return false;
    }

    if (*scheme == "javascript" || *scheme == "vbscript") {
        return false;
    }

    if (*scheme == "data" || *scheme == "blob" || *scheme == "about") {
        return true;
    }

    CapabilityContext effective_context = context;
    const auto resource_origin = ExtractOrigin(url);
    if (resource_origin.has_value()) {
        effective_context.origin = resource_origin;
    }

    const auto decision = Resolve(Capability::PageNetwork, effective_context);
    return decision == CapabilityDecision::Allow;
}

std::optional<CapabilityDecision> CapabilityPolicy::FindDecision(
    const RuleSet& rules,
    const Capability capability) {
    const auto it = rules.find(capability);
    return it == rules.end() ? std::nullopt : std::optional<CapabilityDecision>{it->second};
}

}  // namespace openbrowser::core
