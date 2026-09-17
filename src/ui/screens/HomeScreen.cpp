#include "ui/screens/HomeScreen.h"
#include "cache/ThumbnailCache.h"
#include "network/HttpClientFactory.h"
#include "core/EventQueue.h"
#include "core/Config.h"
#include "core/Logger.h"
#include <algorithm>

namespace yt {

HomeScreen::HomeScreen(std::shared_ptr<IYouTubeClient> client, std::shared_ptr<ThreadPool> pool, std::shared_ptr<IHttpClient> http)
    : m_client(client), m_pool(pool), m_http(http) {
    if (!m_http) {
        m_http = HttpClientFactory::create();
    }
}

void HomeScreen::onEnter() {
    if (m_cards.empty()) {
        loadFeed();
    }
}

void HomeScreen::loadFeed() {
    m_loading = true;
    m_statusMessage = "Loading home feed...";

    auto client = m_client;
    auto pool = m_pool;
    auto http = m_http;
    int maxLimit = ConfigManager::instance().getConfig().maxHomeVideos;

    m_pool->enqueue([this, client, pool, http, maxLimit]() {
        auto result = client->getHomeFeed(maxLimit);

        EventQueue::instance().post([this, pool, http, result]() {
            m_loading = false;
            if (result.success) {
                m_cards.clear();
                for (const auto& item : result.data.items) {
                    CardLayout card;
                    card.item = item;
                    card.hovered = false;
                    m_cards.push_back(card);

                    // If not in cache, create placeholder then queue background real thumbnail download
                    if (!ThumbnailCache::instance().contains(item.id)) {
                        Color col = Color::fromHex(item.colorHex);
                        ThumbnailImage thumb = ThumbnailCache::createPlaceholder(160, 90, col, item.title);
                        ThumbnailCache::instance().put(item.id, thumb);

                        // Asynchronously download real YouTube thumbnail
                        std::string vidId = item.id;
                        pool->enqueue([vidId, http]() {
                            if (http) {
                                ThumbnailCache::instance().fetchAndStore(vidId, *http);
                            }
                        });
                    }
                }
                m_statusMessage = "";
            } else {
                m_statusMessage = "Failed to load feed: " + result.message;
                LOG_ERROR("Home feed load failed: " + result.message);
            }
        });
    });
}

void HomeScreen::render(IRenderer& renderer) {
    int screenW = renderer.getWidth();
    int screenH = renderer.getHeight();

    int startY = 65 - m_scrollY;
    int cardH = 95;
    int cardMargin = 12;
    int cardW = screenW - 60;

    // Header Title
    renderer.drawText("Home Feed (Live YouTube)", 30, startY - 20, Color{230, 230, 240, 255}, 16, true);

    if (m_loading) {
        renderer.drawText("Connecting to YouTube and loading feed...", 30, startY + 30, Color{180, 180, 190, 255}, 14);
        return;
    }

    if (!m_statusMessage.empty() && m_cards.empty()) {
        renderer.drawText(m_statusMessage, 30, startY + 30, Color{255, 100, 100, 255}, 14);
        return;
    }

    int curY = startY + 10;
    for (auto& card : m_cards) {
        card.bounds = Rect{30, curY, cardW, cardH};

        // Render card background
        Color bg = card.hovered ? Color{42, 42, 52, 255} : Color{28, 28, 34, 255};
        renderer.drawRect(card.bounds, bg, true);
        renderer.drawRect(card.bounds, Color{45, 45, 55, 255}, false);

        // Thumbnail (Real decoded JPEG or placeholder)
        card.thumbBounds = Rect{card.bounds.x + 6, card.bounds.y + 6, 140, 82};
        auto cachedThumb = ThumbnailCache::instance().get(card.item.id);
        if (cachedThumb && !cachedThumb->pixels.empty()) {
            renderer.drawImage(cachedThumb->pixels.data(), cachedThumb->width, cachedThumb->height, card.thumbBounds);
        } else {
            renderer.drawRect(card.thumbBounds, Color::fromHex(card.item.colorHex), true);
        }

        // Duration Badge on Thumbnail
        int badgeW = 45;
        int badgeH = 16;
        Rect badgeRect{card.thumbBounds.x + card.thumbBounds.width - badgeW - 3,
                       card.thumbBounds.y + card.thumbBounds.height - badgeH - 3,
                       badgeW, badgeH};
        renderer.drawRect(badgeRect, Color{10, 10, 12, 220}, true);
        Point durSize = renderer.measureText(card.item.duration, 11, true);
        renderer.drawText(card.item.duration,
                          badgeRect.x + (badgeW - durSize.x) / 2,
                          badgeRect.y + (badgeH - durSize.y) / 2,
                          Color{255, 255, 255, 255}, 11, true);

        // Video Title
        int textLeft = card.thumbBounds.x + card.thumbBounds.width + 16;
        renderer.drawText(card.item.title, textLeft, card.bounds.y + 12, Color{245, 245, 245, 255}, 14, true);

        // Channel Name
        renderer.drawText(card.item.channelTitle, textLeft, card.bounds.y + 36, Color{160, 160, 175, 255}, 12);

        // Metadata: Views & Published date
        std::string meta = std::to_string(card.item.viewCount) + " views • " + card.item.publishedAt;
        renderer.drawText(meta, textLeft, card.bounds.y + 56, Color{130, 130, 145, 255}, 11);

        curY += cardH + cardMargin;
    }

    int totalContentH = (curY + m_scrollY) - 65;
    m_maxScrollY = std::max(0, totalContentH - (screenH - 120));
}

bool HomeScreen::handleEvent(const UIEvent& event) {
    if (event.type == EventType::MOUSE_MOVE) {
        for (auto& card : m_cards) {
            card.hovered = card.bounds.contains(event.mouseX, event.mouseY);
        }
        return false;
    }

    if (event.type == EventType::MOUSE_DOWN && event.button == 0) {
        for (const auto& card : m_cards) {
            if (card.bounds.contains(event.mouseX, event.mouseY)) {
                if (m_videoSelectCb) {
                    m_videoSelectCb(card.item);
                }
                return true;
            }
        }
    }

    if (event.type == EventType::MOUSE_SCROLL) {
        m_scrollY = std::clamp(m_scrollY - event.scrollDelta * 25, 0, m_maxScrollY);
        return true;
    }

    return false;
}

} // namespace yt
