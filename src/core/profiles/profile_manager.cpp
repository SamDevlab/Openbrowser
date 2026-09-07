#include "core/profiles/profile_manager.h"

#include <algorithm>
#include <cstring>

namespace openbrowser::core {

namespace {
void SecureZeroMemory(std::string& str) {
    if (!str.empty()) {
        std::fill(str.begin(), str.end(), '\0');
        str.clear();
    }
}
} // namespace

Profile::Profile(ProfileConfig config)
    : config_(std::move(config)) {}

Profile::~Profile() {
    Purge();
}

const std::string& Profile::GetId() const noexcept {
    return config_.id;
}

const std::string& Profile::GetName() const noexcept {
    return config_.name;
}

ProfileType Profile::GetType() const noexcept {
    return config_.type;
}

bool Profile::IsEphemeral() const noexcept {
    return config_.type == ProfileType::Ephemeral;
}

const std::filesystem::path& Profile::GetStoragePath() const noexcept {
    return config_.storage_path;
}

void Profile::SetSecret(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    in_memory_vault_[key] = value;
}

std::string Profile::GetSecret(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = in_memory_vault_.find(key);
    if (it != in_memory_vault_.end()) {
        return it->second;
    }
    return {};
}

bool Profile::HasSecret(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return in_memory_vault_.find(key) != in_memory_vault_.end();
}

bool Profile::RemoveSecret(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = in_memory_vault_.find(key);
    if (it != in_memory_vault_.end()) {
        SecureZeroMemory(it->second);
        in_memory_vault_.erase(it);
        return true;
    }
    return false;
}

void Profile::SetSessionData(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_data_[key] = value;
}

std::string Profile::GetSessionData(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = session_data_.find(key);
    if (it != session_data_.end()) {
        return it->second;
    }
    return {};
}

bool Profile::HasSessionData(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_data_.find(key) != session_data_.end();
}

void Profile::Purge() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [k, v] : in_memory_vault_) {
        SecureZeroMemory(v);
    }
    in_memory_vault_.clear();

    for (auto& [k, v] : session_data_) {
        SecureZeroMemory(v);
    }
    session_data_.clear();
}

ProfileManager::ProfileManager() {
    ProfileConfig default_config;
    default_config.id = "default";
    default_config.name = "Default Profile";
    default_config.type = ProfileType::Persistent;
    default_config.storage_path = "profiles/default";
    CreateProfile(default_config);
    active_profile_id_ = "default";
}

bool ProfileManager::CreateProfile(const ProfileConfig& config) {
    if (config.id.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (profiles_.find(config.id) != profiles_.end()) {
        return false;
    }

    profiles_[config.id] = std::make_shared<Profile>(config);
    if (active_profile_id_.empty()) {
        active_profile_id_ = config.id;
    }
    return true;
}

std::shared_ptr<Profile> ProfileManager::GetProfile(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = profiles_.find(id);
    if (it != profiles_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Profile> ProfileManager::GetActiveProfile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = profiles_.find(active_profile_id_);
    if (it != profiles_.end()) {
        return it->second;
    }
    return nullptr;
}

bool ProfileManager::SetActiveProfile(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (profiles_.find(id) == profiles_.end()) {
        return false;
    }
    active_profile_id_ = id;
    return true;
}

bool ProfileManager::RemoveProfile(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id == "default") {
        return false; // Default profile cannot be deleted
    }

    auto it = profiles_.find(id);
    if (it == profiles_.end()) {
        return false;
    }

    it->second->Purge();
    profiles_.erase(it);

    if (active_profile_id_ == id) {
        active_profile_id_ = "default";
    }
    return true;
}

std::shared_ptr<Profile> ProfileManager::CreateEphemeralProfile(const std::string& name_hint) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++ephemeral_counter_;
    std::string id = "ephemeral-" + std::to_string(ephemeral_counter_);
    std::string name = name_hint.empty() ? ("Private " + std::to_string(ephemeral_counter_)) : name_hint;

    ProfileConfig config;
    config.id = id;
    config.name = name;
    config.type = ProfileType::Ephemeral;
    config.storage_path = ""; // Ephemeral profiles have empty storage path (in-memory only)
    config.enable_telemetry = false;
    config.clear_cache_on_exit = true;

    auto profile = std::make_shared<Profile>(config);
    profiles_[id] = profile;
    return profile;
}

void ProfileManager::PurgeEphemeralProfiles() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = profiles_.begin(); it != profiles_.end();) {
        if (it->second->IsEphemeral()) {
            it->second->Purge();
            if (active_profile_id_ == it->first) {
                active_profile_id_ = "default";
            }
            it = profiles_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<ProfileConfig> ProfileManager::ListProfiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProfileConfig> result;
    result.reserve(profiles_.size());
    for (const auto& [_, p] : profiles_) {
        ProfileConfig cfg;
        cfg.id = p->GetId();
        cfg.name = p->GetName();
        cfg.type = p->GetType();
        cfg.storage_path = p->GetStoragePath();
        cfg.enable_telemetry = false;
        cfg.clear_cache_on_exit = p->IsEphemeral();
        result.push_back(std::move(cfg));
    }
    return result;
}

std::size_t ProfileManager::Count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return profiles_.size();
}

} // namespace openbrowser::core
