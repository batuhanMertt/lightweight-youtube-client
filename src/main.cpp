#include "core/Logger.h"
#include "core/Config.h"
#include "core/PerformanceMonitor.h"
#include "core/ThreadPool.h"
#include "cache/ThumbnailCache.h"
#include "youtube/MockYouTubeClient.h"
#include "youtube/YouTubeClient.h"
#include "youtube/LiveYouTubeClient.h"
#include "network/HttpClientFactory.h"
#include "auth/AuthService.h"
#include "player/PlayerController.h"

#if defined(_WIN32)
#include "platform/windows/Win32Window.h"
#include "platform/windows/WindowsPlayerBackend.h"
#else
#include "platform/linux/LinuxWindow.h"
#include "platform/linux/GStreamerPlayerBackend.h"
#endif

#include "ui/AppUI.h"

#include <chrono>
#include <cstdlib>
#include <thread>
#include <memory>

int main(int /*argc*/, char* /*argv*/[]) {
    auto startTime = std::chrono::steady_clock::now();

    // 1. Initialize Logger
    LOG_INFO("==================================================");
    LOG_INFO("Starting Lightweight YouTube Client v1.0.0");
    LOG_INFO("Engine Policy: Native UI Only (No Chromium, No Electron, No WebView)");
    LOG_INFO("==================================================");

    // 2. Performance Tracking
    yt::PerformanceMonitor::instance().initialize();

    // 3. Load Configuration
    yt::ConfigManager::instance().loadFromFile("config/app_config.json");
    const auto& config = yt::ConfigManager::instance().getConfig();

    // 4. Configure Thumbnail Bounded Cache
    size_t cacheBytes = static_cast<size_t>(config.maxThumbnailCacheMb) * 1024 * 1024;
    yt::ThumbnailCache::instance().setMaxMemoryBytes(cacheBytes);

    // 5. Worker Thread Pool for Async I/O (Keeps UI Thread 100% Non-Blocking)
    auto threadPool = std::make_shared<yt::ThreadPool>(2);

    // 6. HTTP Client & YouTube Client Selection
    auto httpClient = yt::HttpClientFactory::create();
    std::shared_ptr<yt::IYouTubeClient> ytClient;
    if (config.useMockData) {
        ytClient = std::make_shared<yt::MockYouTubeClient>("assets");
    } else {
        ytClient = std::make_shared<yt::LiveYouTubeClient>(httpClient);
    }

    // 7. Auth Service
    auto authService = std::shared_ptr<yt::IAuthService>(&yt::AuthService::instance(), [](yt::IAuthService*){});

    // 8. Video Player Backend Setup
#if defined(_WIN32)
    auto playerBackend = std::make_shared<yt::WindowsPlayerBackend>();
#else
    auto playerBackend = std::make_shared<yt::GStreamerPlayerBackend>();
#endif
    yt::PlayerController::instance().setBackend(playerBackend);

    // 9. Native Window Creation
#if defined(_WIN32)
    auto window = std::make_unique<yt::Win32Window>();
#else
    auto window = std::make_unique<yt::LinuxWindow>();
#endif

    if (!window->create("Lightweight YouTube Client", 1024, 700)) {
        LOG_ERROR("Failed to initialize platform window. Terminating.");
        return 1;
    }

#if defined(_WIN32)
    yt::PlayerController::instance().setVideoOutputWindow(window->getVideoHwnd());
#endif

    // 10. App UI Coordinator with Live HTTP
    yt::AppUI ui(ytClient, authService, threadPool, httpClient);

    // Startup Time Measurement
    auto initEndTime = std::chrono::steady_clock::now();
    double startupMs = std::chrono::duration<double, std::milli>(initEndTime - startTime).count();
    yt::PerformanceMonitor::instance().recordStartupTime(startupMs);
    LOG_INFO("Application startup completed in " + std::to_string(startupMs) + " ms");
    LOG_INFO(yt::PerformanceMonitor::instance().getFormattedSummary());

    window->show();

    // Benchmark / scripting hook: YTC_AUTOPLAY=<videoId> opens that video right after start,
    // so tools/bench can measure playback without clicking through the UI.
    if (const char* autoplayId = std::getenv("YTC_AUTOPLAY")) {
        if (autoplayId[0] != '\0') {
            yt::VideoItem item;
            item.id = autoplayId;
            item.title = std::string("Video ") + autoplayId;
            LOG_INFO(std::string("YTC_AUTOPLAY: opening ") + autoplayId);
            ui.playVideo(item);
        }
    }
#if defined(_WIN32)
    // YTC_BENCH=1: keep the window above everything (its console window included). A covered
    // window stops presenting video frames, which would make the CPU numbers look too good.
    if (const char* bench = std::getenv("YTC_BENCH")) {
        if (bench[0] == '1') {
            SetWindowPos(window->getHwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
    }
#endif

    // 11. Main Loop with 30 FPS Cap for Low CPU / Low Power Profile
    const int targetFps = config.uiFps > 0 ? config.uiFps : 30;
    const std::chrono::milliseconds frameDuration(1000 / targetFps);
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (!window->shouldClose()) {
        auto frameStart = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(frameStart - lastFrameTime).count();
        lastFrameTime = frameStart;

        // Process OS input messages & pump events into UI
        if (!window->processEvents(ui)) {
            break;
        }

        // Update UI state & process async worker callbacks
        ui.update(dt);
        yt::PlayerController::instance().update();

        // Render Frame
        yt::IRenderer& renderer = window->getRenderer();
        ui.render(renderer);

        // Swap / Present Double Buffer
        window->present();

        // Frame rate limiter to save CPU cycles
        auto frameEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
        if (elapsed < frameDuration) {
            std::this_thread::sleep_for(frameDuration - elapsed);
        }
    }

    LOG_INFO("Shutting down application...");
    yt::PlayerController::instance().stop();
    threadPool->shutdown();
    window->close();

    LOG_INFO("Application terminated gracefully.");
    return 0;
}
