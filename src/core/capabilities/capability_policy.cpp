#include "core/capabilities/capability_policy.h"

#include <algorithm>
#include <cctype>

namespace openbrowser::core {

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
    origin_rules_[origin][capability] = decision;
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
    origin_rules_.erase(origin);
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
