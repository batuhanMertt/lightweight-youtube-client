#pragma once

#include "ui/screens/IScreen.h"
#include "youtube/Models.h"
#include <functional>
#include <string>

namespace yt {

class VideoScreen : public IScreen {
public:
    using BackCallback = std::function<void()>;

    VideoScreen();

    void setBackCallback(BackCallback cb) { m_backCb = cb; }
    void loadVideo(const VideoItem& video);

    void onEnter() override;
    void onExit() override;
    void update(float dt) override;
    void render(IRenderer& renderer) override;
    bool handleEvent(const UIEvent& event) override;

    bool isFullscreen() const { return m_isFullscreen; }
    void toggleFullscreen();
    void setFullscreen(bool fs);

private:
    std::string formatTime(int seconds) const;
    int seekBarPositionAt(int mouseX) const;

    VideoItem m_video;
    BackCallback m_backCb;

    Rect m_backButtonBounds{};
    Rect m_playPauseBounds{};
    Rect m_stopBounds{};
    Rect m_rewindBounds{};
    Rect m_forwardBounds{};
    Rect m_muteBounds{};
    Rect m_fullscreenButtonBounds{};
    Rect m_seekBarBounds{};
    Rect m_canvasBounds{};

    bool m_backHovered{false};
    bool m_playHovered{false};
    bool m_stopHovered{false};
    bool m_rewindHovered{false};
    bool m_forwardHovered{false};
    bool m_muteHovered{false};
    bool m_fullscreenHovered{false};
    bool m_seeking{false};
    int m_seekPreviewPos{0}; // position shown while dragging; the real seek is sent on release
    bool m_isMuted{false};
    bool m_isFullscreen{false};
    float m_overlayTimer{3.0f};
    bool m_showOverlay{true};
};

} // namespace yt
