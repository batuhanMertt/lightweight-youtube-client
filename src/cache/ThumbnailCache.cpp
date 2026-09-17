#include "cache/ThumbnailCache.h"
#include "cache/ImageDecoder.h"
#include "network/IHttpClient.h"
#include "core/Logger.h"

namespace yt {

ThumbnailCache::ThumbnailCache() = default;

ThumbnailCache& ThumbnailCache::instance() {
    static ThumbnailCache s_instance;
    return s_instance;
}

void ThumbnailCache::setMaxMemoryBytes(size_t maxBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxMemoryBytes = maxBytes;
}

void ThumbnailCache::put(const std::string& videoId, const ThumbnailImage& image) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t newBytes = image.getSizeBytes();

    // Check if item already exists
    auto it = m_lookup.find(videoId);
    if (it != m_lookup.end()) {
        m_currentMemoryBytes -= it->second->second->getSizeBytes();
        m_lruList.erase(it->second);
        m_lookup.erase(it);
    }

    // Evict while memory limit is exceeded
    while ((m_currentMemoryBytes + newBytes > m_maxMemoryBytes) && !m_lruList.empty()) {
        LOG_WARN("Thumbnail cache limit reached (" + 
                 std::to_string(m_currentMemoryBytes / 1024) + " KB). Evicting oldest item.");
        auto last = --m_lruList.end();
        m_currentMemoryBytes -= last->second->getSizeBytes();
        m_lookup.erase(last->first);
        m_lruList.pop_back();
    }

    auto imgPtr = std::make_shared<ThumbnailImage>(image);
    m_lruList.push_front(std::make_pair(videoId, imgPtr));
    m_lookup[videoId] = m_lruList.begin();
    m_currentMemoryBytes += newBytes;
}

std::shared_ptr<ThumbnailImage> ThumbnailCache::get(const std::string& videoId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_lookup.find(videoId);
    if (it == m_lookup.end()) {
        return nullptr;
    }
    // Move to front of LRU
    m_lruList.splice(m_lruList.begin(), m_lruList, it->second);
    return it->second->second;
}

bool ThumbnailCache::contains(const std::string& videoId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lookup.find(videoId) != m_lookup.end();
}

size_t ThumbnailCache::getCurrentMemoryBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentMemoryBytes;
}

size_t ThumbnailCache::getItemCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lookup.size();
}

void ThumbnailCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lruList.clear();
    m_lookup.clear();
    m_currentMemoryBytes = 0;
}

ThumbnailImage ThumbnailCache::createPlaceholder(int width, int height, const Color& color, const std::string& /*label*/) {
    ThumbnailImage thumb;
    thumb.width = width;
    thumb.height = height;
    thumb.fallbackColor = color;
    thumb.pixels.resize(width * height);

    for (int y = 0; y < height; ++y) {
        float factor = 0.8f + 0.4f * (static_cast<float>(y) / height);
        uint8_t r = static_cast<uint8_t>(std::min(255.0f, color.r * factor));
        uint8_t g = static_cast<uint8_t>(std::min(255.0f, color.g * factor));
        uint8_t b = static_cast<uint8_t>(std::min(255.0f, color.b * factor));
        uint32_t rowColor = (0xFF << 24) | (r << 16) | (g << 8) | b;
        for (int x = 0; x < width; ++x) {
            thumb.pixels[y * width + x] = rowColor;
        }
    }
    return thumb;
}

bool ThumbnailCache::fetchAndStore(const std::string& videoId, IHttpClient& http, const std::string& customUrl) {
    if (videoId.empty()) return false;

    // Target thumbnail URL: YouTube CDN public thumbnail
    std::string url = customUrl.empty() ? ("https://i.ytimg.com/vi/" + videoId + "/mqdefault.jpg") : customUrl;

    HttpResponse resp = http.get(url, "", 6000);
    if (!resp.isSuccess() || resp.body.empty()) {
        return false;
    }

    DecodedImage decoded = ImageDecoder::decodeFromMemory(reinterpret_cast<const uint8_t*>(resp.body.data()), resp.body.size());
    if (!decoded.valid || decoded.argbPixels.empty()) {
        return false;
    }

    // Downscale to bounded embedded size (160x90)
    int targetW = 160;
    int targetH = 90;
    std::vector<uint32_t> resizedPixels = ImageDecoder::resize(decoded.argbPixels.data(), decoded.width, decoded.height, targetW, targetH);

    ThumbnailImage thumb;
    thumb.width = targetW;
    thumb.height = targetH;
    thumb.pixels = std::move(resizedPixels);

    put(videoId, thumb);
    LOG_INFO("Real YouTube thumbnail downloaded and cached for: " + videoId);
    return true;
}

} // namespace yt
