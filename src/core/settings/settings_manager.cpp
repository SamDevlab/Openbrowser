#include "core/settings/settings_manager.h"

#include "core/storage/atomic_file_store.h"
#include "core/storage/json_helper.h"

#include <utility>

namespace openbrowser::core {

SettingsManager::SettingsManager(BrowserSettings settings)
    : settings_(std::move(settings)) {}

const BrowserSettings& SettingsManager::Settings() const noexcept {
    return settings_;
}

void SettingsManager::UpdateSettings(BrowserSettings settings) {
    settings_ = std::move(settings);
    TriggerAutoSave();
}

void SettingsManager::SetSearchProvider(std::string name, std::string url_template) {
    if (!url_template.empty()) {
        settings_.search_provider_name = std::move(name);
        settings_.search_url_template = std::move(url_template);
        TriggerAutoSave();
    }
}

void SettingsManager::SetRestoreSessionOnStartup(const bool restore) {
    settings_.restore_session_on_startup = restore;
    TriggerAutoSave();
}

void SettingsManager::SetHomePageUrl(std::string url) {
    if (!url.empty()) {
        settings_.home_page_url = std::move(url);
        TriggerAutoSave();
    }
}

void SettingsManager::SetDownloadsDirectory(std::string dir) {
    settings_.downloads_directory = std::move(dir);
    TriggerAutoSave();
}

void SettingsManager::SetAutoSavePath(std::filesystem::path path) {
    auto_save_path_ = std::move(path);
}

const std::filesystem::path& SettingsManager::AutoSavePath() const noexcept {
    return auto_save_path_;
}

void SettingsManager::TriggerAutoSave() const {
    if (!auto_save_path_.empty()) {
        static_cast<void>(SaveToFile(auto_save_path_));
    }
}

std::string SettingsManager::Serialize() const {
    std::string out;
    out.reserve(256);
    out += "{\n  \"schema_version\": 1,\n";
    out += "  \"search_provider_name\": "; storage::EscapeJsonString(settings_.search_provider_name, out); out += ",\n";
    out += "  \"search_url_template\": "; storage::EscapeJsonString(settings_.search_url_template, out); out += ",\n";
    out += "  \"restore_session_on_startup\": " + std::string(settings_.restore_session_on_startup ? "true" : "false") + ",\n";
    out += "  \"home_page_url\": "; storage::EscapeJsonString(settings_.home_page_url, out); out += ",\n";
    out += "  \"downloads_directory\": "; storage::EscapeJsonString(settings_.downloads_directory, out); out += "\n";
    out += "}\n";
    return out;
}

bool SettingsManager::Deserialize(const std::string_view json) {
    if (json.empty()) {
        return false;
    }

    const auto root = storage::ParseJson(json);
    if (!root.has_value() || root->type != storage::JsonValue::Type::Object) {
        return false;
    }

    const auto search_name = root->GetString("search_provider_name", "DuckDuckGo");
    const auto search_url = root->GetString("search_url_template", "https://duckduckgo.com/?q=%s");
    const auto restore_session = root->GetBool("restore_session_on_startup", true);
    const auto home_page = root->GetString("home_page_url", "about:blank");
    const auto downloads_dir = root->GetString("downloads_directory", "");

    settings_.search_provider_name = search_name.empty() ? "DuckDuckGo" : search_name;
    settings_.search_url_template = search_url.empty() ? "https://duckduckgo.com/?q=%s" : search_url;
    settings_.restore_session_on_startup = restore_session;
    settings_.home_page_url = home_page.empty() ? "about:blank" : home_page;
    settings_.downloads_directory = downloads_dir;

    return true;
}

bool SettingsManager::SaveToFile(const std::filesystem::path& path) const {
    if (path.empty()) return false;
    const auto json = Serialize();
    const auto result = storage::AtomicWriteFile(path, json, /*keep_backup=*/true);
    return result.success;
}

bool SettingsManager::LoadFromFile(const std::filesystem::path& path) {
    if (path.empty()) return false;
    const auto result = storage::ReadFileWithBackupRecovery(path, [](std::string_view content) {
        const auto root = storage::ParseJson(content);
        return root.has_value() && root->type == storage::JsonValue::Type::Object;
    });

    if (!result.success) {
        return false;
    }
    return Deserialize(result.content);
}

}  // namespace openbrowser::core
