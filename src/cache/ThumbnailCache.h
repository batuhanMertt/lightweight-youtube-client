#pragma once

#include "core/Types.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <list>
#include <unordered_map>

namespace yt {

class IHttpClient;

struct ThumbnailImage {
    int width{160};
    int height{90};
    Color fallbackColor{50, 50, 60, 255};
    std::vector<uint32_t> pixels; // ARGB pixel buffer for small thumbnail (160x90 = ~57 KB per thumbnail)

    size_t getSizeBytes() const {
        return pixels.size() * sizeof(uint32_t) + sizeof(ThumbnailImage);
    }
};

class ThumbnailCache {
public:
    static ThumbnailCache& instance();

    void setMaxMemoryBytes(size_t maxBytes);
    void put(const std::string& videoId, const ThumbnailImage& image);
    std::shared_ptr<ThumbnailImage> get(const std::string& videoId);
    bool contains(const std::string& videoId) const;

    size_t getCurrentMemoryBytes() const;
    size_t getItemCount() const;
    void clear();

    // Generate a lightweight placeholder thumbnail of target dimensions
    static ThumbnailImage createPlaceholder(int width, int height, const Color& color, const std::string& label = "");

    // Download real JPEG thumbnail from YouTube over HTTPS, decode and store
    bool fetchAndStore(const std::string& videoId, IHttpClient& http, const std::string& customUrl = "");

private:
    ThumbnailCache();

    size_t m_maxMemoryBytes{16 * 1024 * 1024}; // 16 MB default limit
    size_t m_currentMemoryBytes{0};
    mutable std::mutex m_mutex;

    using Entry = std::pair<std::string, std::shared_ptr<ThumbnailImage>>;
    std::list<Entry> m_lruList;
    std::unordered_map<std::string, std::list<Entry>::iterator> m_lookup;
};

} // namespace yt
