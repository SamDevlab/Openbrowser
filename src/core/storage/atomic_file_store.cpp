#include "core/storage/atomic_file_store.h"

#include <fstream>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdio>
#endif

namespace openbrowser::core::storage {

namespace {

std::optional<std::string> ReadFileToString(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        return std::nullopt;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << stream.rdbuf();
    if (stream.bad()) {
        return std::nullopt;
    }
    return ss.str();
}

}  // namespace

AtomicWriteResult AtomicWriteFile(
    const std::filesystem::path& target,
    const std::string_view content,
    const bool keep_backup) {
    AtomicWriteResult result;

    if (target.empty()) {
        result.error_message = "Target path cannot be empty";
        return result;
    }

    std::error_code ec;
    const auto parent = target.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            result.error_message = "Failed to create directory: " + ec.message();
            return result;
        }
    }

    const auto tmp_path = std::filesystem::path(target.string() + ".tmp");
    const auto bak_path = std::filesystem::path(target.string() + ".bak");

    // 1. Write to tmp file, flush, and close stream
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            result.error_message = "Failed to open temporary file for writing";
            return result;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) {
            out.close();
            std::filesystem::remove(tmp_path, ec);
            result.error_message = "Failed writing data to temporary file";
            return result;
        }
        out.close();
    }

    const bool target_exists = std::filesystem::exists(target, ec);

#ifdef _WIN32
    if (!target_exists) {
        // Target does not exist yet: atomic move of tmp to target
        if (!MoveFileExW(tmp_path.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const DWORD err = GetLastError();
            std::filesystem::remove(tmp_path, ec);
            result.error_message = "MoveFileExW failed with code " + std::to_string(err);
            return result;
        }
        result.success = true;
        return result;
    }

    // Target already exists: atomically replace using ReplaceFileW
    const LPCWSTR backup_arg = keep_backup ? bak_path.c_str() : nullptr;
    if (ReplaceFileW(target.c_str(), tmp_path.c_str(), backup_arg, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
        result.success = true;
        result.backup_created = keep_backup;
        return result;
    }

    // Fallback if ReplaceFileW fails on certain file systems
    if (keep_backup) {
        CopyFileW(target.c_str(), bak_path.c_str(), FALSE);
        result.backup_created = true;
    }
    if (MoveFileExW(tmp_path.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        result.success = true;
        return result;
    }

    const DWORD fallback_err = GetLastError();
    std::filesystem::remove(tmp_path, ec);
    result.error_message = "Windows atomic replacement failed with code " + std::to_string(fallback_err);
    return result;

#else
    // POSIX platform
    if (target_exists && keep_backup) {
        std::filesystem::copy_file(
            target, bak_path,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            result.backup_created = true;
        }
    }

    // rename(2) is atomic and replaces destination in POSIX
    if (std::rename(tmp_path.c_str(), target.c_str()) == 0) {
        result.success = true;
        return result;
    }

    // Fallback in case of unexpected cross-device or permission issue
    std::filesystem::copy_file(
        tmp_path, target,
        std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::remove(tmp_path, ec);
    if (!ec) {
        result.success = true;
        return result;
    }

    result.error_message = "POSIX atomic replacement failed: " + ec.message();
    return result;
#endif
}

FileReadResult ReadFileWithBackupRecovery(
    const std::filesystem::path& target,
    const std::function<bool(std::string_view)>& validator) {
    FileReadResult result;

    if (target.empty()) {
        result.error_message = "Target path is empty";
        return result;
    }

    // 1. Try primary file
    auto primary_content = ReadFileToString(target);
    if (primary_content.has_value()) {
        const bool is_valid = (!validator || validator(*primary_content));
        if (is_valid) {
            result.success = true;
            result.source = ReadRecoverySource::Primary;
            result.content = std::move(*primary_content);
            return result;
        }
        result.error_message = "Primary file failed validation; attempting backup recovery";
    } else {
        result.error_message = "Primary file missing or unreadable; attempting backup recovery";
    }

    // 2. Try backup file (.bak)
    const auto bak_path = std::filesystem::path(target.string() + ".bak");
    auto backup_content = ReadFileToString(bak_path);
    if (backup_content.has_value()) {
        const bool is_valid = (!validator || validator(*backup_content));
        if (is_valid) {
            result.success = true;
            result.source = ReadRecoverySource::Backup;
            result.content = std::move(*backup_content);
            return result;
        }
        result.error_message = "Backup file also failed validation";
    } else {
        result.error_message = "Backup file is missing or unreadable";
    }

    result.success = false;
    result.source = ReadRecoverySource::None;
    return result;
}

}  // namespace openbrowser::core::storage
