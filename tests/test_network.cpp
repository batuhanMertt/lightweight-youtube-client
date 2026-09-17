#include "youtube/YouTubeClient.h"
#include "auth/AuthService.h"
#include "auth/TokenStorage.h"
#include <iostream>
#include <cassert>

void runNetworkTests() {
    std::cout << "\n[TEST] Starting Network & YouTube Client API Unit Tests..." << std::endl;

    // Test 1: ISO 8601 Duration Parsing
    assert(yt::YouTubeClient::parseIso8601Duration("PT42M15S") == "42:15");
    assert(yt::YouTubeClient::parseIso8601Duration("PT1H2M30S") == "1:02:30");
    assert(yt::YouTubeClient::parseIso8601Duration("PT5M") == "5:00");
    assert(yt::YouTubeClient::parseIso8601Duration("PT45S") == "0:45");
    std::cout << "  - ISO 8601 Duration Parser: OK" << std::endl;

    // Test 2: URL Encoder
    assert(yt::YouTubeClient::urlEncode("linux kernel") == "linux+kernel");
    assert(yt::YouTubeClient::urlEncode("c++20 systems") == "c%2b%2b20+systems");
    std::cout << "  - URL Encoder: OK" << std::endl;

    // Test 3: YouTubeClient Missing API Key Error Handling
    yt::YouTubeClient client(nullptr, "");
    auto res = client.search("linux");
    assert(!res.success && res.error == yt::ErrorCode::API_ERROR);
    std::cout << "  - API Key Error Handling: OK" << std::endl;

    // Test 4: OAuth RFC 8628 Device Authorization Code Generation
    auto& auth = yt::AuthService::instance();
    auto devCodeRes = auth.requestDeviceCode();
    assert(devCodeRes.success && "Device code generation must succeed");
    assert(!devCodeRes.data.userCode.empty());
    assert(devCodeRes.data.verificationUrl == "https://www.google.com/device");
    std::cout << "  - RFC 8628 Device Auth Code Flow: OK (User Code: " << devCodeRes.data.userCode << ")" << std::endl;

    // Test 5: Token Storage & Session Expiration
    auto& storage = yt::TokenStorage::instance();
    storage.clear();
    assert(!storage.hasValidToken());

    storage.saveTokens("test_access_tok_123", "test_refresh_tok_456", 3600);
    assert(storage.hasValidToken());
    assert(storage.getAccessToken().value() == "test_access_tok_123");
    assert(storage.getRefreshToken().value() == "test_refresh_tok_456");

    storage.clear();
    assert(!storage.hasValidToken());
    std::cout << "  - Token Storage & Expiry Lifecycle: OK" << std::endl;

    std::cout << "  -> Network, API & Auth Tests Passed!" << std::endl;
}
