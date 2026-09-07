#pragma once

#include "core/transfers/transfer.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace openbrowser::core {

class FileBroker;

class TransferObserver {
public:
    virtual ~TransferObserver() = default;

    virtual void OnTransferStarted(const TransferItem& item) = 0;
    virtual void OnTransferUpdated(const TransferItem& item) = 0;
    virtual void OnTransferFinished(const TransferItem& item) = 0;
};

struct TransferControl {
    std::function<void()> pause;
    std::function<void()> resume;
    std::function<void()> cancel;
};

class TransferBroker {
public:
    TransferBroker() = default;
    ~TransferBroker() = default;

    TransferBroker(const TransferBroker&) = delete;
    TransferBroker& operator=(const TransferBroker&) = delete;

    void AddObserver(TransferObserver* observer);
    void RemoveObserver(TransferObserver* observer) noexcept;

    void SetFileBroker(FileBroker* file_broker) noexcept;
    [[nodiscard]] FileBroker* GetFileBroker() const noexcept;

    bool RegisterTransfer(TransferItem item);
    bool UpdateMetadata(
        const TransferId& id,
        std::string url,
        std::string suggested_filename,
        std::string target_path,
        std::string mime_type);
    bool UpdateProgress(
        const TransferId& id,
        std::int64_t received_bytes,
        std::int64_t total_bytes,
        std::int64_t current_speed);

    void SetControl(const TransferId& id, TransferControl control);
    void ClearControl(const TransferId& id) noexcept;

    bool CompleteTransfer(const TransferId& id);
    bool FailTransfer(const TransferId& id, std::string error_message);
    bool CancelTransfer(const TransferId& id);
    bool PauseTransfer(const TransferId& id);
    bool ResumeTransfer(const TransferId& id);

    [[nodiscard]] const TransferItem* FindTransfer(const TransferId& id) const noexcept;
    [[nodiscard]] std::vector<TransferItem> ListTransfers() const;
    [[nodiscard]] std::size_t ActiveTransfersCount() const noexcept;

private:
    void NotifyStarted(const TransferItem& item);
    void NotifyUpdated(const TransferItem& item);
    void NotifyFinished(const TransferItem& item);

    mutable std::mutex mutex_;
    std::unordered_map<TransferId, TransferItem> transfers_;
    std::unordered_map<TransferId, TransferControl> controls_;
    std::vector<TransferObserver*> observers_;
    FileBroker* file_broker_{nullptr};
};

}  // namespace openbrowser::core
