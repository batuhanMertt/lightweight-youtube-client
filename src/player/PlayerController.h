#pragma once

#include "player/IVideoPlayer.h"
#include <memory>

namespace yt {

class PlayerController {
public:
    static PlayerController& instance();

    void setBackend(std::shared_ptr<IVideoPlayer> backend);
    std::shared_ptr<IVideoPlayer> getBackend() const { return m_backend; }

    bool play(const VideoSource& source);
    void pause();
    void resume();
    void togglePlayPause();
    void stop();
    void seek(int seconds);
    void seekRelative(int deltaSeconds);
    void setVolume(int vol);

    void setVideoOutputWindow(void* nativeWindowHandle);
    void setVideoOutputBounds(int x, int y, int width, int height);
    void setVideoVisible(bool visible);

    PlaybackState getState() const;
    int getCurrentPosition() const;
    int getDuration() const;
    int getVolume() const;
    const VideoSource& getCurrentSource() const { return m_currentSource; }

    void update(); // Called by UI loop to tick time/events

private:
    PlayerController() = default;
    std::shared_ptr<IVideoPlayer> m_backend;
    VideoSource m_currentSource;
};

} // namespace yt
