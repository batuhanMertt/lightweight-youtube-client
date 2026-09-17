#pragma once

#include "player/IVideoPlayer.h"
#include <chrono>
#include <atomic>
#include <thread>

namespace yt {

class MockPlayerBackend : public IVideoPlayer {
public:
    MockPlayerBackend();
    ~MockPlayerBackend() override;

    bool play(const VideoSource& source) override;
    void pause() override;
    void resume() override;
    void stop() override;
    void seek(int seconds) override;

    PlaybackState getState() const override;
    int getCurrentPosition() const override;
    int getDuration() const override;
    void setVolume(int volumePercent) override;
    int getVolume() const override;

    void setStateCallback(StateCallback cb) override { m_stateCb = cb; }
    void setPositionCallback(PositionCallback cb) override { m_posCb = cb; }

private:
    void timerLoop();

    PlaybackState m_state{PlaybackState::IDLE};
    VideoSource m_source;
    std::atomic<int> m_currentPosition{0};
    int m_duration{180}; // seconds
    int m_volume{80};

    StateCallback m_stateCb;
    PositionCallback m_posCb;

    std::atomic<bool> m_running{false};
    std::thread m_timerThread;
};

} // namespace yt
