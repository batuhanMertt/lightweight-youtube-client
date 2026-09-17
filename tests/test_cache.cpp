#include "cache/LruCache.h"
#include "cache/ThumbnailCache.h"
#include <iostream>
#include <cassert>

void runCacheTests() {
    std::cout << "\n[TEST] Starting LRU & Thumbnail Cache Unit Tests..." << std::endl;

    // Test 1: LruCache basic capacity & eviction
    yt::LruCache<std::string, int> cache(3);
    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);

    assert(cache.size() == 3);
    assert(cache.get("a").value() == 1); // Access 'a', making 'b' least recently used

    cache.put("d", 4); // Evicts 'b'
    assert(cache.size() == 3);
    assert(!cache.contains("b") && "'b' must be evicted");
    assert(cache.contains("a") && "'a' must still exist");
    assert(cache.contains("c") && "'c' must still exist");
    assert(cache.contains("d") && "'d' must still exist");

    // Test 2: ThumbnailCache memory bounding
    auto& thumbCache = yt::ThumbnailCache::instance();
    thumbCache.clear();

    // Set tight limit of 200 KB
    thumbCache.setMaxMemoryBytes(200 * 1024);

    // Create 160x90 thumbnail (approx ~57KB each)
    yt::ThumbnailImage img1 = yt::ThumbnailCache::createPlaceholder(160, 90, yt::Color{10, 20, 30, 255});
    yt::ThumbnailImage img2 = yt::ThumbnailCache::createPlaceholder(160, 90, yt::Color{20, 30, 40, 255});
    yt::ThumbnailImage img3 = yt::ThumbnailCache::createPlaceholder(160, 90, yt::Color{30, 40, 50, 255});
    yt::ThumbnailImage img4 = yt::ThumbnailCache::createPlaceholder(160, 90, yt::Color{40, 50, 60, 255});
    yt::ThumbnailImage img5 = yt::ThumbnailCache::createPlaceholder(160, 90, yt::Color{50, 60, 70, 255});

    thumbCache.put("t1", img1);
    thumbCache.put("t2", img2);
    thumbCache.put("t3", img3);
    thumbCache.put("t4", img4);
    thumbCache.put("t5", img5);

    // Verify cache total memory never exceeded 200 KB limit
    assert(thumbCache.getCurrentMemoryBytes() <= 200 * 1024);
    // Oldest items (t1, t2) must have been evicted automatically
    assert(!thumbCache.contains("t1") && "t1 must be evicted to respect bounded memory");

    std::cout << "  -> Cache Eviction & Bounded Memory Tests Passed!" << std::endl;
}
