// M7.1 Connection Diagnostics Tests
// Tests for ConnectionDiagnostics, TlsInfo, and ConnectionRegistry.

#include "devtools/network/connection_diagnostics.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace openbrowser::devtools::network;

static void TestRegisterAndFind() {
    ConnectionRegistry registry;

    ConnectionDiagnostics diag;
    diag.connection_id = "conn-1";
    diag.remote_host   = "example.com";
    diag.remote_ip     = "93.184.216.34";
    diag.remote_port   = 443;
    diag.protocol      = "h2";
    diag.is_reused     = false;
    diag.dns_ms        = 12.5;
    diag.connect_ms    = 45.0;
    diag.tls_ms        = 30.0;

    TlsInfo tls;
    tls.tls_version  = "TLS 1.3";
    tls.cipher_suite = "TLS_AES_128_GCM_SHA256";
    tls.alpn         = "h2";
    tls.cert_subject = "CN=example.com";
    tls.cert_issuer  = "CN=Let's Encrypt R3";
    tls.cert_is_valid = true;
    diag.tls_info = tls;

    registry.Register(diag);

    assert(registry.Size() == 1);

    const auto found = registry.Find("conn-1");
    assert(found.has_value());
    assert(found->remote_host == "example.com");
    assert(found->remote_ip   == "93.184.216.34");
    assert(found->protocol    == "h2");
    assert(found->tls_info.has_value());
    assert(found->tls_info->tls_version == "TLS 1.3");
    assert(!found->is_reused);

    std::cout << "PASS: TestRegisterAndFind\n";
}

static void TestFindMissing() {
    ConnectionRegistry registry;
    const auto result = registry.Find("nonexistent");
    assert(!result.has_value());
    std::cout << "PASS: TestFindMissing\n";
}

static void TestUpdateTiming() {
    ConnectionRegistry registry;
    ConnectionDiagnostics diag;
    diag.connection_id = "conn-2";
    registry.Register(diag);

    registry.UpdateTiming("conn-2", 5.0, 20.0, 15.0);
    const auto found = registry.Find("conn-2");
    assert(found.has_value());
    assert(found->dns_ms     == 5.0);
    assert(found->connect_ms == 20.0);
    assert(found->tls_ms     == 15.0);
    std::cout << "PASS: TestUpdateTiming\n";
}

static void TestUpdateTimingMissingNoOp() {
    ConnectionRegistry registry;
    // Should not throw or crash.
    registry.UpdateTiming("nonexistent", 1.0, 2.0, 3.0);
    assert(registry.Size() == 0);
    std::cout << "PASS: TestUpdateTimingMissingNoOp\n";
}

static void TestUpdateTls() {
    ConnectionRegistry registry;
    ConnectionDiagnostics diag;
    diag.connection_id = "conn-3";
    registry.Register(diag);

    TlsInfo tls;
    tls.tls_version = "TLS 1.2";
    tls.alpn        = "http/1.1";
    registry.UpdateTls("conn-3", tls);

    const auto found = registry.Find("conn-3");
    assert(found.has_value());
    assert(found->tls_info.has_value());
    assert(found->tls_info->tls_version == "TLS 1.2");
    std::cout << "PASS: TestUpdateTls\n";
}

static void TestMarkReused() {
    ConnectionRegistry registry;
    ConnectionDiagnostics diag;
    diag.connection_id = "conn-4";
    diag.is_reused = false;
    registry.Register(diag);

    registry.MarkReused("conn-4", true);
    const auto found = registry.Find("conn-4");
    assert(found.has_value());
    assert(found->is_reused);
    std::cout << "PASS: TestMarkReused\n";
}

static void TestAll() {
    ConnectionRegistry registry;
    for (int i = 0; i < 5; ++i) {
        ConnectionDiagnostics d;
        d.connection_id = "conn-" + std::to_string(i);
        registry.Register(d);
    }
    assert(registry.All().size() == 5);
    std::cout << "PASS: TestAll\n";
}

static void TestClear() {
    ConnectionRegistry registry;
    ConnectionDiagnostics diag;
    diag.connection_id = "conn-x";
    registry.Register(diag);
    assert(registry.Size() == 1);
    registry.Clear();
    assert(registry.Size() == 0);
    assert(!registry.Find("conn-x").has_value());
    std::cout << "PASS: TestClear\n";
}

static void TestReplaceExisting() {
    ConnectionRegistry registry;
    ConnectionDiagnostics diag;
    diag.connection_id = "conn-dup";
    diag.protocol = "h2";
    registry.Register(diag);

    diag.protocol = "h3";
    registry.Register(diag);

    assert(registry.Size() == 1);
    const auto found = registry.Find("conn-dup");
    assert(found.has_value());
    assert(found->protocol == "h3");
    std::cout << "PASS: TestReplaceExisting\n";
}

int main() {
    TestRegisterAndFind();
    TestFindMissing();
    TestUpdateTiming();
    TestUpdateTimingMissingNoOp();
    TestUpdateTls();
    TestMarkReused();
    TestAll();
    TestClear();
    TestReplaceExisting();
    std::cout << "All connection_diagnostics_tests PASSED.\n";
    return 0;
}
