#include "devtools/network/connection_diagnostics.h"

#include <algorithm>

namespace openbrowser::devtools::network {

void ConnectionRegistry::Register(ConnectionDiagnostics diag) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_[diag.connection_id] = std::move(diag);
}

void ConnectionRegistry::UpdateTiming(const ConnectionId& id,
                                       double dns_ms,
                                       double connect_ms,
                                       double tls_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return;
    }
    it->second.dns_ms     = dns_ms;
    it->second.connect_ms = connect_ms;
    it->second.tls_ms     = tls_ms;
}

void ConnectionRegistry::UpdateTls(const ConnectionId& id, TlsInfo tls_info) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return;
    }
    it->second.tls_info = std::move(tls_info);
}

void ConnectionRegistry::MarkReused(const ConnectionId& id, bool reused) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return;
    }
    it->second.is_reused = reused;
}

std::optional<ConnectionDiagnostics>
ConnectionRegistry::Find(const ConnectionId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<ConnectionDiagnostics> ConnectionRegistry::All() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ConnectionDiagnostics> result;
    result.reserve(entries_.size());
    for (const auto& [id, diag] : entries_) {
        result.push_back(diag);
    }
    return result;
}

std::size_t ConnectionRegistry::Size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

void ConnectionRegistry::Clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

} // namespace openbrowser::devtools::network
