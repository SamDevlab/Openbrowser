#include "core/transfers/transfer_broker.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace openbrowser::core {

namespace {

std::int64_t NowEpochMs() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

}  // namespace

void TransferBroker::AddObserver(TransferObserver* observer) {
    if (observer == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end()) {
        observers_.push_back(observer);
    }
}

void TransferBroker::RemoveObserver(TransferObserver* observer) noexcept {
    if (observer == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = std::remove(observers_.begin(), observers_.end(), observer);
    observers_.erase(it, observers_.end());
}

bool TransferBroker::RegisterTransfer(TransferItem item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (item.id.empty() || transfers_.find(item.id) != transfers_.end()) {
        return false;
    }

    if (item.start_time_ms <= 0) {
        item.start_time_ms = NowEpochMs();
    }
    item.state = TransferState::InProgress;

    const auto id = item.id;
    const auto [it, inserted] = transfers_.emplace(id, std::move(item));
    if (!inserted) {
        return false;
    }

    NotifyStarted(it->second);
    return true;
}

bool TransferBroker::UpdateProgress(
    const TransferId& id,
    const std::int64_t received_bytes,
    const std::int64_t total_bytes,
    const std::int64_t current_speed) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = transfers_.find(id);
    if (it == transfers_.end()) {
        return false;
    }

    if (it->second.state != TransferState::InProgress && it->second.state != TransferState::Queued) {
        return false;
    }

    it->second.received_bytes = received_bytes;
    it->second.total_bytes = total_bytes;
    it->second.speed_bytes_per_sec = current_speed;
    it->second.state = TransferState::InProgress;

    NotifyUpdated(it->second);
    return true;
}

bool TransferBroker::CompleteTransfer(const TransferId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = transfers_.find(id);
    if (it == transfers_.end()) {
        return false;
    }

    it->second.state = TransferState::Completed;
    it->second.end_time_ms = NowEpochMs();
    it->second.speed_bytes_per_sec = 0;
    if (it->second.total_bytes > 0) {
        it->second.received_bytes = it->second.total_bytes;
    }

    NotifyFinished(it->second);
    return true;
}

bool TransferBroker::FailTransfer(const TransferId& id, std::string error_message) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = transfers_.find(id);
    if (it == transfers_.end()) {
        return false;
    }

    it->second.state = TransferState::Failed;
    it->second.error_message = std::move(error_message);
    it->second.end_time_ms = NowEpochMs();
    it->second.speed_bytes_per_sec = 0;

    NotifyFinished(it->second);
    return true;
}

bool TransferBroker::CancelTransfer(const TransferId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = transfers_.find(id);
    if (it == transfers_.end()) {
        return false;
    }

    it->second.state = TransferState::Cancelled;
    it->second.end_time_ms = NowEpochMs();
    it->second.speed_bytes_per_sec = 0;

    NotifyFinished(it->second);
    return true;
}

bool TransferBroker::PauseTransfer(const TransferId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = transfers_.find(id);
    if (it == transfers_.end() || it->second.state != TransferState::InProgress) {
        return false;
    }

    it->second.state = TransferState::Paused;
    it->second.speed_bytes_per_sec = 0;

    NotifyUpdated(it->second);
    return true;
}

bool TransferBroker::ResumeTransfer(const TransferId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = transfers_.find(id);
    if (it == transfers_.end() || it->second.state != TransferState::Paused) {
        return false;
    }

    it->second.state = TransferState::InProgress;

    NotifyUpdated(it->second);
    return true;
}

const TransferItem* TransferBroker::FindTransfer(const TransferId& id) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = transfers_.find(id);
    if (it == transfers_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<TransferItem> TransferBroker::ListTransfers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TransferItem> result;
    result.reserve(transfers_.size());
    for (const auto& [_, item] : transfers_) {
        result.push_back(item);
    }
    return result;
}

std::size_t TransferBroker::ActiveTransfersCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t active = 0;
    for (const auto& [_, item] : transfers_) {
        if (item.state == TransferState::InProgress || item.state == TransferState::Paused) {
            ++active;
        }
    }
    return active;
}

void TransferBroker::NotifyStarted(const TransferItem& item) {
    for (auto* obs : observers_) {
        if (obs != nullptr) {
            obs->OnTransferStarted(item);
        }
    }
}

void TransferBroker::NotifyUpdated(const TransferItem& item) {
    for (auto* obs : observers_) {
        if (obs != nullptr) {
            obs->OnTransferUpdated(item);
        }
    }
}

void TransferBroker::NotifyFinished(const TransferItem& item) {
    for (auto* obs : observers_) {
        if (obs != nullptr) {
            obs->OnTransferFinished(item);
        }
    }
}

}  // namespace openbrowser::core
