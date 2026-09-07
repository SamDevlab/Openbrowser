#include "core/transfers/file_broker.h"
#include "core/transfers/transfer_broker.h"

#include <iostream>
#include <string>

namespace {
void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

class TestTransferObserver final : public openbrowser::core::TransferObserver {
public:
    void OnTransferStarted(const openbrowser::core::TransferItem& item) override {
        started_count++;
        last_item = item;
    }

    void OnTransferProgress(const openbrowser::core::TransferItem& item) override {
        progress_count++;
        last_item = item;
    }

    void OnTransferCompleted(const openbrowser::core::TransferItem& item) override {
        completed_count++;
        last_item = item;
    }

    void OnTransferFailed(const openbrowser::core::TransferItem& item, const std::string& error) override {
        failed_count++;
        last_error = error;
        last_item = item;
    }

    int started_count = 0;
    int progress_count = 0;
    int completed_count = 0;
    int failed_count = 0;
    std::string last_error;
    openbrowser::core::TransferItem last_item;
};

void TestDownloadsPanelTransferObservation() {
    using namespace openbrowser::core;

    TransferBroker broker;
    TestTransferObserver observer;
    broker.AddObserver(&observer);

    const auto id = broker.StartTransfer("https://example.com/archive.zip", "archive.zip", 1000);
    Require(id > 0, "Transfer started");
    Require(observer.started_count == 1, "Observer notified of start");

    // Progress
    broker.UpdateProgress(id, 500, 1000, 50000);
    Require(observer.progress_count == 1, "Observer notified of progress");
    Require(observer.last_item.ProgressPercentage() == 50.0, "Progress is 50%");
    Require(observer.last_item.speed_bytes_per_sec == 50000, "Speed is 50000 bytes/sec");

    // Pause and Resume
    Require(broker.PauseTransfer(id), "Transfer paused");
    auto item = broker.GetTransfer(id);
    Require(item.has_value() && item->state == TransferState::Paused, "State is Paused");

    Require(broker.ResumeTransfer(id), "Transfer resumed");
    item = broker.GetTransfer(id);
    Require(item.has_value() && item->state == TransferState::Downloading, "State is Downloading");

    // Complete
    broker.CompleteTransfer(id);
    Require(observer.completed_count == 1, "Observer notified of completion");

    // Sanitize filename through FileBroker
    std::string sanitized = FileBroker::SanitizeFilename(observer.last_item.suggested_filename);
    Require(sanitized == "archive.zip", "Filename sanitized safely");

    broker.RemoveObserver(&observer);
}

} // namespace

int main() {
    TestDownloadsPanelTransferObservation();

    std::cout << "All DownloadsPanel tests passed successfully." << std::endl;
    return 0;
}
