#include "core/sync/local_filesystem_sync.h"
#include "core/sync/sync_port.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestSyncEntityTypeConversions() {
    using namespace openbrowser::core;

    Require(SyncEntityTypeToString(SyncEntityType::Bookmark) == "Bookmark", "Bookmark string");
    Require(SyncEntityTypeToString(SyncEntityType::History) == "History", "History string");
    Require(SyncEntityTypeToString(SyncEntityType::WorkspaceConfig) == "WorkspaceConfig", "WorkspaceConfig string");
    Require(SyncEntityTypeToString(SyncEntityType::CapabilityRule) == "CapabilityRule", "CapabilityRule string");

    Require(StringToSyncEntityType("Bookmark") == SyncEntityType::Bookmark, "Bookmark parse");
    Require(StringToSyncEntityType("History") == SyncEntityType::History, "History parse");
    Require(StringToSyncEntityType("WorkspaceConfig") == SyncEntityType::WorkspaceConfig, "WorkspaceConfig parse");
    Require(StringToSyncEntityType("CapabilityRule") == SyncEntityType::CapabilityRule, "CapabilityRule parse");
}

void TestLocalFilesystemSyncPushAndPull() {
    using namespace openbrowser::core;

    const auto temp_dir = std::filesystem::temp_directory_path() / "openbrowser_sync_test";
    std::filesystem::remove_all(temp_dir);

    LocalFilesystemSyncProvider provider(temp_dir, "desktop-client-1");

    SyncRecord rec1;
    rec1.entity_type = SyncEntityType::Bookmark;
    rec1.record_id = "bm-42";
    rec1.version = 1;
    rec1.timestamp = 1000;
    rec1.payload_json = "{\"url\":\"https://example.com\",\"title\":\"Example\"}";

    SyncRecord rec2;
    rec2.entity_type = SyncEntityType::History;
    rec2.record_id = "hist-101";
    rec2.version = 1;
    rec2.timestamp = 2000;
    rec2.payload_json = "{\"url\":\"https://example.com/page\",\"visits\":3}";

    Require(provider.PushRecords({rec1, rec2}), "PushRecords succeeds");
    Require(provider.TotalStoredRecords() == 2, "Stored records count is 2");

    const auto manifest = provider.GetManifest();
    Require(manifest.device_id == "desktop-client-1", "Manifest device_id matches");
    Require(manifest.last_sync_timestamp == 2000, "Manifest last sync timestamp is 2000");

    // Pull all (since 0)
    const auto all_records = provider.PullRecords(0);
    Require(all_records.size() == 2, "Pull since 0 returns 2 records");

    // Incremental pull (since 1500) -> should only return rec2
    const auto newer_records = provider.PullRecords(1500);
    Require(newer_records.size() == 1, "Pull since 1500 returns 1 record");
    Require(newer_records[0].record_id == "hist-101", "Returned record is hist-101");

    // Conflict update: rec1 version 2
    SyncRecord rec1_v2 = rec1;
    rec1_v2.version = 2;
    rec1_v2.timestamp = 3000;
    rec1_v2.payload_json = "{\"url\":\"https://example.com\",\"title\":\"Updated Example\"}";

    Require(provider.PushRecords({rec1_v2}), "Push higher version succeeds");
    const auto updated_records = provider.PullRecords(2500);
    Require(updated_records.size() == 1, "Only updated record pulled");
    Require(updated_records[0].version == 2, "Version updated to 2");

    // Persistence reload into a new provider
    LocalFilesystemSyncProvider reader(temp_dir, "desktop-client-2");
    Require(reader.LoadFromDisk(), "LoadFromDisk succeeds");
    Require(reader.TotalStoredRecords() == 2, "Reloaded records count is 2");

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

}  // namespace

int main() {
    TestSyncEntityTypeConversions();
    TestLocalFilesystemSyncPushAndPull();

    if (failures != 0) {
        std::cerr << failures << " Sync test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser SyncPort & LocalFilesystemSync invariants: PASS\n";
    return 0;
}
