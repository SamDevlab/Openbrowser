#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace openbrowser::core {

enum class SyncEntityType {
    Bookmark,
    History,
    WorkspaceConfig,
    CapabilityRule,
};

[[nodiscard]] std::string SyncEntityTypeToString(SyncEntityType type);
[[nodiscard]] SyncEntityType StringToSyncEntityType(const std::string& str);

struct SyncRecord {
    SyncEntityType entity_type{SyncEntityType::Bookmark};
    std::string record_id;
    std::uint64_t version{1};
    std::uint64_t timestamp{0};
    std::string payload_json;
    bool is_deleted{false};
};

struct SyncManifest {
    std::string device_id;
    std::string schema_version{"1.0"};
    std::uint64_t last_sync_timestamp{0};
    std::size_t total_records{0};
};

class SyncPort {
public:
    virtual ~SyncPort() = default;

    virtual bool PushRecords(const std::vector<SyncRecord>& records) = 0;
    virtual std::vector<SyncRecord> PullRecords(std::uint64_t since_timestamp) = 0;
    virtual SyncManifest GetManifest() = 0;
};

}  // namespace openbrowser::core
