#include "core/transfers/transfer_broker.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

class MockTransferObserver final : public openbrowser::core::TransferObserver {
public:
    void OnTransferStarted(const openbrowser::core::TransferItem& item) override {
        started.push_back(item.id);
    }
    void OnTransferUpdated(const openbrowser::core::TransferItem& item) override {
        updated.push_back(item.id);
    }
    void OnTransferFinished(const openbrowser::core::TransferItem& item) override {
        finished.push_back(item.id);
    }

    std::vector<std::string> started;
    std::vector<std::string> updated;
    std::vector<std::string> finished;
};

void TestTransferBrokerRegistration() {
    using namespace openbrowser::core;

    TransferBroker broker;
    Require(broker.ActiveTransfersCount() == 0, "Initial active transfers should be 0");

    TransferItem item;
    item.id = "dl-1";
    item.url = "https://example.com/file.zip";
    item.suggested_filename = "file.zip";
    item.target_path = "/downloads/file.zip";
    item.total_bytes = 1000;

    Require(broker.RegisterTransfer(item), "RegisterTransfer should succeed");
    Require(broker.ActiveTransfersCount() == 1, "Active transfers should be 1");

    const auto* found = broker.FindTransfer("dl-1");
    Require(found != nullptr, "FindTransfer should locate dl-1");
    Require(found->state == TransferState::InProgress, "State should be InProgress");

    // Duplicate registration should fail
    Require(!broker.RegisterTransfer(item), "Duplicate registration should fail");
}

void TestTransferProgressAndSpeed() {
    using namespace openbrowser::core;

    TransferBroker broker;
    TransferItem item;
    item.id = "dl-2";
    item.url = "https://example.com/archive.tar.gz";
    item.total_bytes = 50000;
    broker.RegisterTransfer(item);

    Require(broker.UpdateProgress("dl-2", 15000, 50000, 10240), "UpdateProgress should succeed");

    const auto* found = broker.FindTransfer("dl-2");
    Require(found != nullptr && found->received_bytes == 15000, "Received bytes should match");
    Require(found->speed_bytes_per_sec == 10240, "Speed should match");

    Require(broker.PauseTransfer("dl-2"), "PauseTransfer should succeed");
    Require(broker.FindTransfer("dl-2")->state == TransferState::Paused, "State should be Paused");
    Require(broker.FindTransfer("dl-2")->speed_bytes_per_sec == 0, "Speed should be 0 when paused");

    Require(broker.ResumeTransfer("dl-2"), "ResumeTransfer should succeed");
    Require(broker.FindTransfer("dl-2")->state == TransferState::InProgress, "State should be InProgress");
}

void TestTransferCompletionAndCancellation() {
    using namespace openbrowser::core;

    TransferBroker broker;

    TransferItem item1;
    item1.id = "dl-complete";
    item1.total_bytes = 1000;
    broker.RegisterTransfer(item1);

    Require(broker.CompleteTransfer("dl-complete"), "CompleteTransfer should succeed");
    Require(broker.FindTransfer("dl-complete")->state == TransferState::Completed, "State should be Completed");
    Require(broker.FindTransfer("dl-complete")->received_bytes == 1000, "Received bytes should equal total on complete");

    TransferItem item2;
    item2.id = "dl-cancel";
    broker.RegisterTransfer(item2);
    Require(broker.CancelTransfer("dl-cancel"), "CancelTransfer should succeed");
    Require(broker.FindTransfer("dl-cancel")->state == TransferState::Cancelled, "State should be Cancelled");

    TransferItem item3;
    item3.id = "dl-fail";
    broker.RegisterTransfer(item3);
    Require(broker.FailTransfer("dl-fail", "Disk full"), "FailTransfer should succeed");
    const auto* failed = broker.FindTransfer("dl-fail");
    Require(failed->state == TransferState::Failed, "State should be Failed");
    Require(failed->error_message == "Disk full", "Error message should match");

    Require(broker.ActiveTransfersCount() == 0, "All transfers finished; active count should be 0");
}

void TestTransferObserverEvents() {
    using namespace openbrowser::core;

    TransferBroker broker;
    MockTransferObserver observer;
    broker.AddObserver(&observer);

    TransferItem item;
    item.id = "obs-1";
    broker.RegisterTransfer(item);
    broker.UpdateProgress("obs-1", 100, 200, 50);
    broker.CompleteTransfer("obs-1");

    Require(observer.started.size() == 1 && observer.started[0] == "obs-1", "Observer should receive started");
    Require(observer.updated.size() == 1 && observer.updated[0] == "obs-1", "Observer should receive updated");
    Require(observer.finished.size() == 1 && observer.finished[0] == "obs-1", "Observer should receive finished");

    broker.RemoveObserver(&observer);
    TransferItem item2;
    item2.id = "obs-2";
    broker.RegisterTransfer(item2);
    Require(observer.started.size() == 1, "Unsubscribed observer should not receive events");
}

}  // namespace

int main() {
    TestTransferBrokerRegistration();
    TestTransferProgressAndSpeed();
    TestTransferCompletionAndCancellation();
    TestTransferObserverEvents();

    if (failures != 0) {
        std::cerr << "transfer_tests failed with " << failures << " failures.\n";
        return 1;
    }

    std::cout << "All transfer tests passed!\n";
    return 0;
}
