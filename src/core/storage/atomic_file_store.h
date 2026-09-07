#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace openbrowser::core::storage {

struct AtomicWriteResult {
    bool success{false};
    std::string error_message;
    bool backup_created{false};
};

enum class ReadRecoverySource {
    None,
    Primary,
    Backup
};

struct FileReadResult {
    bool success{false};
    ReadRecoverySource source{ReadRecoverySource::None};
    std::string content;
    std::string error_message;
};

// Atomically writes content to target.
// 1. Writes to target.tmp, flushes and closes stream.
// 2. If target exists and keep_backup is true: preserves previous valid target as target.bak.
// 3. Atomically replaces target with target.tmp using platform-native atomic operations
//    (ReplaceFileW / MoveFileExW on Windows, rename on POSIX).
// 4. Cleans up temporary file on failure.
[[nodiscard]] AtomicWriteResult AtomicWriteFile(
    const std::filesystem::path& target,
    std::string_view content,
    bool keep_backup = true);

// Reads file with automatic backup recovery:
// 1. Tries target. If present and validator(content) is true (or validator is null),
//    returns {success: true, source: Primary, content: ...}.
// 2. If target is missing, truncated, or fails validator, tries target.bak.
//    If target.bak is present and valid, returns {success: true, source: Backup, content: ...}.
// 3. If both fail, returns {success: false, source: None, content: ""}.
[[nodiscard]] FileReadResult ReadFileWithBackupRecovery(
    const std::filesystem::path& target,
    const std::function<bool(std::string_view)>& validator = nullptr);

}  // namespace openbrowser::core::storage
