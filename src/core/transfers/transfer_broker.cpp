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

bool IsTerminalState(const TransferState state) noexcept {
    return state == TransferState::Completed ||
        state == TransferState::Failed ||
        state == TransferState::Cancelled;
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

void TransferBroker::SetFileBroker(FileBroker* file_broker) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    file_broker_ = file_broker;
}

FileBroker* TransferBroker::GetFileBroker() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return file_broker_;
}

bool TransferBroker::RegisterTransfer(TransferItem item) {
    TransferItem snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (item.id.empty() || transfers_.find(item.id) != transfers_.end()) {
            return false;
        }

        if (item.start_time_ms <= 0) {
            item.start_time_ms = NowEpochMs();
        }
        item.received_bytes = std::max<std::int64_t>(0, item.received_bytes);
        item.total_bytes = std::max<std::int64_t>(0, item.total_bytes);
        item.speed_bytes_per_sec = std::max<std::int64_t>(0, item.speed_bytes_per_sec);
        item.state = TransferState::InProgress;

        const auto id = item.id;
        const auto [it, inserted] = transfers_.emplace(id, std::move(item));
        if (!inserted) {
            return false;
        }
        snapshot = it->second;
    }

    NotifyStarted(snapshot);
    return true;
}

bool TransferBroker::UpdateMetadata(
    const TransferId& id,
    std::string url,
    std::string suggested_filename,
    std::string target_path,
    std::string mime_type) {
    TransferItem snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = transfers_.find(id);
        if (it == transfers_.end() || IsTerminalState(it->second.state)) {
            return false;
        }

        if (!url.empty()) {
            it->second.url = std::move(url);
        }
        if (!suggested_filename.empty()) {
            it->second.suggested_filename = std::move(suggested_filename);
        }
        if (!target_path.empty()) {
            it->second.target_path = std::move(target_path);
        }
        if (!mime_type.empty()) {
            it->second.mime_type = std::move(mime_type);
        }
        snapshot = it->second;
    }

    NotifyUpdated(snapshot);
    return true;
}

bool TransferBroker::UpdateProgress(
    const TransferId& id,
    const std::int64_t received_bytes,
    const std::int64_t total_bytes,
    const std::int64_t current_speed) {
    TransferItem snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = transfers_.find(id);
        if (it == transfers_.end() || IsTerminalState(it->second.state)) {
            return false;
        }

        it->second.received_bytes = std::max<std::int64_t>(0, received_bytes);
        it->second.total_bytes = std::max<std::int64_t>(0, total_bytes);
        it->second.speed_bytes_per_sec = std::max<std::int64_t>(0, current_speed);
        if (it->second.state != TransferState::Paused) {
            it->second.state = TransferState::InProgress;
        }
        snapshot = it->second;
    }

    NotifyUpdated(snapshot);
    return true;
}

void TransferBroker::SetControl(const TransferId& id, TransferControl control) {
    if (id.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (transfers_.find(id) != transfers_.end()) {
        controls_[id] = std::move(control);
    }
}

void TransferBroker::ClearControl(const TransferId& id) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    controls_.erase(id);
}

bool TransferBroker::CompleteTransfer(const TransferId& id) {
    TransferItem snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = transfers_.find(id);
        if (it == transfers_.end() || IsTerminalState(it->second.state)) {
            return false;
        }

        it->second.state = TransferState::Completed;
        it->second.end_time_ms = NowEpochMs();
        it->second.speed_bytes_per_sec = 0;
        if (it->second.total_bytes > 0) {
            it->second.received_bytes = it->second.total_bytes;
        }
        controls_.erase(id);
        snapshot = it->second;
    }

    NotifyFinished(snapshot);
    return true;
}

bool TransferBroker::FailTransfer(const TransferId& id, std::string error_message) {
    TransferItem snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = transfers_.find(id);
        if (it == transfers_.end() || IsTerminalState(it->second.state)) {
            return false;
        }

        it->second.state = TransferState::Failed;
        it->second.error_message = std::move(error_message);
        it->second.end_time_ms = NowEpochMs();
        it->second.speed_bytes_per_sec = 0;
        controls_.erase(id);
        snapshot = it->second;
    }

    NotifyFinished(snapshot);
    return true;
}

bool TransferBroker::CancelTransfer(const TransferId& id) {
    TransferItem snapshot;
    std::function<void()> cancel;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = transfers_.find(id);
        if (it == transfers_.end() || IsTerminalState(it->second.state)) {
            return false;
        }

        const auto control_it = controls_.find(id);
        if (control_it != controls_.end()) {
            cancel = control_it->second.cancel;
        }

        it->second.state = TransferState::Cancelled;
        it->second.end_time_ms = NowEpochMs();
        it->second.speed_bytes_per_sec = 0;
        controls_.erase(id);
        snapshot = it->second;
    }

    if (cancel) {
        cancel();
    }
    NotifyFinished(snapshot);
    return true;
}

bool TransferBroker::PauseTransfer(const TransferId& id) {
    TransferItem snapshot;
    std::function<void()> pause;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = transfers_.find(id);
        if (it == transfers_.end() || it->second.state != TransferState::InProgress) {
            return false;
        }

        const auto control_it = controls_.find(id);
        if (control_it != controls_.end()) {
            pause = control_it->second.pause;
        }

        it->second.state = TransferState::Paused;
        it->second.speed_bytes_per_sec = 0;
        snapshot = it->second;
    }

    if (pause) {
        pause();
    }
    NotifyUpdated(snapshot);
    return true;
}

bool TransferBroker::ResumeTransfer(const TransferId& id) {
    TransferItem snapshot;
    std::function<void()> resume;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = transfers_.find(id);
        if (it == transfers_.end() || it->second.state != TransferState::Paused) {
            return false;
        }

        const auto control_it = controls_.find(id);
        if (control_it != controls_.end()) {
            resume = control_it->second.resume;
        }

        it->second.state = TransferState::InProgress;
        snapshot = it->second;
    }

    if (resume) {
        resume();
    }
    NotifyUpdated(snapshot);
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
        if (item.state == TransferState::Queued ||
            item.state == TransferState::InProgress ||
            item.state == TransferState::Paused) {
            ++active;
        }
    }
    return active;
}

void TransferBroker::NotifyStarted(const TransferItem& item) {
    std::vector<TransferObserver*> observers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        observers = observers_;
    }
    for (auto* obs : observers) {
        if (obs != nullptr) {
            obs->OnTransferStarted(item);
        }
    }
}

void TransferBroker::NotifyUpdated(const TransferItem& item) {
    std::vector<TransferObserver*> observers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        observers = observers_;
    }
    for (auto* obs : observers) {
        if (obs != nullptr) {
            obs->OnTransferUpdated(item);
        }
    }
}

void TransferBroker::NotifyFinished(const TransferItem& item) {
    std::vector<TransferObserver*> observers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        observers = observers_;
    }
    for (auto* obs : observers) {
        if (obs != nullptr) {
            obs->OnTransferFinished(item);
        }
    }
}

}  // namespace openbrowser::core
