#include "youtube/MockYouTubeClient.h"
#include <iostream>
#include <cassert>

void runYouTubeMockTests() {
    std::cout << "\n[TEST] Starting YouTube Mock Client Tests..." << std::endl;

    yt::MockYouTubeClient client("assets");

    // Test 1: Home Feed Bounded to Limit
    auto homeRes = client.getHomeFeed(20);
    assert(homeRes.success && "Home feed fetch must succeed");
    assert(!homeRes.data.items.empty() && "Home feed items must not be empty");
    assert(homeRes.data.items.size() <= 20 && "Home feed items must not exceed limit");
    std::cout << "  - Loaded " << homeRes.data.items.size() << " home videos" << std::endl;

    // Test 2: Search with pagination
    auto searchRes = client.search("linux kernel");
    assert(searchRes.success && "Search must succeed");
    assert(!searchRes.data.items.empty() && "Search results must not be empty");
    assert(searchRes.data.items.size() <= 10 && "Search must respect max 10 results");
    std::cout << "  - Search 'linux kernel' returned " << searchRes.data.items.size() << " videos" << std::endl;

    // Test 3: Subscriptions
    auto subRes = client.getSubscriptions(10);
    assert(subRes.success && "Subscriptions must succeed");
    assert(!subRes.data.empty() && "Subscriptions must not be empty");
    std::cout << "  - Subscriptions returned " << subRes.data.size() << " channels" << std::endl;

    // Test 4: Video Details
    auto detailRes = client.getVideoInfo("vid_001");
    assert(detailRes.success && "Video details must succeed");
    assert(!detailRes.data.title.empty() && "Title must not be empty");
    assert(detailRes.data.durationSeconds > 0 && "Duration must be valid");
    std::cout << "  - Video details retrieved: " << detailRes.data.title << std::endl;

    std::cout << "  -> YouTube Mock Client Tests Passed!" << std::endl;
}
