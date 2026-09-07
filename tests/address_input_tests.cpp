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
    Require(
        openbrowser::core::navigation::NormalizeAddressInput("github.com/openai") ==
            std::optional<std::string>{"https://github.com/openai"},
        "default path-based host to HTTPS");
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
        openbrowser::core::navigation::NormalizeAddressInput("127.0.0.1:5173") ==
            std::optional<std::string>{"http://127.0.0.1:5173"},
        "IPv4 Vite dev port defaults to HTTP");
    Require(
        openbrowser::core::navigation::NormalizeAddressInput("[::1]:8080") ==
            std::optional<std::string>{"http://[::1]:8080"},
        "IPv6 loopback defaults to HTTP");
}

void TestUnexpectedSchemesAndWhitespaceAreRejected() {
    Require(!openbrowser::core::navigation::NormalizeAddressInput("file:///tmp/test.html").has_value(), "reject file scheme");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("file:///etc/passwd").has_value(), "reject file scheme etc passwd");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("javascript:alert(1)").has_value(), "reject javascript scheme");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("data:text/plain,hello").has_value(), "reject data scheme");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("data:text/html,<h1>test</h1>").has_value(), "reject data html scheme");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("custom://protocol").has_value(), "reject custom scheme");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("example.com/a b").has_value(), "reject embedded whitespace");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("   ").has_value(), "reject whitespace-only address");
    Require(!openbrowser::core::navigation::NormalizeAddressInput("").has_value(), "reject empty address");
}

void TestSearchQueryFallback() {
    using openbrowser::core::navigation::ResolveAddressInput;

    // Direct URLs and domains
    Require(
        ResolveAddressInput("https://github.com") == std::optional<std::string>{"https://github.com"},
        "ResolveAddressInput direct https");
    Require(
        ResolveAddressInput("http://example.org/page") == std::optional<std::string>{"http://example.org/page"},
        "ResolveAddressInput direct http");
    Require(
        ResolveAddressInput("about:blank") == std::optional<std::string>{"about:blank"},
        "ResolveAddressInput about scheme");
    Require(
        ResolveAddressInput("github.com") == std::optional<std::string>{"https://github.com"},
        "ResolveAddressInput naked domain");
    Require(
        ResolveAddressInput("localhost:8080") == std::optional<std::string>{"http://localhost:8080"},
        "ResolveAddressInput loopback");

    // Search query fallback for text with spaces
    Require(
        ResolveAddressInput("open source browser github") ==
            std::optional<std::string>{"https://duckduckgo.com/?q=open+source+browser+github"},
        "ResolveAddressInput search query with spaces");

    // Single-word search queries (not domains)
    Require(
        ResolveAddressInput("browser") ==
            std::optional<std::string>{"https://duckduckgo.com/?q=browser"},
        "ResolveAddressInput single word query");

    // Special characters encoding
    Require(
        ResolveAddressInput("c++ standard library") ==
            std::optional<std::string>{"https://duckduckgo.com/?q=c%2B%2B+standard+library"},
        "ResolveAddressInput encoded query");

    // Empty and whitespace-only
    Require(!ResolveAddressInput("").has_value(), "ResolveAddressInput empty rejected");
    Require(!ResolveAddressInput("   ").has_value(), "ResolveAddressInput whitespace-only rejected");
}

void TestCustomSearchProvider() {
    using openbrowser::core::navigation::ResolveAddressInput;
    using openbrowser::core::navigation::SearchProvider;

    const SearchProvider google_provider{
        .name = "Google",
        .search_url_template = "https://www.google.com/search?q=%s",
    };

    Require(
        ResolveAddressInput("hello world", google_provider) ==
            std::optional<std::string>{"https://www.google.com/search?q=hello+world"},
        "custom search provider URL template");
}

void TestIpLiteralsAreDirectNavigation() {
    using openbrowser::core::navigation::ResolveAddressInput;
    using openbrowser::core::navigation::NormalizeAddressInput;

    // IPv4 literals
    Require(
        ResolveAddressInput("192.168.1.1") == std::optional<std::string>{"https://192.168.1.1"},
        "IPv4 literal resolves to https");
    Require(
        ResolveAddressInput("192.168.1.1:8080") == std::optional<std::string>{"https://192.168.1.1:8080"},
        "IPv4 literal with port resolves to https");
    Require(
        ResolveAddressInput("10.0.0.5/path") == std::optional<std::string>{"https://10.0.0.5/path"},
        "IPv4 literal with path resolves to https");

    // IPv6 literals
    Require(
        ResolveAddressInput("[2001:db8::1]") == std::optional<std::string>{"https://[2001:db8::1]"},
        "IPv6 literal resolves to https");
    Require(
        ResolveAddressInput("[2001:db8::1]:8080") == std::optional<std::string>{"https://[2001:db8::1]:8080"},
        "IPv6 literal with port resolves to https");

    // NormalizeAddressInput directly
    Require(
        NormalizeAddressInput("192.168.1.1") == std::optional<std::string>{"https://192.168.1.1"},
        "NormalizeAddressInput IPv4 literal");
    Require(
        NormalizeAddressInput("[2001:db8::1]:8080") == std::optional<std::string>{"https://[2001:db8::1]:8080"},
        "NormalizeAddressInput IPv6 literal");

    // IP address inside a search phrase with spaces must still be sent to search provider
    Require(
        ResolveAddressInput("find 192.168.1.1 device") ==
            std::optional<std::string>{"https://duckduckgo.com/?q=find+192.168.1.1+device"},
        "search phrase containing IP falls through to search provider");
}

}  // namespace

int main() {
    TestHttpAndHttpsArePreserved();
    TestHttpsIsDefault();
    TestLoopbackUsesHttpForLocalDevelopment();
    TestUnexpectedSchemesAndWhitespaceAreRejected();
    TestSearchQueryFallback();
    TestCustomSearchProvider();
    TestIpLiteralsAreDirectNavigation();

    if (failures != 0) {
        std::cerr << failures << " address input assertion(s) failed\n";
        return 1;
    }

    std::cout << "Openbrowser address input: PASS\n";
    return 0;
}
