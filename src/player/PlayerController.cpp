#include "player/PlayerController.h"
#include "core/Logger.h"
#include <algorithm>

namespace yt {

PlayerController& PlayerController::instance() {
    static PlayerController s_instance;
    return s_instance;
}

void PlayerController::setBackend(std::shared_ptr<IVideoPlayer> backend) {
    m_backend = backend;
    LOG_INFO("Player backend attached to controller");
}

bool PlayerController::play(const VideoSource& source) {
    m_currentSource = source;
    LOG_INFO("Starting playback: " + source.title + " (ID: " + source.videoId + ")");
    if (m_backend) {
        return m_backend->play(source);
    }
    return false;
}

void PlayerController::pause() {
    if (m_backend) {
        m_backend->pause();
    }
}

void PlayerController::resume() {
    if (m_backend) {
        m_backend->resume();
    }
}

void PlayerController::togglePlayPause() {
    if (!m_backend) return;
    PlaybackState s = m_backend->getState();
    if (s == PlaybackState::PLAYING) {
        pause();
    } else if (s == PlaybackState::PAUSED) {
        resume();
    } else if (s == PlaybackState::STOPPED || s == PlaybackState::IDLE) {
        if (!m_currentSource.streamUri.empty()) {
            play(m_currentSource);
        }
    }
}

void PlayerController::stop() {
    if (m_backend) {
        m_backend->stop();
    }
}

void PlayerController::seek(int seconds) {
    if (m_backend) {
        m_backend->seek(seconds);
    }
}

void PlayerController::seekRelative(int deltaSeconds) {
    if (m_backend) {
        m_backend->seekRelative(deltaSeconds);
    }
}

void PlayerController::setVolume(int vol) {
    if (m_backend) {
        m_backend->setVolume(vol);
    }
}

void PlayerController::setVideoOutputWindow(void* nativeWindowHandle) {
    if (m_backend) {
        m_backend->setVideoOutputWindow(nativeWindowHandle);
    }
}

void PlayerController::setVideoOutputBounds(int x, int y, int width, int height) {
    if (m_backend) {
        m_backend->setVideoOutputBounds(x, y, width, height);
    }
}

void PlayerController::setVideoVisible(bool visible) {
    if (m_backend) {
        m_backend->setVideoVisible(visible);
    }
}

PlaybackState PlayerController::getState() const {
    return m_backend ? m_backend->getState() : PlaybackState::IDLE;
}

int PlayerController::getCurrentPosition() const {
    return m_backend ? m_backend->getCurrentPosition() : 0;
}

int PlayerController::getDuration() const {
    return m_backend ? m_backend->getDuration() : 0;
}

int PlayerController::getVolume() const {
    return m_backend ? m_backend->getVolume() : 100;
}

void PlayerController::update() {
    // Poll or tick player state if needed
}

} // namespace yt
