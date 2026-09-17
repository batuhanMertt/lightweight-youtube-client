#pragma once

#include "core/Types.h"
#include <vector>
#include <cstdint>
#include <string>

namespace yt {

struct DecodedImage {
    int width{0};
    int height{0};
    int channels{0};
    std::vector<uint32_t> argbPixels;
    bool valid{false};
};

class ImageDecoder {
public:
    // Decode JPEG, PNG from memory buffer into 32-bit ARGB pixels
    static DecodedImage decodeFromMemory(const uint8_t* data, size_t size);

    // Downscale an ARGB image to target dimensions (e.g. 160x90) for thumbnail cache
    static std::vector<uint32_t> resize(const uint32_t* src, int srcW, int srcH, int dstW, int dstH);
};

} // namespace yt
