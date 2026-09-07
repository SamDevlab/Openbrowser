#pragma once

#include "core/tabs/tab.h"

#include <cstdint>
#include <optional>
#include <string>

namespace openbrowser::core {

using TransferId = std::string;

enum class TransferState {
    Queued,
    InProgress,
    Paused,
    Completed,
    Failed,
    Cancelled,
};

struct TransferItem {
    TransferId id;
    std::string url;
    std::string suggested_filename;
    std::string target_path;
    std::string mime_type;
    std::int64_t received_bytes{0};
    std::int64_t total_bytes{0};
    std::int64_t speed_bytes_per_sec{0};
    TransferState state{TransferState::Queued};
    std::string error_message;
    std::optional<WorkspaceId> workspace_id;
    std::int64_t start_time_ms{0};
    std::int64_t end_time_ms{0};
};

}  // namespace openbrowser::core
