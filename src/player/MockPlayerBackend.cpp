#include "player/MockPlayerBackend.h"
#include "core/Logger.h"

namespace yt {

MockPlayerBackend::MockPlayerBackend() {
    m_running = true;
    m_timerThread = std::thread(&MockPlayerBackend::timerLoop, this);
}

MockPlayerBackend::~MockPlayerBackend() {
    m_running = false;
    if (m_timerThread.joinable()) {
        m_timerThread.join();
    }
}

bool MockPlayerBackend::play(const VideoSource& source) {
    m_source = source;
    m_duration = source.durationSeconds > 0 ? source.durationSeconds : 180;
    m_currentPosition = 0;
    m_state = PlaybackState::PLAYING;

    LOG_INFO("MockPlayer playing: " + source.title + " (Duration: " + std::to_string(m_duration) + "s)");
    if (m_stateCb) m_stateCb(m_state);
    return true;
}

void MockPlayerBackend::pause() {
    if (m_state == PlaybackState::PLAYING) {
        m_state = PlaybackState::PAUSED;
        LOG_INFO("MockPlayer paused at " + std::to_string(m_currentPosition) + "s");
        if (m_stateCb) m_stateCb(m_state);
    }
}

void MockPlayerBackend::resume() {
    if (m_state == PlaybackState::PAUSED || m_state == PlaybackState::STOPPED) {
        m_state = PlaybackState::PLAYING;
        LOG_INFO("MockPlayer resumed at " + std::to_string(m_currentPosition) + "s");
        if (m_stateCb) m_stateCb(m_state);
    }
}

void MockPlayerBackend::stop() {
    m_state = PlaybackState::STOPPED;
    m_currentPosition = 0;
    LOG_INFO("MockPlayer stopped");
    if (m_stateCb) m_stateCb(m_state);
}

void MockPlayerBackend::seek(int seconds) {
    int target = std::clamp(seconds, 0, m_duration);
    m_currentPosition = target;
    LOG_INFO("MockPlayer seek to " + std::to_string(target) + "s");
    if (m_posCb) m_posCb(m_currentPosition, m_duration);
}

PlaybackState MockPlayerBackend::getState() const {
    return m_state;
}

int MockPlayerBackend::getCurrentPosition() const {
    return m_currentPosition;
}

int MockPlayerBackend::getDuration() const {
    return m_duration;
}

void MockPlayerBackend::setVolume(int volumePercent) {
    m_volume = std::clamp(volumePercent, 0, 100);
}

int MockPlayerBackend::getVolume() const {
    return m_volume;
}

void MockPlayerBackend::timerLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (m_state == PlaybackState::PLAYING) {
            int cur = m_currentPosition.load();
            if (cur < m_duration) {
                // Increment every 1000ms equivalent (4 * 250ms)
                static int subTick = 0;
                if (++subTick >= 4) {
                    subTick = 0;
                    m_currentPosition.fetch_add(1);
                    if (m_posCb) {
                        m_posCb(m_currentPosition.load(), m_duration);
                    }
                }
            } else {
                m_state = PlaybackState::STOPPED;
                if (m_stateCb) m_stateCb(m_state);
            }
        }
    }
}

} // namespace yt
