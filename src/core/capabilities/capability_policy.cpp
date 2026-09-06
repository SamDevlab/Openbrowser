#include "core/capabilities/capability_policy.h"

namespace openbrowser::core {

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

std::optional<CapabilityDecision> CapabilityPolicy::FindDecision(
    const RuleSet& rules,
    const Capability capability) {
    const auto it = rules.find(capability);
    return it == rules.end() ? std::nullopt : std::optional<CapabilityDecision>{it->second};
}

}  // namespace openbrowser::core
