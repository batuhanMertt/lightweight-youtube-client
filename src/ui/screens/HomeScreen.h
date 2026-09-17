#pragma once

#include "ui/screens/IScreen.h"
#include "youtube/IYouTubeClient.h"
#include "network/IHttpClient.h"
#include "core/ThreadPool.h"
#include <vector>
#include <memory>
#include <functional>

namespace yt {

class HomeScreen : public IScreen {
public:
    using VideoSelectCallback = std::function<void(const VideoItem& video)>;

    HomeScreen(std::shared_ptr<IYouTubeClient> client, std::shared_ptr<ThreadPool> pool, std::shared_ptr<IHttpClient> http = nullptr);

    void setVideoSelectCallback(VideoSelectCallback cb) { m_videoSelectCb = cb; }

    void onEnter() override;
    void render(IRenderer& renderer) override;
    bool handleEvent(const UIEvent& event) override;

    void loadFeed();

private:
    struct CardLayout {
        VideoItem item;
        Rect bounds;
        Rect thumbBounds;
        bool hovered{false};
    };

    std::shared_ptr<IYouTubeClient> m_client;
    std::shared_ptr<ThreadPool> m_pool;
    std::shared_ptr<IHttpClient> m_http;
    VideoSelectCallback m_videoSelectCb;

    std::vector<CardLayout> m_cards;
    bool m_loading{false};
    std::string m_statusMessage{"Loading home feed..."};
    int m_scrollY{0};
    int m_maxScrollY{0};
};

} // namespace yt
