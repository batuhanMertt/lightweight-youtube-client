#include "ui/components/NavigationBar.h"

namespace yt {

NavigationBar::NavigationBar() {
    m_tabs = {
        {ScreenType::HOME, "Home", {}, false},
        {ScreenType::SUBSCRIPTIONS, "Subscriptions", {}, false},
        {ScreenType::SEARCH, "Search", {}, false},
        {ScreenType::ACCOUNT, "Account", {}, false}
    };
}

void NavigationBar::render(IRenderer& renderer, int width, int /*height*/) {
    // Render top bar background
    Rect barRect{0, 0, width, m_height};
    renderer.drawRect(barRect, Color{24, 24, 28, 255}, true);

    // App Logo / Title
    renderer.drawText("YouTube", 16, 14, Color{255, 60, 60, 255}, 16, true);

    // Render Tabs
    int startX = 220;
    int tabW = 120;
    int tabH = 34;
    int tabY = 7;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        auto& tab = m_tabs[i];
        tab.bounds = Rect{startX + static_cast<int>(i) * (tabW + 8), tabY, tabW, tabH};

        bool isSelected = (tab.type == m_currentScreen);
        Color tabBg = isSelected ? Color{60, 60, 75, 255} : (tab.hovered ? Color{45, 45, 55, 255} : Color{30, 30, 36, 255});
        renderer.drawRect(tab.bounds, tabBg, true);

        // Highlight line under active tab
        if (isSelected) {
            Rect lineRect{tab.bounds.x, tab.bounds.y + tabH - 3, tabW, 3};
            renderer.drawRect(lineRect, Color{255, 60, 60, 255}, true);
        }

        Color textColor = isSelected ? Color{255, 255, 255, 255} : Color{180, 180, 190, 255};
        Point textSize = renderer.measureText(tab.label, 13, isSelected);
        int textX = tab.bounds.x + (tabW - textSize.x) / 2;
        int textY = tab.bounds.y + (tabH - textSize.y) / 2;
        renderer.drawText(tab.label, textX, textY, textColor, 13, isSelected);
    }

    // Bottom border line
    Rect borderRect{0, m_height - 1, width, 1};
    renderer.drawRect(borderRect, Color{45, 45, 52, 255}, true);
}

bool NavigationBar::handleEvent(const UIEvent& event) {
    if (event.type == EventType::MOUSE_MOVE) {
        for (auto& tab : m_tabs) {
            tab.hovered = tab.bounds.contains(event.mouseX, event.mouseY);
        }
        return false;
    }

    if (event.type == EventType::MOUSE_DOWN && event.button == 0) {
        for (auto& tab : m_tabs) {
            if (tab.bounds.contains(event.mouseX, event.mouseY)) {
                if (m_currentScreen != tab.type) {
                    m_currentScreen = tab.type;
                    if (m_callback) {
                        m_callback(m_currentScreen);
                    }
                }
                return true;
            }
        }
    }
    return false;
}

} // namespace yt
