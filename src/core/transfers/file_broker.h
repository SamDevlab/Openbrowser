#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace openbrowser::core {

enum class FileRiskLevel {
    Safe,
    CautionExecutable,
    DangerousScript,
};

class FileBroker {
public:
    explicit FileBroker(std::filesystem::path allowed_directory = {});

    void SetAllowedDirectory(std::filesystem::path dir);
    [[nodiscard]] const std::filesystem::path& AllowedDirectory() const noexcept;

    [[nodiscard]] bool IsPathContained(const std::filesystem::path& path) const;

    [[nodiscard]] static std::string SanitizeFilename(std::string_view raw_filename);

    [[nodiscard]] static FileRiskLevel AssessRisk(const std::filesystem::path& path);

    [[nodiscard]] std::filesystem::path ResolveCollision(
        const std::string& sanitized_filename,
        const std::filesystem::path& base_dir = {}) const;

private:
    std::filesystem::path allowed_directory_;
};

}  // namespace openbrowser::core
