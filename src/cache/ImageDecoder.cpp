#include "cache/ImageDecoder.h"
#include "core/Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

#include <algorithm>

namespace yt {

DecodedImage ImageDecoder::decodeFromMemory(const uint8_t* data, size_t size) {
    DecodedImage result;
    if (!data || size == 0) return result;

    int w = 0, h = 0, comp = 0;
    // Request 4 channels (RGBA)
    stbi_uc* decoded = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &comp, 4);
    if (!decoded) {
        LOG_WARN("Failed to decode image from memory buffer");
        return result;
    }

    result.width = w;
    result.height = h;
    result.channels = 4;
    result.valid = true;
    result.argbPixels.resize(w * h);

    // Convert RGBA to 32-bit ARGB (for Windows GDI / framebuffers)
    for (int i = 0; i < w * h; ++i) {
        uint8_t r = decoded[i * 4 + 0];
        uint8_t g = decoded[i * 4 + 1];
        uint8_t b = decoded[i * 4 + 2];
        uint8_t a = decoded[i * 4 + 3];
        result.argbPixels[i] = (static_cast<uint32_t>(a) << 24) |
                               (static_cast<uint32_t>(r) << 16) |
                               (static_cast<uint32_t>(g) << 8)  |
                               static_cast<uint32_t>(b);
    }

    stbi_image_free(decoded);
    return result;
}

std::vector<uint32_t> ImageDecoder::resize(const uint32_t* src, int srcW, int srcH, int dstW, int dstH) {
    std::vector<uint32_t> dst(dstW * dstH);
    if (!src || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
        return dst;
    }

    float xRatio = static_cast<float>(srcW) / static_cast<float>(dstW);
    float yRatio = static_cast<float>(srcH) / static_cast<float>(dstH);

    for (int dy = 0; dy < dstH; ++dy) {
        int sy = std::min(srcH - 1, static_cast<int>(dy * yRatio));
        for (int dx = 0; dx < dstW; ++dx) {
            int sx = std::min(srcW - 1, static_cast<int>(dx * xRatio));
            dst[dy * dstW + dx] = src[sy * srcW + sx];
        }
    }
    return dst;
}

} // namespace yt
