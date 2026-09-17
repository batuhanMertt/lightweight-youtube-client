#include "ui/screens/SubscriptionsScreen.h"
#include "cache/ThumbnailCache.h"
#include "core/EventQueue.h"
#include "core/Config.h"
#include "core/Logger.h"
#include <algorithm>

namespace yt {

SubscriptionsScreen::SubscriptionsScreen(std::shared_ptr<IYouTubeClient> client, std::shared_ptr<ThreadPool> pool)
    : m_client(client), m_pool(pool) {
}

void SubscriptionsScreen::onEnter() {
    if (m_channels.empty()) {
        loadSubscriptions();
    }
}

void SubscriptionsScreen::loadSubscriptions() {
    m_loading = true;
    m_statusMessage = "Loading subscriptions...";
    auto client = m_client;
    int maxChannels = 10;

    m_pool->enqueue([this, client, maxChannels]() {
        auto result = client->getSubscriptions(maxChannels);

        EventQueue::instance().post([this, result]() {
            m_loading = false;
            if (result.success) {
                m_channels.clear();
                for (const auto& ch : result.data) {
                    ChannelGroup grp;
                    grp.channel = ch;
                    for (const auto& v : ch.recentVideos) {
                        SubVideoCard vc;
                        vc.item = v;
                        vc.hovered = false;
                        grp.videoCards.push_back(vc);

                        if (!ThumbnailCache::instance().contains(v.id)) {
                            ThumbnailImage thumb = ThumbnailCache::createPlaceholder(160, 90, Color::fromHex(v.colorHex), v.title);
                            ThumbnailCache::instance().put(v.id, thumb);
                        }
                    }
                    m_channels.push_back(grp);
                }
                m_statusMessage = "";
            } else {
                m_statusMessage = "Subscriptions: " + result.message;
            }
        });
    });
}

void SubscriptionsScreen::render(IRenderer& renderer) {
    int screenW = renderer.getWidth();
    int screenH = renderer.getHeight();

    int startY = 65 - m_scrollY;
    renderer.drawText("Subscribed Channels & Recent Uploads", 30, startY - 20, Color{230, 230, 240, 255}, 16, true);

    if (m_loading) {
        renderer.drawText("Fetching subscriptions...", 30, startY + 30, Color{180, 180, 190, 255}, 14);
        return;
    }

    if (!m_statusMessage.empty() && m_channels.empty()) {
        renderer.drawText(m_statusMessage, 30, startY + 30, Color{255, 120, 120, 255}, 14);
        return;
    }

    int curY = startY + 10;
    int contentW = screenW - 60;

    for (auto& group : m_channels) {
        // Channel Header Bar
        group.headerBounds = Rect{30, curY, contentW, 36};
        renderer.drawRect(group.headerBounds, Color{36, 36, 46, 255}, true);
        renderer.drawRect(group.headerBounds, Color{55, 55, 70, 255}, false);

        std::string chanText = "Channel: " + group.channel.channelTitle + " (" + group.channel.subscriberCount + " subscribers)";
        renderer.drawText(chanText, 45, curY + 10, Color{255, 215, 0, 255}, 13, true);

        curY += 42;

        // Recent videos under this channel
        for (auto& card : group.videoCards) {
            int cardH = 72;
            card.bounds = Rect{50, curY, contentW - 20, cardH};

            Color bg = card.hovered ? Color{45, 45, 55, 255} : Color{26, 26, 32, 255};
            renderer.drawRect(card.bounds, bg, true);
            renderer.drawRect(card.bounds, Color{40, 40, 50, 255}, false);

            // Small thumb
            Rect thumbRect{card.bounds.x + 6, card.bounds.y + 6, 105, 60};
            auto cachedThumb = ThumbnailCache::instance().get(card.item.id);
            if (cachedThumb && !cachedThumb->pixels.empty()) {
                renderer.drawImage(cachedThumb->pixels.data(), cachedThumb->width, cachedThumb->height, thumbRect);
            } else {
                renderer.drawRect(thumbRect, Color::fromHex(card.item.colorHex), true);
            }

            int textLeft = thumbRect.x + thumbRect.width + 12;
            renderer.drawText(card.item.title, textLeft, card.bounds.y + 12, Color{240, 240, 240, 255}, 13, true);
            std::string subMeta = card.item.duration + " • " + std::to_string(card.item.viewCount) + " views • " + card.item.publishedAt;
            renderer.drawText(subMeta, textLeft, card.bounds.y + 36, Color{150, 150, 160, 255}, 11);

            curY += cardH + 8;
        }

        curY += 12;
    }

    int totalH = (curY + m_scrollY) - 65;
    m_maxScrollY = std::max(0, totalH - (screenH - 120));
}

bool SubscriptionsScreen::handleEvent(const UIEvent& event) {
    if (event.type == EventType::MOUSE_MOVE) {
        for (auto& group : m_channels) {
            for (auto& card : group.videoCards) {
                card.hovered = card.bounds.contains(event.mouseX, event.mouseY);
            }
        }
        return false;
    }

    if (event.type == EventType::MOUSE_DOWN && event.button == 0) {
        for (auto& group : m_channels) {
            for (auto& card : group.videoCards) {
                if (card.bounds.contains(event.mouseX, event.mouseY)) {
                    if (m_videoSelectCb) {
                        m_videoSelectCb(card.item);
                    }
                    return true;
                }
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
