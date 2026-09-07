#include "core/transfers/file_broker.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestSandboxPathContainment() {
    using namespace openbrowser::core;

    const std::filesystem::path allowed = "/home/user/downloads";
    FileBroker broker(allowed);

    Require(broker.AllowedDirectory() == allowed, "AllowedDirectory matches constructor input");
    Require(broker.IsPathContained("/home/user/downloads/report.pdf"), "Sub-path is contained");
    Require(broker.IsPathContained("/home/user/downloads/nested/sub/file.zip"), "Nested sub-path is contained");

    // Traversal rejection
    Require(!broker.IsPathContained("/home/user/downloads/../secrets.txt"), "Traversal '..' is rejected");
    Require(!broker.IsPathContained("/home/user/other/file.zip"), "Unrelated path is rejected");
    Require(!broker.IsPathContained("/etc/passwd"), "System path is rejected");

    // Empty allowed dir containment returns false
    FileBroker empty_broker;
    Require(!empty_broker.IsPathContained("/home/user/downloads/test.pdf"), "Empty broker allows nothing");
}

void TestFilenameSanitization() {
    using namespace openbrowser::core;

    Require(FileBroker::SanitizeFilename("valid_name.pdf") == "valid_name.pdf", "Normal filename untouched");
    Require(FileBroker::SanitizeFilename("../../bad:name*.zip") == "____bad_name_.zip", "Separators and illegal chars replaced");
    Require(FileBroker::SanitizeFilename("trailing.dots... ") == "trailing.dots", "Trailing dots and spaces trimmed");
    Require(FileBroker::SanitizeFilename("CON.txt") == "_CON.txt", "Windows reserved name prefixed");
    Require(FileBroker::SanitizeFilename("aux") == "_aux", "Reserved name aux prefixed");
    Require(FileBroker::SanitizeFilename("   ") == "download", "Empty or whitespace-only defaults to 'download'");
}

void TestRiskAssessment() {
    using namespace openbrowser::core;

    Require(FileBroker::AssessRisk("document.pdf") == FileRiskLevel::Safe, "PDF is safe");
    Require(FileBroker::AssessRisk("photo.JPEG") == FileRiskLevel::Safe, "Case-insensitive safe ext");
    Require(FileBroker::AssessRisk("setup.exe") == FileRiskLevel::CautionExecutable, "EXE is CautionExecutable");
    Require(FileBroker::AssessRisk("installer.msi") == FileRiskLevel::CautionExecutable, "MSI is CautionExecutable");
    Require(FileBroker::AssessRisk("bundle.dmg") == FileRiskLevel::CautionExecutable, "DMG is CautionExecutable");
    Require(FileBroker::AssessRisk("script.vbs") == FileRiskLevel::DangerousScript, "VBS is DangerousScript");
    Require(FileBroker::AssessRisk("deploy.sh") == FileRiskLevel::DangerousScript, "SH is DangerousScript");
    Require(FileBroker::AssessRisk("batch.bat") == FileRiskLevel::DangerousScript, "BAT is DangerousScript");
}

void TestCollisionResolution() {
    using namespace openbrowser::core;

    const auto temp_dir = std::filesystem::temp_directory_path() / "openbrowser_broker_test";
    std::filesystem::create_directories(temp_dir);

    FileBroker broker(temp_dir);

    const auto target1 = broker.ResolveCollision("test_file.txt");
    Require(target1 == temp_dir / "test_file.txt", "Non-existing file does not rename");

    // Create target1
    {
        std::ofstream f(target1);
        f << "content";
    }

    const auto target2 = broker.ResolveCollision("test_file.txt");
    Require(target2 == temp_dir / "test_file (1).txt", "Collision resolved with (1)");

    // Create target2
    {
        std::ofstream f(target2);
        f << "content2";
    }

    const auto target3 = broker.ResolveCollision("test_file.txt");
    Require(target3 == temp_dir / "test_file (2).txt", "Collision resolved with (2)");

    // Cleanup
    std::filesystem::remove(target1);
    std::filesystem::remove(target2);
    std::filesystem::remove_all(temp_dir);
}

}  // namespace

int main() {
    TestSandboxPathContainment();
    TestFilenameSanitization();
    TestRiskAssessment();
    TestCollisionResolution();

    if (failures != 0) {
        std::cerr << failures << " FileBroker test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser FileBroker sandbox invariants: PASS\n";
    return 0;
}
