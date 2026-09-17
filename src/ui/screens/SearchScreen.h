#pragma once

#include "ui/screens/IScreen.h"
#include "ui/components/SearchBar.h"
#include "youtube/IYouTubeClient.h"
#include "network/IHttpClient.h"
#include "core/ThreadPool.h"
#include <vector>
#include <memory>
#include <functional>

namespace yt {

class SearchScreen : public IScreen {
public:
    using VideoSelectCallback = std::function<void(const VideoItem& video)>;

    SearchScreen(std::shared_ptr<IYouTubeClient> client, std::shared_ptr<ThreadPool> pool, std::shared_ptr<IHttpClient> http = nullptr);

    void setVideoSelectCallback(VideoSelectCallback cb) { m_videoSelectCb = cb; }

    void onEnter() override;
    void render(IRenderer& renderer) override;
    bool handleEvent(const UIEvent& event) override;

    void performSearch(const std::string& query, bool isNextPage = false);

private:
    struct SearchCard {
        VideoItem item;
        Rect bounds;
        Rect thumbBounds;
        bool hovered{false};
    };

    std::shared_ptr<IYouTubeClient> m_client;
    std::shared_ptr<ThreadPool> m_pool;
    std::shared_ptr<IHttpClient> m_http;
    VideoSelectCallback m_videoSelectCb;

    SearchBar m_searchBar;
    std::vector<SearchCard> m_results;
    bool m_loading{false};
    std::string m_statusMessage;
    std::string m_nextPageToken;
    int m_scrollY{0};
    int m_maxScrollY{0};

    Rect m_loadMoreBounds{};
    bool m_loadMoreHovered{false};
};

} // namespace yt
