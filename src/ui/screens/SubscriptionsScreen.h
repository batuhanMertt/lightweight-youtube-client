#pragma once

#include "ui/screens/IScreen.h"
#include "youtube/IYouTubeClient.h"
#include "core/ThreadPool.h"
#include <vector>
#include <memory>
#include <functional>

namespace yt {

class SubscriptionsScreen : public IScreen {
public:
    using VideoSelectCallback = std::function<void(const VideoItem& video)>;

    SubscriptionsScreen(std::shared_ptr<IYouTubeClient> client, std::shared_ptr<ThreadPool> pool);

    void setVideoSelectCallback(VideoSelectCallback cb) { m_videoSelectCb = cb; }

    void onEnter() override;
    void render(IRenderer& renderer) override;
    bool handleEvent(const UIEvent& event) override;

    void loadSubscriptions();

private:
    struct SubVideoCard {
        VideoItem item;
        Rect bounds;
        bool hovered{false};
    };

    struct ChannelGroup {
        ChannelItem channel;
        Rect headerBounds;
        std::vector<SubVideoCard> videoCards;
    };

    std::shared_ptr<IYouTubeClient> m_client;
    std::shared_ptr<ThreadPool> m_pool;
    VideoSelectCallback m_videoSelectCb;

    std::vector<ChannelGroup> m_channels;
    bool m_loading{false};
    std::string m_statusMessage;
    int m_scrollY{0};
    int m_maxScrollY{0};
};

} // namespace yt
