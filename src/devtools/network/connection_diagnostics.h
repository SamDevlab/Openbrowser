#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace openbrowser::devtools::network {

// Opaque browser-owned connection identifier. Never derived from a CEF or CDP
// identifier — those are adapter-private mappings.
using ConnectionId = std::string;

// TLS/security metadata for an established connection.
struct TlsInfo {
    std::string tls_version;     // e.g. "TLS 1.3", "TLS 1.2", "none"
    std::string cipher_suite;    // e.g. "TLS_AES_128_GCM_SHA256"
    std::string alpn;            // e.g. "h2", "http/1.1", "h3"
    std::string cert_subject;    // e.g. "CN=example.com"
    std::string cert_issuer;     // e.g. "CN=Let's Encrypt R3,..."
    std::string cert_expiry_utc; // ISO 8601 date string
    bool cert_is_valid{true};
};

// Connection-level diagnostics observed at the CEF/Chromium adapter boundary.
// Unavailable fields are represented as empty strings or -1.0.
struct ConnectionDiagnostics {
    ConnectionId connection_id;
    std::string remote_host;      // hostname as navigated
    std::string remote_ip;        // resolved IP address
    std::uint16_t remote_port{0};
    std::string protocol;         // "http/1.1", "h2", "h3", "quic"
    bool is_reused{false};        // connection pool reuse
    std::string proxy_route;      // empty if direct

    // Timing in milliseconds; -1.0 means not observed/available.
    double dns_ms{-1.0};
    double connect_ms{-1.0};
    double tls_ms{-1.0};

    std::optional<TlsInfo> tls_info; // absent for plain HTTP connections
};

// Thread-safe registry mapping ConnectionId -> ConnectionDiagnostics.
// Updated incrementally as the CEF adapter observes connection events.
class ConnectionRegistry {
public:
    ConnectionRegistry() = default;
    ~ConnectionRegistry() = default;

    ConnectionRegistry(const ConnectionRegistry&) = delete;
    ConnectionRegistry& operator=(const ConnectionRegistry&) = delete;

    // Register a new connection or replace an existing one with the same id.
    void Register(ConnectionDiagnostics diag);

    // Partially update timing or TLS fields after initial registration.
    // No-op if the connection_id is not known.
    void UpdateTiming(const ConnectionId& id,
                      double dns_ms, double connect_ms, double tls_ms);
    void UpdateTls(const ConnectionId& id, TlsInfo tls_info);
    void MarkReused(const ConnectionId& id, bool reused) noexcept;

    [[nodiscard]] std::optional<ConnectionDiagnostics>
    Find(const ConnectionId& id) const;

    [[nodiscard]] std::vector<ConnectionDiagnostics> All() const;

    [[nodiscard]] std::size_t Size() const noexcept;

    void Clear() noexcept;

private:
    mutable std::mutex mutex_;
    std::unordered_map<ConnectionId, ConnectionDiagnostics> entries_;
};

} // namespace openbrowser::devtools::network
