#pragma once

#include <string>

#if defined(_WIN32)
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace yt {

enum class PlaybackState {
    IDLE,
    BUFFERING,
    PLAYING,
    PAUSED,
    STOPPED,
    ERROR
};

inline const char* playbackStateToString(PlaybackState state) {
    switch (state) {
        case PlaybackState::IDLE: return "IDLE";
        case PlaybackState::BUFFERING: return "BUFFERING";
        case PlaybackState::PLAYING: return "PLAYING";
        case PlaybackState::PAUSED: return "PAUSED";
        case PlaybackState::STOPPED: return "STOPPED";
        case PlaybackState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

struct VideoSource {
    std::string videoId;
    std::string streamUri;
    std::string title;
    int durationSeconds{0};
    std::string resolution{"720p"};
};

} // namespace yt
