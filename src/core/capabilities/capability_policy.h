#pragma once

#include "core/tabs/tab.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace openbrowser::core {

enum class Capability {
    PageNetwork,
    UpdateCheck,
    FilterUpdate,
    Sync,
    Dns,
    Torrent,
    CrashReport,
    ExternalService,
    Camera,
    Microphone,
    Geolocation,
    Notifications,
    ClipboardRead,
    ClipboardWrite,
    PersistentStorage,
    ThirdPartyStorage,
    Automation,
    UserScript,
};

enum class CapabilityDecision {
    Allow,
    Deny,
    Ask,
};

struct CapabilityContext {
    std::optional<WorkspaceId> workspace_id;
    std::optional<std::string> origin;
    std::optional<std::string> session_id;
};

class CapabilityPolicy {
public:
    [[nodiscard]] static CapabilityPolicy CreateDefault();
    [[nodiscard]] static std::optional<std::string> ExtractOrigin(std::string_view url);
    [[nodiscard]] static std::optional<std::string> ExtractScheme(std::string_view url);

    void SetGlobal(Capability capability, CapabilityDecision decision);
    void SetWorkspace(const WorkspaceId& workspace_id, Capability capability, CapabilityDecision decision);
    void SetOrigin(const std::string& origin, Capability capability, CapabilityDecision decision);
    void SetSession(const std::string& session_id, Capability capability, CapabilityDecision decision);

    [[nodiscard]] std::map<Capability, CapabilityDecision> GetOriginRules(const std::string& origin) const;
    void ClearOriginRules(const std::string& origin);

    [[nodiscard]] CapabilityDecision Resolve(Capability capability, const CapabilityContext& context) const;

    [[nodiscard]] bool CanNavigate(const std::string& url, const CapabilityContext& context = {}) const;
    [[nodiscard]] bool CanLoadResource(
        const std::string& url,
        const std::string& initiator = {},
        const CapabilityContext& context = {}) const;

private:
    using RuleSet = std::map<Capability, CapabilityDecision>;

    [[nodiscard]] static std::optional<CapabilityDecision> FindDecision(
        const RuleSet& rules,
        Capability capability);

    RuleSet global_rules_;
    std::map<WorkspaceId, RuleSet> workspace_rules_;
    std::map<std::string, RuleSet> origin_rules_;
    std::map<std::string, RuleSet> session_rules_;
};

}  // namespace openbrowser::core
