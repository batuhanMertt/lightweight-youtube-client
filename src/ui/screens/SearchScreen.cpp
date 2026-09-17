#include "ui/screens/SearchScreen.h"
#include "cache/ThumbnailCache.h"
#include "network/HttpClientFactory.h"
#include "core/EventQueue.h"
#include "core/Logger.h"
#include <algorithm>

namespace yt {

SearchScreen::SearchScreen(std::shared_ptr<IYouTubeClient> client, std::shared_ptr<ThreadPool> pool, std::shared_ptr<IHttpClient> http)
    : m_client(client), m_pool(pool), m_http(http) {
    if (!m_http) {
        m_http = HttpClientFactory::create();
    }
    m_searchBar.setCallback([this](const std::string& q) {
        performSearch(q, false);
    });
}

void SearchScreen::onEnter() {
    if (m_results.empty() && !m_searchBar.getQuery().empty()) {
        performSearch(m_searchBar.getQuery(), false);
    }
}

void SearchScreen::performSearch(const std::string& query, bool isNextPage) {
    m_loading = true;
    m_statusMessage = "Searching YouTube for: " + query + "...";
    auto client = m_client;
    auto pool = m_pool;
    auto http = m_http;
    std::string token = isNextPage ? m_nextPageToken : "";

    m_pool->enqueue([this, client, pool, http, query, token, isNextPage]() {
        auto result = client->search(query, token);

        EventQueue::instance().post([this, pool, http, result, isNextPage]() {
            m_loading = false;
            if (result.success) {
                if (!isNextPage) {
                    m_results.clear();
                }
                m_nextPageToken = result.data.nextPageToken;
                for (const auto& item : result.data.items) {
                    SearchCard card;
                    card.item = item;
                    card.hovered = false;
                    m_results.push_back(card);

                    // Real thumbnail background download
                    if (!ThumbnailCache::instance().contains(item.id)) {
                        Color col = Color::fromHex(item.colorHex);
                        ThumbnailImage thumb = ThumbnailCache::createPlaceholder(160, 90, col, item.title);
                        ThumbnailCache::instance().put(item.id, thumb);

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
                m_statusMessage = "Search error: " + result.message;
                LOG_ERROR("Search failed: " + result.message);
            }
        });
    });
}

void SearchScreen::render(IRenderer& renderer) {
    int screenW = renderer.getWidth();
    int screenH = renderer.getHeight();

    // Position Search Bar
    m_searchBar.setBounds(Rect{30, 60, std::min(600, screenW - 60), 36});
    m_searchBar.render(renderer);

    int startY = 115 - m_scrollY;
    int cardH = 95;
    int cardMargin = 10;
    int cardW = screenW - 60;

    if (m_loading && m_results.empty()) {
        renderer.drawText("Connecting and searching YouTube live...", 30, startY + 20, Color{180, 180, 190, 255}, 14);
        return;
    }

    if (!m_statusMessage.empty() && m_results.empty()) {
        renderer.drawText(m_statusMessage, 30, startY + 20, Color{255, 120, 120, 255}, 14);
        return;
    }

    int curY = startY;
    for (auto& card : m_results) {
        card.bounds = Rect{30, curY, cardW, cardH};

        Color bg = card.hovered ? Color{42, 42, 52, 255} : Color{28, 28, 34, 255};
        renderer.drawRect(card.bounds, bg, true);
        renderer.drawRect(card.bounds, Color{45, 45, 55, 255}, false);

        // Thumbnail
        card.thumbBounds = Rect{card.bounds.x + 6, card.bounds.y + 6, 140, 82};
        auto cachedThumb = ThumbnailCache::instance().get(card.item.id);
        if (cachedThumb && !cachedThumb->pixels.empty()) {
            renderer.drawImage(cachedThumb->pixels.data(), cachedThumb->width, cachedThumb->height, card.thumbBounds);
        } else {
            renderer.drawRect(card.thumbBounds, Color::fromHex(card.item.colorHex), true);
        }

        // Duration Badge
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

        // Info
        int textLeft = card.thumbBounds.x + card.thumbBounds.width + 16;
        renderer.drawText(card.item.title, textLeft, card.bounds.y + 12, Color{245, 245, 245, 255}, 14, true);
        renderer.drawText(card.item.channelTitle, textLeft, card.bounds.y + 36, Color{160, 160, 175, 255}, 12);
        std::string meta = std::to_string(card.item.viewCount) + " views • " + card.item.publishedAt;
        renderer.drawText(meta, textLeft, card.bounds.y + 56, Color{130, 130, 145, 255}, 11);

        curY += cardH + cardMargin;
    }

    // Pagination: Load More Results button
    if (!m_results.empty()) {
        m_loadMoreBounds = Rect{30, curY + 6, 200, 36};
        Color btnBg = m_loadMoreHovered ? Color{65, 65, 80, 255} : Color{45, 45, 55, 255};
        renderer.drawRect(m_loadMoreBounds, btnBg, true);
        renderer.drawRect(m_loadMoreBounds, Color{70, 70, 85, 255}, false);
        Point pSize = renderer.measureText("Load More Results", 13, true);
        renderer.drawText("Load More Results",
                          m_loadMoreBounds.x + (200 - pSize.x) / 2,
                          m_loadMoreBounds.y + (36 - pSize.y) / 2,
                          Color{240, 240, 250, 255}, 13, true);
        curY += 50;
    }

    int totalH = (curY + m_scrollY) - 115;
    m_maxScrollY = std::max(0, totalH - (screenH - 140));
}

bool SearchScreen::handleEvent(const UIEvent& event) {
    if (m_searchBar.handleEvent(event)) {
        return true;
    }

    if (event.type == EventType::MOUSE_MOVE) {
        for (auto& card : m_results) {
            card.hovered = card.bounds.contains(event.mouseX, event.mouseY);
        }
        m_loadMoreHovered = m_loadMoreBounds.contains(event.mouseX, event.mouseY);
        return false;
    }

    if (event.type == EventType::MOUSE_DOWN && event.button == 0) {
        for (const auto& card : m_results) {
            if (card.bounds.contains(event.mouseX, event.mouseY)) {
                if (m_videoSelectCb) {
                    m_videoSelectCb(card.item);
                }
                return true;
            }
        }

        if (m_loadMoreBounds.contains(event.mouseX, event.mouseY)) {
            performSearch(m_searchBar.getQuery(), true);
            return true;
        }
    }

    if (event.type == EventType::MOUSE_SCROLL) {
        m_scrollY = std::clamp(m_scrollY - event.scrollDelta * 25, 0, m_maxScrollY);
        return true;
    }

    return false;
}

} // namespace yt
