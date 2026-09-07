#pragma once

#include "core/capabilities/capability_policy.h"
#include "core/tabs/tab.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openbrowser::core {

enum class PermissionResponse {
    Allow,
    Block,
    Dismiss,
};

struct PermissionPrompt {
    uint64_t prompt_id{0};
    TabId tab_id;
    std::string origin;
    std::vector<Capability> capabilities;
};

[[nodiscard]] inline bool ShouldRememberPermissionForOrigin(
    const bool requested_remember,
    const bool is_private_mode) noexcept {
    return requested_remember && !is_private_mode;
}

[[nodiscard]] inline std::string_view ToString(const Capability capability) noexcept {
    switch (capability) {
        case Capability::PageNetwork:
            return "Page Network";
        case Capability::UpdateCheck:
            return "Update Check";
        case Capability::FilterUpdate:
            return "Filter Update";
        case Capability::Sync:
            return "Sync";
        case Capability::Dns:
            return "DNS";
        case Capability::Torrent:
            return "Torrent";
        case Capability::CrashReport:
            return "Crash Report";
        case Capability::ExternalService:
            return "External Service";
        case Capability::Camera:
            return "Camera";
        case Capability::Microphone:
            return "Microphone";
        case Capability::Geolocation:
            return "Geolocation";
        case Capability::Notifications:
            return "Notifications";
        case Capability::ClipboardRead:
            return "Clipboard Read";
        case Capability::ClipboardWrite:
            return "Clipboard Write";
        case Capability::PersistentStorage:
            return "Persistent Storage";
        case Capability::ThirdPartyStorage:
            return "Third-Party Storage";
        case Capability::Automation:
            return "Automation";
        case Capability::UserScript:
            return "User Script";
    }
    return "Unknown";
}

[[nodiscard]] inline std::string_view ToString(const PermissionResponse response) noexcept {
    switch (response) {
        case PermissionResponse::Allow:
            return "Allow";
        case PermissionResponse::Block:
            return "Block";
        case PermissionResponse::Dismiss:
            return "Dismiss";
    }
    return "Unknown";
}

class PermissionPromptObserver {
public:
    virtual ~PermissionPromptObserver() = default;
    virtual void OnPermissionPromptRequested(const PermissionPrompt& prompt) = 0;
    virtual void OnPermissionPromptDismissed(uint64_t prompt_id) = 0;
};

}  // namespace openbrowser::core
