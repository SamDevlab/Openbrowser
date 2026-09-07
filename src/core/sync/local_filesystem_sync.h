#pragma once

#include "core/sync/sync_port.h"

#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace openbrowser::core {

class LocalFilesystemSyncProvider final : public SyncPort {
public:
    explicit LocalFilesystemSyncProvider(
        std::filesystem::path sync_directory,
        std::string device_id = "local-device");

    bool PushRecords(const std::vector<SyncRecord>& records) override;
    std::vector<SyncRecord> PullRecords(std::uint64_t since_timestamp) override;
    SyncManifest GetManifest() override;

    [[nodiscard]] const std::filesystem::path& SyncDirectory() const noexcept;
    [[nodiscard]] std::size_t TotalStoredRecords() const noexcept;

    bool SaveToDisk();
    bool LoadFromDisk();

private:
    std::filesystem::path sync_directory_;
    std::string device_id_;
    std::uint64_t last_sync_timestamp_{0};
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SyncRecord> records_;
};

}  // namespace openbrowser::core
