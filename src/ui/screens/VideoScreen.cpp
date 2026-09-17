#include "ui/screens/VideoScreen.h"
#include "cache/ThumbnailCache.h"
#include "player/PlayerController.h"
#include "core/Logger.h"

#include <iomanip>
#include <sstream>
#include <algorithm>

namespace yt {

VideoScreen::VideoScreen() = default;

void VideoScreen::onEnter() {
    PlayerController::instance().setVideoVisible(true);
    m_showOverlay = true;
    m_overlayTimer = 3.0f;
}

void VideoScreen::onExit() {
    m_isFullscreen = false;
    m_seeking = false;
    PlayerController::instance().setVideoVisible(false);
    PlayerController::instance().stop();
}

void VideoScreen::update(float dt) {
    if (m_isFullscreen) {
        if (m_overlayTimer > 0.0f) {
            m_overlayTimer -= dt;
            if (m_overlayTimer <= 0.0f) {
                m_showOverlay = false;
            }
        }
    } else {
        m_showOverlay = true;
    }
}

void VideoScreen::toggleFullscreen() {
    setFullscreen(!m_isFullscreen);
}

void VideoScreen::setFullscreen(bool fs) {
    m_isFullscreen = fs;
    m_showOverlay = true;
    m_overlayTimer = 3.0f;
}

void VideoScreen::loadVideo(const VideoItem& video) {
    m_video = video;
    VideoSource src;
    src.videoId = video.id;
    src.title = video.title;
    src.streamUri = "https://www.youtube.com/watch?v=" + video.id;

    int totalSec = 300;
    size_t colon = video.duration.find(':');
    if (colon != std::string::npos) {
        try {
            int m = std::stoi(video.duration.substr(0, colon));
            int s = std::stoi(video.duration.substr(colon + 1));
            totalSec = m * 60 + s;
        } catch (...) {}
    }
    src.durationSeconds = totalSec;

    PlayerController::instance().play(src);
}

int VideoScreen::seekBarPositionAt(int mouseX) const {
    if (m_seekBarBounds.width <= 0) return 0;
    float ratio = static_cast<float>(mouseX - m_seekBarBounds.x) / static_cast<float>(m_seekBarBounds.width);
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    return static_cast<int>(ratio * static_cast<float>(PlayerController::instance().getDuration()));
}

std::string VideoScreen::formatTime(int seconds) const {
    int m = seconds / 60;
    int s = seconds % 60;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << m << ":"
        << std::setfill('0') << std::setw(2) << s;
    return oss.str();
}

void VideoScreen::render(IRenderer& renderer) {
    int screenW = renderer.getWidth();
    int screenH = renderer.getHeight();

    PlaybackState state = PlayerController::instance().getState();
    int curPos = m_seeking ? m_seekPreviewPos : PlayerController::instance().getCurrentPosition();
    int duration = PlayerController::instance().getDuration();
    if (duration <= 0) duration = 1;
    curPos = std::clamp(curPos, 0, duration);

    if (m_isFullscreen) {
        // Fullscreen Mode: Video takes 100% of window
        m_canvasBounds = Rect{0, 0, screenW, screenH};
        // The video is a native child window drawn on top of everything we render, so while
        // the controls overlay is visible, shrink the video to leave the bottom bar uncovered.
        const int overlayReserve = m_showOverlay ? 96 : 0;
        PlayerController::instance().setVideoOutputBounds(0, 0, screenW, screenH - overlayReserve);
        PlayerController::instance().setVideoVisible(true);
        // AppUI doesn't clear the frame in fullscreen; paint the area around the video black.
        renderer.drawRect(Rect{0, 0, screenW, screenH}, Color{0, 0, 0, 255}, true);

        // If overlay is active (e.g. mouse moved), render floating controls
        if (m_showOverlay) {
            int barH = 56;
            int barW = screenW - 40;
            int barX = 20;
            int barY = screenH - barH - 20;
            Rect overlayBar{barX, barY, barW, barH};

            // Floating semi-transparent dark background
            renderer.drawRect(overlayBar, Color{15, 15, 20, 240}, true);
            renderer.drawRect(overlayBar, Color{60, 60, 75, 255}, false);

            // Floating Seek Bar
            m_seekBarBounds = Rect{barX + 16, barY + 8, barW - 32, 8};
            float progress = static_cast<float>(curPos) / static_cast<float>(duration);
            renderer.drawProgressBar(m_seekBarBounds, progress, Color{50, 50, 60, 255}, Color{255, 60, 60, 255});

            // Buttons row inside overlay
            int btnY = barY + 20;
            int curBtnX = barX + 16;
            int bH = 28;

            // Play/Pause
            int bW1 = 70;
            m_playPauseBounds = Rect{curBtnX, btnY, bW1, bH};
            renderer.drawButton(m_playPauseBounds, state == PlaybackState::PLAYING ? "Pause" : "Play", m_playHovered);
            curBtnX += bW1 + 8;

            // Stop
            int bW2 = 60;
            m_stopBounds = Rect{curBtnX, btnY, bW2, bH};
            renderer.drawButton(m_stopBounds, "Stop", m_stopHovered);
            curBtnX += bW2 + 8;

            // -10s
            int bW3 = 55;
            m_rewindBounds = Rect{curBtnX, btnY, bW3, bH};
            renderer.drawButton(m_rewindBounds, "-10s", m_rewindHovered);
            curBtnX += bW3 + 8;

            // +10s
            int bW4 = 55;
            m_forwardBounds = Rect{curBtnX, btnY, bW4, bH};
            renderer.drawButton(m_forwardBounds, "+10s", m_forwardHovered);
            curBtnX += bW4 + 8;

            // Mute
            int bW5 = 65;
            m_muteBounds = Rect{curBtnX, btnY, bW5, bH};
            renderer.drawButton(m_muteBounds, m_isMuted ? "Unmute" : "Mute", m_muteHovered);
            curBtnX += bW5 + 12;

            // Time readout
            std::string timeStr = formatTime(curPos) + " / " + formatTime(duration);
            renderer.drawText(timeStr, curBtnX, btnY + 7, Color{220, 220, 230, 255}, 12);

            // Exit Fullscreen Button
            int bW6 = 110;
            m_fullscreenButtonBounds = Rect{barX + barW - bW6 - 16, btnY, bW6, bH};
            Color fsBg = m_fullscreenHovered ? Color{75, 75, 95, 255} : Color{50, 50, 65, 255};
            renderer.drawRect(m_fullscreenButtonBounds, fsBg, true);
            renderer.drawRect(m_fullscreenButtonBounds, Color{80, 80, 100, 255}, false);
            Point fsTxt = renderer.measureText("Exit Fullscreen", 11, true);
            renderer.drawText("Exit Fullscreen",
                              m_fullscreenButtonBounds.x + (bW6 - fsTxt.x) / 2,
                              m_fullscreenButtonBounds.y + (bH - fsTxt.y) / 2,
                              Color{255, 255, 255, 255}, 11, true);

            // Title watermark in top-left
            renderer.drawText(m_video.title, 30, 24, Color{240, 240, 240, 220}, 14, true);
        } else {
            // Overlay hidden: zero out bounds so clicks don't hit invisible buttons
            m_backButtonBounds = Rect{};
            m_playPauseBounds = Rect{};
            m_stopBounds = Rect{};
            m_rewindBounds = Rect{};
            m_forwardBounds = Rect{};
            m_muteBounds = Rect{};
            m_fullscreenButtonBounds = Rect{};
            m_seekBarBounds = Rect{};
        }
        return;
    }

    // NORMAL MODE:
    // Top Navigation / Back Bar
    m_backButtonBounds = Rect{30, 56, 130, 32};
    Color backBg = m_backHovered ? Color{65, 65, 75, 255} : Color{45, 45, 55, 255};
    renderer.drawRect(m_backButtonBounds, backBg, true);
    renderer.drawRect(m_backButtonBounds, Color{70, 70, 80, 255}, false);
    Point backTxt = renderer.measureText("< Back to Feed", 12, true);
    renderer.drawText("< Back to Feed",
                      m_backButtonBounds.x + (130 - backTxt.x) / 2,
                      m_backButtonBounds.y + (32 - backTxt.y) / 2,
                      Color{240, 240, 240, 255}, 12, true);

    // EXACT 16:9 PROPORTIONAL VIDEO CANVAS
    int availW = screenW - 60;
    int availH = std::max(200, screenH - 240);
    int targetW = availW;
    int targetH = availW * 9 / 16;
    if (targetH > availH) {
        targetH = availH;
        targetW = targetH * 16 / 9;
    }
    int startX = (screenW - targetW) / 2;
    int startY = 96;
    m_canvasBounds = Rect{startX, startY, targetW, targetH};

    // Keep child HWND synchronized with exact 16:9 bounds
    PlayerController::instance().setVideoOutputBounds(m_canvasBounds.x, m_canvasBounds.y, m_canvasBounds.width, m_canvasBounds.height);
    PlayerController::instance().setVideoVisible(true);

    // Dark screen background
    renderer.drawRect(m_canvasBounds, Color{10, 10, 14, 255}, true);
    renderer.drawRect(m_canvasBounds, Color{50, 50, 60, 255}, false);

    // Draw thumbnail preview before playback has actually produced a frame (idle, still
    // connecting to the player, or stopped-at-start) so the canvas never flashes to a bare
    // black rectangle while mpv is spinning up.
    if (state == PlaybackState::IDLE || state == PlaybackState::BUFFERING ||
        (state == PlaybackState::STOPPED && curPos == 0)) {
        auto cachedThumb = ThumbnailCache::instance().get(m_video.id);
        if (cachedThumb && !cachedThumb->pixels.empty()) {
            int thumbW = std::min(320, targetW - 40);
            int thumbH = thumbW * 9 / 16;
            Rect centerThumb{m_canvasBounds.x + (targetW - thumbW) / 2, m_canvasBounds.y + (targetH - thumbH) / 2, thumbW, thumbH};
            renderer.drawImage(cachedThumb->pixels.data(), cachedThumb->width, cachedThumb->height, centerThumb);
            renderer.drawRect(centerThumb, Color{80, 80, 100, 255}, false);
        }
    }

    std::string stateBadge = "[" + std::string(playbackStateToString(state)) + "]";
    Color badgeColor = (state == PlaybackState::PLAYING) ? Color{50, 205, 50, 255} :
                       (state == PlaybackState::PAUSED)  ? Color{255, 165, 0, 255} :
                       (state == PlaybackState::BUFFERING) ? Color{100, 170, 255, 255} :
                       (state == PlaybackState::ERROR)   ? Color{220, 60, 60, 255} : Color{180, 180, 180, 255};
    renderer.drawText(stateBadge, m_canvasBounds.x + 12, m_canvasBounds.y + 12, badgeColor, 12, true);

    // Controls Panel directly below 16:9 Canvas
    int controlsY = m_canvasBounds.y + targetH + 10;

    // Seek Bar
    m_seekBarBounds = Rect{startX, controlsY, targetW, 8};
    float progress = static_cast<float>(curPos) / static_cast<float>(duration);
    renderer.drawProgressBar(m_seekBarBounds, progress, Color{45, 45, 55, 255}, Color{255, 60, 60, 255});

    // Control Buttons Row
    int btnRowY = controlsY + 16;
    int btnH = 32;
    int curX = startX;

    // 1. Play / Pause Button
    int btnW1 = 75;
    m_playPauseBounds = Rect{curX, btnRowY, btnW1, btnH};
    renderer.drawButton(m_playPauseBounds, state == PlaybackState::PLAYING ? "Pause" : "Play", m_playHovered);
    curX += btnW1 + 8;

    // 2. Stop Button
    int btnW2 = 65;
    m_stopBounds = Rect{curX, btnRowY, btnW2, btnH};
    renderer.drawButton(m_stopBounds, "Stop", m_stopHovered);
    curX += btnW2 + 8;

    // 3. Rewind -10s Button
    int btnW3 = 65;
    m_rewindBounds = Rect{curX, btnRowY, btnW3, btnH};
    renderer.drawButton(m_rewindBounds, "-10s", m_rewindHovered);
    curX += btnW3 + 8;

    // 4. Forward +10s Button
    int btnW4 = 65;
    m_forwardBounds = Rect{curX, btnRowY, btnW4, btnH};
    renderer.drawButton(m_forwardBounds, "+10s", m_forwardHovered);
    curX += btnW4 + 8;

    // 5. Mute / Unmute Button
    int btnW5 = 70;
    m_muteBounds = Rect{curX, btnRowY, btnW5, btnH};
    renderer.drawButton(m_muteBounds, m_isMuted ? "Unmute" : "Mute", m_muteHovered);
    curX += btnW5 + 14;

    // Time Readout
    std::string timeStr = formatTime(curPos) + " / " + formatTime(duration);
    renderer.drawText(timeStr, curX, btnRowY + 8, Color{200, 200, 210, 255}, 13);

    // 6. Fullscreen Button [Fullscreen]
    int btnW6 = 110;
    m_fullscreenButtonBounds = Rect{startX + targetW - btnW6, btnRowY, btnW6, btnH};
    Color fsBg = m_fullscreenHovered ? Color{65, 65, 80, 255} : Color{45, 45, 58, 255};
    renderer.drawRect(m_fullscreenButtonBounds, fsBg, true);
    renderer.drawRect(m_fullscreenButtonBounds, Color{70, 70, 85, 255}, false);
    Point fsTxt = renderer.measureText("Fullscreen", 12, true);
    renderer.drawText("Fullscreen",
                      m_fullscreenButtonBounds.x + (btnW6 - fsTxt.x) / 2,
                      m_fullscreenButtonBounds.y + (btnH - fsTxt.y) / 2,
                      Color{240, 240, 250, 255}, 12, true);

    // Video Metadata below controls
    int metaY = btnRowY + 42;
    renderer.drawText(m_video.title, startX, metaY, Color{250, 250, 250, 255}, 14, true);
    std::string subline = m_video.channelTitle + " • " + std::to_string(m_video.viewCount) + " views • " + m_video.publishedAt;
    renderer.drawText(subline, startX, metaY + 22, Color{160, 160, 175, 255}, 12);
}

bool VideoScreen::handleEvent(const UIEvent& event) {
    if (event.type == EventType::MOUSE_MOVE) {
        m_showOverlay = true;
        m_overlayTimer = 3.0f;

        m_backHovered = m_backButtonBounds.contains(event.mouseX, event.mouseY);
        m_playHovered = m_playPauseBounds.contains(event.mouseX, event.mouseY);
        m_stopHovered = m_stopBounds.contains(event.mouseX, event.mouseY);
        m_rewindHovered = m_rewindBounds.contains(event.mouseX, event.mouseY);
        m_forwardHovered = m_forwardBounds.contains(event.mouseX, event.mouseY);
        m_muteHovered = m_muteBounds.contains(event.mouseX, event.mouseY);
        m_fullscreenHovered = m_fullscreenButtonBounds.contains(event.mouseX, event.mouseY);

        if (m_seeking) {
            // Only move the preview while dragging. Sending a seek on every mouse-move event
            // flooded mpv with dozens of seeks per second and made the video stutter/freeze.
            m_seekPreviewPos = seekBarPositionAt(event.mouseX);
        }
        return false;
    }

    if (event.type == EventType::MOUSE_DOWN && event.button == 0) {
        m_showOverlay = true;
        m_overlayTimer = 3.0f;

        if (m_backButtonBounds.contains(event.mouseX, event.mouseY)) {
            if (m_backCb) {
                m_backCb();
            }
            return true;
        }

        if (m_fullscreenButtonBounds.contains(event.mouseX, event.mouseY)) {
            toggleFullscreen();
            return true;
        }

        if (m_playPauseBounds.contains(event.mouseX, event.mouseY)) {
            PlayerController::instance().togglePlayPause();
            return true;
        }

        if (m_stopBounds.contains(event.mouseX, event.mouseY)) {
            PlayerController::instance().stop();
            return true;
        }

        if (m_rewindBounds.contains(event.mouseX, event.mouseY)) {
            PlayerController::instance().seekRelative(-10);
            return true;
        }

        if (m_forwardBounds.contains(event.mouseX, event.mouseY)) {
            PlayerController::instance().seekRelative(10);
            return true;
        }

        if (m_muteBounds.contains(event.mouseX, event.mouseY)) {
            m_isMuted = !m_isMuted;
            PlayerController::instance().setVolume(m_isMuted ? 0 : 100);
            return true;
        }

        // Seek bar is only 8px tall - give it a few px of vertical slack so it's clickable.
        Rect seekHit{m_seekBarBounds.x, m_seekBarBounds.y - 5, m_seekBarBounds.width, m_seekBarBounds.height + 10};
        if (m_seekBarBounds.width > 0 && seekHit.contains(event.mouseX, event.mouseY)) {
            m_seeking = true;
            m_seekPreviewPos = seekBarPositionAt(event.mouseX);
            return true;
        }

        // Clicking directly on video canvas toggles play/pause
        if (m_canvasBounds.contains(event.mouseX, event.mouseY)) {
            PlayerController::instance().togglePlayPause();
            return true;
        }
    }

    if (event.type == EventType::MOUSE_DBLCLICK && event.button == 0) {
        if (m_canvasBounds.contains(event.mouseX, event.mouseY)) {
            // The first click of the double-click already toggled play/pause - undo it so
            // double-click only switches fullscreen.
            PlayerController::instance().togglePlayPause();
            toggleFullscreen();
            return true;
        }
    }

    if (event.type == EventType::MOUSE_UP && event.button == 0) {
        if (m_seeking) {
            m_seeking = false;
            m_seekPreviewPos = seekBarPositionAt(event.mouseX);
            PlayerController::instance().seek(m_seekPreviewPos);
            return true;
        }
    }

    if (event.type == EventType::KEY_DOWN) {
        if (event.keyCode == 32) { // Space -> Play/Pause
            PlayerController::instance().togglePlayPause();
            return true;
        } else if (event.keyCode == 70 || event.keyCode == 13) { // 'F' or Enter -> Fullscreen
            toggleFullscreen();
            return true;
        } else if (event.keyCode == 77) { // 'M' -> Mute
            m_isMuted = !m_isMuted;
            PlayerController::instance().setVolume(m_isMuted ? 0 : 100);
            return true;
        } else if (event.keyCode == 37) { // Left arrow
            PlayerController::instance().seekRelative(-10);
            return true;
        } else if (event.keyCode == 39) { // Right arrow
            PlayerController::instance().seekRelative(10);
            return true;
        } else if (event.keyCode == 27) { // Escape
            if (m_isFullscreen) {
                setFullscreen(false);
            } else {
                if (m_backCb) m_backCb();
            }
            return true;
        }
    }

    return false;
}

} // namespace yt
