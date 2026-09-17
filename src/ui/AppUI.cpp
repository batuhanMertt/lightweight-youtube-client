#include "ui/AppUI.h"
#include "core/EventQueue.h"
#include "core/PerformanceMonitor.h"
#include "network/HttpClientFactory.h"
#include "core/Logger.h"

namespace yt {

AppUI::AppUI(std::shared_ptr<IYouTubeClient> ytClient,
             std::shared_ptr<IAuthService> authService,
             std::shared_ptr<ThreadPool> threadPool,
             std::shared_ptr<IHttpClient> httpClient)
    : m_ytClient(ytClient), m_authService(authService), m_threadPool(threadPool), m_http(httpClient) {

    if (!m_http) {
        m_http = HttpClientFactory::create();
    }

    m_homeScreen = std::make_unique<HomeScreen>(m_ytClient, m_threadPool, m_http);
    m_searchScreen = std::make_unique<SearchScreen>(m_ytClient, m_threadPool, m_http);
    m_subscriptionsScreen = std::make_unique<SubscriptionsScreen>(m_ytClient, m_threadPool);
    m_videoScreen = std::make_unique<VideoScreen>();
    m_accountScreen = std::make_unique<AccountScreen>(m_authService, m_threadPool);

    // Setup Video Selection Callbacks to switch to Video Screen
    auto onVideoSelect = [this](const VideoItem& v) {
        playVideo(v);
    };

    m_homeScreen->setVideoSelectCallback(onVideoSelect);
    m_searchScreen->setVideoSelectCallback(onVideoSelect);
    m_subscriptionsScreen->setVideoSelectCallback(onVideoSelect);

    m_videoScreen->setBackCallback([this]() {
        switchScreen(ScreenType::HOME);
    });

    m_navBar.setTabCallback([this](ScreenType screen) {
        switchScreen(screen);
    });

    m_activeScreen = m_homeScreen.get();
    m_activeScreen->onEnter();
}

void AppUI::switchScreen(ScreenType type) {
    if (m_activeScreen) {
        m_activeScreen->onExit();
    }

    m_currentScreenType = type;
    m_navBar.setSelectedScreen(type);

    switch (type) {
        case ScreenType::HOME:
            m_activeScreen = m_homeScreen.get();
            break;
        case ScreenType::SEARCH:
            m_activeScreen = m_searchScreen.get();
            break;
        case ScreenType::SUBSCRIPTIONS:
            m_activeScreen = m_subscriptionsScreen.get();
            break;
        case ScreenType::VIDEO:
            m_activeScreen = m_videoScreen.get();
            break;
        case ScreenType::ACCOUNT:
            m_activeScreen = m_accountScreen.get();
            break;
    }

    if (m_activeScreen) {
        m_activeScreen->onEnter();
    }
}

void AppUI::playVideo(const VideoItem& item) {
    switchScreen(ScreenType::VIDEO);
    m_videoScreen->loadVideo(item);
}

void AppUI::update(float dt) {
    EventQueue::instance().processPending();

    if (m_activeScreen) {
        m_activeScreen->update(dt);
    }
}

void AppUI::render(IRenderer& renderer) {
    int w = renderer.getWidth();
    int h = renderer.getHeight();

    bool isFs = (m_currentScreenType == ScreenType::VIDEO && m_videoScreen && m_videoScreen->isFullscreen());

    if (!isFs) {
        renderer.clear(Color{18, 18, 22, 255});
    }

    if (m_activeScreen) {
        m_activeScreen->render(renderer);
    }

    // Only render top navigation bar and bottom status bar when NOT in fullscreen video mode
    if (!isFs) {
        m_navBar.render(renderer, w, h);
        renderStatusBar(renderer, w, h);
    }
}

void AppUI::renderStatusBar(IRenderer& renderer, int width, int height) {
    int barH = 26;
    Rect statusRect{0, height - barH, width, barH};
    renderer.drawRect(statusRect, Color{15, 15, 18, 255}, true);
    renderer.drawRect(Rect{0, height - barH, width, 1}, Color{35, 35, 42, 255}, true);

    renderer.drawText("Native C++ client | No browser engine", 12, height - barH + 7, Color{130, 140, 160, 255}, 11);

    std::string perf = PerformanceMonitor::instance().getFormattedSummary();
    Point pSize = renderer.measureText(perf, 11, true);
    renderer.drawText(perf, width - pSize.x - 16, height - barH + 7, Color{100, 220, 120, 255}, 11, true);
}

bool AppUI::handleEvent(const UIEvent& event) {
    bool isFs = (m_currentScreenType == ScreenType::VIDEO && m_videoScreen && m_videoScreen->isFullscreen());

    // In fullscreen video mode, the active screen handles all events first
    if (isFs && m_activeScreen) {
        return m_activeScreen->handleEvent(event);
    }

    if (m_navBar.handleEvent(event)) {
        return true;
    }

    if (m_activeScreen && m_activeScreen->handleEvent(event)) {
        return true;
    }

    return false;
}

} // namespace yt
