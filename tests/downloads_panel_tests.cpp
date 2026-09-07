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

    void OnTransferUpdated(const openbrowser::core::TransferItem& item) override {
        updated_count++;
        last_item = item;
    }

    void OnTransferFinished(const openbrowser::core::TransferItem& item) override {
        finished_count++;
        last_item = item;
    }

    int started_count = 0;
    int updated_count = 0;
    int finished_count = 0;
    openbrowser::core::TransferItem last_item;
};

void TestDownloadsPanelTransferObservation() {
    using namespace openbrowser::core;

    TransferBroker broker;
    TestTransferObserver observer;
    broker.AddObserver(&observer);

    TransferItem item;
    item.id = "dl-test-1";
    item.url = "https://example.com/archive.zip";
    item.suggested_filename = "archive.zip";
    item.total_bytes = 1000;

    Require(broker.RegisterTransfer(item), "Transfer registered and started");
    Require(observer.started_count == 1, "Observer notified of start");

    // Progress
    Require(broker.UpdateProgress("dl-test-1", 500, 1000, 50000), "Progress updated");
    Require(observer.updated_count == 1, "Observer notified of update");
    const double pct = (observer.last_item.total_bytes > 0)
        ? (static_cast<double>(observer.last_item.received_bytes) * 100.0 / static_cast<double>(observer.last_item.total_bytes))
        : 0.0;
    Require(pct == 50.0, "Progress is 50%");
    Require(observer.last_item.speed_bytes_per_sec == 50000, "Speed is 50000 bytes/sec");

    // Pause and Resume
    Require(broker.PauseTransfer("dl-test-1"), "Transfer paused");
    const auto* found = broker.FindTransfer("dl-test-1");
    Require(found != nullptr && found->state == TransferState::Paused, "State is Paused");

    Require(broker.ResumeTransfer("dl-test-1"), "Transfer resumed");
    found = broker.FindTransfer("dl-test-1");
    Require(found != nullptr && found->state == TransferState::InProgress, "State is InProgress");

    // Complete
    Require(broker.CompleteTransfer("dl-test-1"), "Transfer completed");
    Require(observer.finished_count == 1, "Observer notified of completion");

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
