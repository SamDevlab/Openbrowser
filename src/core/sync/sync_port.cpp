#include "core/sync/sync_port.h"

namespace openbrowser::core {

std::string SyncEntityTypeToString(const SyncEntityType type) {
    switch (type) {
        case SyncEntityType::Bookmark:
            return "Bookmark";
        case SyncEntityType::History:
            return "History";
        case SyncEntityType::WorkspaceConfig:
            return "WorkspaceConfig";
        case SyncEntityType::CapabilityRule:
            return "CapabilityRule";
    }
    return "Bookmark";
}

SyncEntityType StringToSyncEntityType(const std::string& str) {
    if (str == "History") {
        return SyncEntityType::History;
    }
    if (str == "WorkspaceConfig") {
        return SyncEntityType::WorkspaceConfig;
    }
    if (str == "CapabilityRule") {
        return SyncEntityType::CapabilityRule;
    }
    return SyncEntityType::Bookmark;
}

}  // namespace openbrowser::core
