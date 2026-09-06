#include "core/navigation/address_input.h"

#include <iostream>
#include <optional>
#include <string>

namespace {

int failures = 0;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestHttpAndHttpsArePreserved() {
    Require(
        openbrowser::core::navigation::NormalizeAddressInput("https://example.com/path") ==
            std::optional<std::string>{"https://example.com/path"},
        "preserve HTTPS URL");
    Require(
        openbrowser::core::navigation::NormalizeAddressInput("HTTP://example.com") ==
            std::optional<std::string>{"HTTP://example.com"},
        "accept HTTP case-insensitively");
}

void TestHttpsIsDefault() {
    Require(
        openbrowser::core::navigation::NormalizeAddressInput("  example.com/docs  ") ==
            std::optional<std::string>{"https://example.com/docs"},
        "trim and default public host to HTTPS");
}

void TestLoopbackUsesHttpForLocalDevelopment() {
    Require(
        openbrowser::core::navigation::NormalizeAddressInput("localhost:3000") ==
            std::optional<std::string>{"http://localhost:3000"},
        "localhost defaults to HTTP");
    Require(
        openbrowser::core::navigation::NormalizeAddressInput("127.0.0.1:8080") ==
            std::optional<std::string>{"http://127.0.0.1:8080"},
        "IPv4 loopback defaults to HTTP");
    Require(
        openbrowser::core::navigation::NormalizeAddressInput("[::1]:8080") ==
            std::optional<std::string>{"http://[::1]:8080"},
        "IPv6 loopback defaults to HTTP");
}

void TestUnexpectedSchemesAndWhitespaceAreRejected() {
    Require(!openbrowser::core::navigation::NormalizeAddressInput("file:///tmp/test.html").has_value(), "reject file scheme");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("javascript:alert(1)").has_value(), "reject javascript scheme");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("data:text/plain,hello").has_value(), "reject data scheme");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("example.com/a b").has_value(), "reject embedded whitespace");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("   ").has_value(), "reject empty address");
}

}  // namespace

int main() {
    TestHttpAndHttpsArePreserved();
    TestHttpsIsDefault();
    TestLoopbackUsesHttpForLocalDevelopment();
    TestUnexpectedSchemesAndWhitespaceAreRejected();

    if (failures != 0) {
        std::cerr << failures << " address input assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser address input: PASS\n";
    return 0;
}
