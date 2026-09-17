#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace yt {

enum class ScreenType {
    HOME,
    SEARCH,
    SUBSCRIPTIONS,
    VIDEO,
    ACCOUNT
};

struct Color {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};

    static Color fromHex(const std::string& hex) {
        if (hex.empty() || hex[0] != '#') return Color{40, 40, 40, 255};
        uint32_t val = 0;
        try {
            val = std::stoul(hex.substr(1), nullptr, 16);
        } catch (...) {
            return Color{40, 40, 40, 255};
        }
        if (hex.length() == 7) {
            return Color{
                static_cast<uint8_t>((val >> 16) & 0xFF),
                static_cast<uint8_t>((val >> 8) & 0xFF),
                static_cast<uint8_t>(val & 0xFF),
                255
            };
        }
        return Color{40, 40, 40, 255};
    }
};

struct Rect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    bool contains(int px, int py) const {
        return px >= x && px < (x + width) && py >= y && py < (y + height);
    }
};

struct Point {
    int x{0};
    int y{0};
};

enum class ErrorCode {
    OK = 0,
    NETWORK_UNAVAILABLE,
    NETWORK_TIMEOUT,
    HTTP_ERROR,
    API_ERROR,
    AUTHENTICATION_EXPIRED,
    INVALID_TOKEN,
    NO_SEARCH_RESULT,
    PLAYBACK_UNAVAILABLE,
    DECODER_ERROR,
    FILE_NOT_FOUND,
    JSON_PARSE_ERROR
};

inline const char* errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "OK";
        case ErrorCode::NETWORK_UNAVAILABLE: return "Network unavailable";
        case ErrorCode::NETWORK_TIMEOUT: return "Network timeout";
        case ErrorCode::HTTP_ERROR: return "HTTP error";
        case ErrorCode::API_ERROR: return "API error";
        case ErrorCode::AUTHENTICATION_EXPIRED: return "Authentication expired";
        case ErrorCode::INVALID_TOKEN: return "Invalid token";
        case ErrorCode::NO_SEARCH_RESULT: return "No results found";
        case ErrorCode::PLAYBACK_UNAVAILABLE: return "Playback unavailable";
        case ErrorCode::DECODER_ERROR: return "Decoder error";
        case ErrorCode::FILE_NOT_FOUND: return "File not found";
        case ErrorCode::JSON_PARSE_ERROR: return "JSON parse error";
        default: return "Unknown error";
    }
}

template <typename T>
struct Result {
    bool success{false};
    T data{};
    ErrorCode error{ErrorCode::OK};
    std::string message{};

    static Result<T> Ok(const T& val) {
        Result<T> r;
        r.success = true;
        r.data = val;
        r.error = ErrorCode::OK;
        return r;
    }

    static Result<T> Fail(ErrorCode err, const std::string& msg = "") {
        Result<T> r;
        r.success = false;
        r.error = err;
        r.message = msg.empty() ? errorCodeToString(err) : msg;
        return r;
    }
};

} // namespace yt
