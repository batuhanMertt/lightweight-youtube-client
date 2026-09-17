#pragma once

#include "player/PlayerTypes.h"
#include <functional>

namespace yt {

class IVideoPlayer {
public:
    virtual ~IVideoPlayer() = default;

    virtual bool play(const VideoSource& source) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
    virtual void seek(int seconds) = 0;
    virtual void seekRelative(int seconds) { seek(getCurrentPosition() + seconds); }

    virtual PlaybackState getState() const = 0;
    virtual int getCurrentPosition() const = 0;
    virtual int getDuration() const = 0;
    virtual void setVolume(int volumePercent) = 0; // 0 to 100
    virtual int getVolume() const = 0;

    using StateCallback = std::function<void(PlaybackState newState)>;
    using PositionCallback = std::function<void(int currentSeconds, int totalSeconds)>;

    virtual void setStateCallback(StateCallback cb) = 0;
    virtual void setPositionCallback(PositionCallback cb) = 0;

    virtual void setVideoOutputWindow(void* /*nativeWindowHandle*/) {}
    virtual void setVideoOutputBounds(int /*x*/, int /*y*/, int /*width*/, int /*height*/) {}
    virtual void setVideoVisible(bool /*visible*/) {}
};

} // namespace yt
