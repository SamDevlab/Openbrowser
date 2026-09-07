#pragma once

#include "core/settings/settings.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace openbrowser::core {

class SettingsManager {
public:
    SettingsManager() = default;
    explicit SettingsManager(BrowserSettings settings);

    [[nodiscard]] const BrowserSettings& Settings() const noexcept;
    void UpdateSettings(BrowserSettings settings);

    void SetSearchProvider(std::string name, std::string url_template);
    void SetRestoreSessionOnStartup(bool restore);
    void SetHomePageUrl(std::string url);
    void SetDownloadsDirectory(std::string dir);

    void SetAutoSavePath(std::filesystem::path path);
    [[nodiscard]] const std::filesystem::path& AutoSavePath() const noexcept;

    [[nodiscard]] std::string Serialize() const;
    bool Deserialize(std::string_view json);

    [[nodiscard]] bool SaveToFile(const std::filesystem::path& path) const;
    bool LoadFromFile(const std::filesystem::path& path);

private:
    void TriggerAutoSave() const;

    BrowserSettings settings_;
    std::filesystem::path auto_save_path_;
};

}  // namespace openbrowser::core
