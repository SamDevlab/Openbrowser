#include "core/sync/local_filesystem_sync.h"

#include <algorithm>
#include <cstdio>
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
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    out += '"';
}

}  // namespace

LocalFilesystemSyncProvider::LocalFilesystemSyncProvider(
    std::filesystem::path sync_directory,
    std::string device_id)
    : sync_directory_(std::move(sync_directory)),
      device_id_(std::move(device_id)) {}

bool LocalFilesystemSyncProvider::PushRecords(const std::vector<SyncRecord>& records) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& rec : records) {
        if (rec.record_id.empty()) {
            continue;
        }

        const auto it = records_.find(rec.record_id);
        if (it == records_.end()) {
            records_[rec.record_id] = rec;
        } else {
            // Version conflict resolution: higher version wins, or later timestamp wins
            if (rec.version > it->second.version ||
                (rec.version == it->second.version && rec.timestamp >= it->second.timestamp)) {
                it->second = rec;
            }
        }

        if (rec.timestamp > last_sync_timestamp_) {
            last_sync_timestamp_ = rec.timestamp;
        }
    }

    return SaveToDisk();
}

std::vector<SyncRecord> LocalFilesystemSyncProvider::PullRecords(const std::uint64_t since_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    LoadFromDisk();

    std::vector<SyncRecord> result;
    for (const auto& [id, rec] : records_) {
        if (rec.timestamp > since_timestamp) {
            result.push_back(rec);
        }
    }

    std::sort(result.begin(), result.end(), [](const SyncRecord& a, const SyncRecord& b) {
        return a.timestamp < b.timestamp;
    });

    return result;
}

SyncManifest LocalFilesystemSyncProvider::GetManifest() {
    std::lock_guard<std::mutex> lock(mutex_);
    LoadFromDisk();

    SyncManifest manifest;
    manifest.device_id = device_id_;
    manifest.schema_version = "1.0";
    manifest.last_sync_timestamp = last_sync_timestamp_;
    manifest.total_records = records_.size();
    return manifest;
}

const std::filesystem::path& LocalFilesystemSyncProvider::SyncDirectory() const noexcept {
    return sync_directory_;
}

std::size_t LocalFilesystemSyncProvider::TotalStoredRecords() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_.size();
}

bool LocalFilesystemSyncProvider::SaveToDisk() {
    if (sync_directory_.empty()) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(sync_directory_, ec);
    if (ec) {
        return false;
    }

    const auto file_path = sync_directory_ / "sync_store.json";
    const auto temp_path = sync_directory_ / "sync_store.json.tmp";

    std::string json = "{\n  \"device_id\": ";
    EscapeString(device_id_, json);
    json += ",\n  \"last_sync_timestamp\": " + std::to_string(last_sync_timestamp_) + ",\n";
    json += "  \"records\": [\n";

    std::size_t count = 0;
    for (const auto& [id, rec] : records_) {
        if (count > 0) {
            json += ",\n";
        }
        json += "    {\n";
        json += "      \"entity_type\": ";
        EscapeString(SyncEntityTypeToString(rec.entity_type), json);
        json += ",\n      \"record_id\": ";
        EscapeString(rec.record_id, json);
        json += ",\n      \"version\": " + std::to_string(rec.version);
        json += ",\n      \"timestamp\": " + std::to_string(rec.timestamp);
        json += ",\n      \"is_deleted\": " + std::string(rec.is_deleted ? "true" : "false");
        json += ",\n      \"payload_json\": ";
        EscapeString(rec.payload_json, json);
        json += "\n    }";
        ++count;
    }

    json += "\n  ]\n}\n";

    {
        std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
    }

    std::filesystem::rename(temp_path, file_path, ec);
    return !ec;
}

bool LocalFilesystemSyncProvider::LoadFromDisk() {
    if (sync_directory_.empty()) {
        return true;
    }

    const auto file_path = sync_directory_ / "sync_store.json";
    if (!std::filesystem::exists(file_path)) {
        return true;
    }

    std::ifstream in(file_path);
    if (!in) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    // Simple robust record extractor from serialized JSON format
    std::size_t pos = 0;
    while ((pos = content.find("\"record_id\":", pos)) != std::string::npos) {
        const auto id_start = content.find('"', pos + 12);
        if (id_start == std::string::npos) break;
        const auto id_end = content.find('"', id_start + 1);
        if (id_end == std::string::npos) break;
        const std::string rec_id = content.substr(id_start + 1, id_end - id_start - 1);

        SyncRecord rec;
        rec.record_id = rec_id;

        // Find surrounding record block bounds
        const auto block_start = content.rfind('{', pos);
        const auto block_end = content.find('}', pos);
        if (block_start != std::string::npos && block_end != std::string::npos) {
            const std::string block = content.substr(block_start, block_end - block_start + 1);

            const auto type_pos = block.find("\"entity_type\":");
            if (type_pos != std::string::npos) {
                const auto s = block.find('"', type_pos + 14);
                const auto e = block.find('"', s + 1);
                if (s != std::string::npos && e != std::string::npos) {
                    rec.entity_type = StringToSyncEntityType(block.substr(s + 1, e - s - 1));
                }
            }

            const auto ver_pos = block.find("\"version\":");
            if (ver_pos != std::string::npos) {
                rec.version = std::stoull(block.substr(ver_pos + 10));
            }

            const auto time_pos = block.find("\"timestamp\":");
            if (time_pos != std::string::npos) {
                rec.timestamp = std::stoull(block.substr(time_pos + 12));
                if (rec.timestamp > last_sync_timestamp_) {
                    last_sync_timestamp_ = rec.timestamp;
                }
            }

            const auto del_pos = block.find("\"is_deleted\":");
            if (del_pos != std::string::npos) {
                rec.is_deleted = block.substr(del_pos + 13, 4) == "true";
            }

            const auto pay_pos = block.find("\"payload_json\":");
            if (pay_pos != std::string::npos) {
                const auto s = block.find('"', pay_pos + 15);
                const auto e = block.rfind('"');
                if (s != std::string::npos && e != std::string::npos && e > s) {
                    rec.payload_json = block.substr(s + 1, e - s - 1);
                }
            }
        }

        records_[rec_id] = rec;
        pos = id_end + 1;
    }

    return true;
}

}  // namespace openbrowser::core
