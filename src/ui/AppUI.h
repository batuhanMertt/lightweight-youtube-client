#pragma once

#include "ui/IRenderer.h"
#include "ui/UIEvents.h"
#include "ui/components/NavigationBar.h"
#include "ui/screens/HomeScreen.h"
#include "ui/screens/SearchScreen.h"
#include "ui/screens/SubscriptionsScreen.h"
#include "ui/screens/VideoScreen.h"
#include "ui/screens/AccountScreen.h"
#include "youtube/IYouTubeClient.h"
#include "auth/IAuthService.h"
#include "network/IHttpClient.h"
#include "core/ThreadPool.h"
#include <memory>

namespace yt {

class AppUI {
public:
    AppUI(std::shared_ptr<IYouTubeClient> ytClient,
          std::shared_ptr<IAuthService> authService,
          std::shared_ptr<ThreadPool> threadPool,
          std::shared_ptr<IHttpClient> httpClient = nullptr);

    void switchScreen(ScreenType type);
    void playVideo(const VideoItem& item);

    void update(float dt);
    void render(IRenderer& renderer);
    bool handleEvent(const UIEvent& event);

    ScreenType getCurrentScreen() const { return m_currentScreenType; }

private:
    void renderStatusBar(IRenderer& renderer, int width, int height);

    std::shared_ptr<IYouTubeClient> m_ytClient;
    std::shared_ptr<IAuthService> m_authService;
    std::shared_ptr<ThreadPool> m_threadPool;
    std::shared_ptr<IHttpClient> m_http;

    NavigationBar m_navBar;
    std::unique_ptr<HomeScreen> m_homeScreen;
    std::unique_ptr<SearchScreen> m_searchScreen;
    std::unique_ptr<SubscriptionsScreen> m_subscriptionsScreen;
    std::unique_ptr<VideoScreen> m_videoScreen;
    std::unique_ptr<AccountScreen> m_accountScreen;

    ScreenType m_currentScreenType{ScreenType::HOME};
    IScreen* m_activeScreen{nullptr};
};

} // namespace yt
