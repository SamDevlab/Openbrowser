#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace openbrowser::core {

enum class ProfileType {
    Persistent,
    Ephemeral
};

struct ProfileConfig {
    std::string id;
    std::string name;
    ProfileType type = ProfileType::Persistent;
    std::filesystem::path storage_path;
    bool enable_telemetry = false;
    bool clear_cache_on_exit = false;
};

class Profile {
public:
    explicit Profile(ProfileConfig config);
    ~Profile();

    [[nodiscard]] const std::string& GetId() const noexcept;
    [[nodiscard]] const std::string& GetName() const noexcept;
    [[nodiscard]] ProfileType GetType() const noexcept;
    [[nodiscard]] bool IsEphemeral() const noexcept;
    [[nodiscard]] const std::filesystem::path& GetStoragePath() const noexcept;

    void SetSecret(const std::string& key, const std::string& value);
    [[nodiscard]] std::string GetSecret(const std::string& key) const;
    [[nodiscard]] bool HasSecret(const std::string& key) const;
    bool RemoveSecret(const std::string& key);

    void SetSessionData(const std::string& key, const std::string& value);
    [[nodiscard]] std::string GetSessionData(const std::string& key) const;
    [[nodiscard]] bool HasSessionData(const std::string& key) const;

    void Purge();

private:
    ProfileConfig config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> in_memory_vault_;
    std::unordered_map<std::string, std::string> session_data_;
};

class ProfileManager {
public:
    ProfileManager();
    ~ProfileManager() = default;

    bool CreateProfile(const ProfileConfig& config);
    [[nodiscard]] std::shared_ptr<Profile> GetProfile(const std::string& id) const;
    [[nodiscard]] std::shared_ptr<Profile> GetActiveProfile() const;
    bool SetActiveProfile(const std::string& id);
    bool RemoveProfile(const std::string& id);

    [[nodiscard]] std::shared_ptr<Profile> CreateEphemeralProfile(const std::string& name_hint = "Private Browsing");
    void PurgeEphemeralProfiles();

    [[nodiscard]] std::vector<ProfileConfig> ListProfiles() const;
    [[nodiscard]] std::size_t Count() const;

private:
    mutable std::mutex mutex_;
    std::string active_profile_id_;
    std::unordered_map<std::string, std::shared_ptr<Profile>> profiles_;
    uint64_t ephemeral_counter_ = 0;
};

} // namespace openbrowser::core
