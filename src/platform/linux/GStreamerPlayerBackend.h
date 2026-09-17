#pragma once

#include "player/IVideoPlayer.h"
#include <string>

namespace yt {

class GStreamerPlayerBackend : public IVideoPlayer {
public:
    GStreamerPlayerBackend();
    ~GStreamerPlayerBackend() override;

    bool play(const VideoSource& source) override;
    void pause() override;
    void resume() override;
    void stop() override;
    void seek(int seconds) override;

    PlaybackState getState() const override { return m_state; }
    int getCurrentPosition() const override { return m_currentPosition; }
    int getDuration() const override { return m_duration; }
    void setVolume(int volumePercent) override { m_volume = volumePercent; }
    int getVolume() const override { return m_volume; }

    void setStateCallback(StateCallback cb) override { m_stateCb = cb; }
    void setPositionCallback(PositionCallback cb) override { m_posCb = cb; }

    // GStreamer pipeline string builder for embedded hardware decoding
    std::string buildHardwarePipeline(const std::string& uri) const;

private:
    PlaybackState m_state{PlaybackState::IDLE};
    VideoSource m_source;
    int m_currentPosition{0};
    int m_duration{0};
    int m_volume{100};

    StateCallback m_stateCb;
    PositionCallback m_posCb;
};

} // namespace yt
