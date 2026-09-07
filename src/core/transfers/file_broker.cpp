#include "core/transfers/file_broker.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace openbrowser::core {
namespace {

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool IsReservedWindowsName(const std::string& name) {
    static constexpr std::array reserved_names{
        "con", "prn", "aux", "nul",
        "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
        "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
    };

    const auto lower = LowerAscii(name);
    return std::find(reserved_names.begin(), reserved_names.end(), lower) != reserved_names.end();
}

}  // namespace

FileBroker::FileBroker(std::filesystem::path allowed_directory)
    : allowed_directory_(std::move(allowed_directory)) {}

void FileBroker::SetAllowedDirectory(std::filesystem::path dir) {
    allowed_directory_ = std::move(dir);
}

const std::filesystem::path& FileBroker::AllowedDirectory() const noexcept {
    return allowed_directory_;
}

bool FileBroker::IsPathContained(const std::filesystem::path& path) const {
    if (allowed_directory_.empty()) {
        return false;
    }

    const auto normal_allowed = allowed_directory_.lexically_normal();
    const auto normal_candidate = path.lexically_normal();

    // Reject paths containing parent directory traversals
    for (const auto& part : normal_candidate) {
        if (part == "..") {
            return false;
        }
    }

    auto allowed_it = normal_allowed.begin();
    auto cand_it = normal_candidate.begin();

    while (allowed_it != normal_allowed.end() && cand_it != normal_candidate.end()) {
        if (*allowed_it != *cand_it) {
            return false;
        }
        ++allowed_it;
        ++cand_it;
    }

    return allowed_it == normal_allowed.end();
}

std::string FileBroker::SanitizeFilename(const std::string_view raw_filename) {
    std::string clean;
    clean.reserve(raw_filename.size());

    for (const char c : raw_filename) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc < 32 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            clean += '_';
        } else {
            clean += c;
        }
    }

    // Trim trailing dots and spaces
    while (!clean.empty() && (clean.back() == '.' || clean.back() == ' ')) {
        clean.pop_back();
    }

    if (clean.empty()) {
        return "download";
    }

    // Check stem for Windows reserved device names
    const std::filesystem::path path_check(clean);
    const auto stem = path_check.stem().string();
    if (IsReservedWindowsName(stem)) {
        clean = "_" + clean;
    }

    return clean;
}

FileRiskLevel FileBroker::AssessRisk(const std::filesystem::path& path) {
    const auto ext = LowerAscii(path.extension().string());

    static constexpr std::array dangerous_scripts{
        ".vbs", ".js", ".jse", ".bat", ".cmd", ".ps1", ".sh", ".bash"
    };

    static constexpr std::array caution_executables{
        ".exe", ".msi", ".dll", ".com", ".scr", ".app", ".dmg", ".pkg", ".iso"
    };

    if (std::find(dangerous_scripts.begin(), dangerous_scripts.end(), ext) != dangerous_scripts.end()) {
        return FileRiskLevel::DangerousScript;
    }

    if (std::find(caution_executables.begin(), caution_executables.end(), ext) != caution_executables.end()) {
        return FileRiskLevel::CautionExecutable;
    }

    return FileRiskLevel::Safe;
}

std::filesystem::path FileBroker::ResolveCollision(
    const std::string& sanitized_filename,
    const std::filesystem::path& base_dir) const {
    const auto& dir = base_dir.empty() ? allowed_directory_ : base_dir;
    const std::filesystem::path target = dir / sanitized_filename;

    if (!std::filesystem::exists(target)) {
        return target;
    }

    const std::filesystem::path initial(sanitized_filename);
    const auto stem = initial.stem().string();
    const auto ext = initial.extension().string();

    for (std::size_t i = 1; i < 10000; ++i) {
        const std::string numbered = stem + " (" + std::to_string(i) + ")" + ext;
        const auto candidate = dir / numbered;
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return target;
}

}  // namespace openbrowser::core
