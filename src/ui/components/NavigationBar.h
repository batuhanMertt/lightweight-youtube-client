#pragma once

#include "ui/IRenderer.h"
#include "ui/UIEvents.h"
#include <functional>
#include <vector>

namespace yt {

class NavigationBar {
public:
    using TabCallback = std::function<void(ScreenType selectedScreen)>;

    NavigationBar();

    void setTabCallback(TabCallback cb) { m_callback = cb; }
    void setSelectedScreen(ScreenType screen) { m_currentScreen = screen; }
    ScreenType getSelectedScreen() const { return m_currentScreen; }

    void render(IRenderer& renderer, int width, int height);
    bool handleEvent(const UIEvent& event);

private:
    struct TabItem {
        ScreenType type;
        std::string label;
        Rect bounds;
        bool hovered{false};
    };

    std::vector<TabItem> m_tabs;
    ScreenType m_currentScreen{ScreenType::HOME};
    TabCallback m_callback;
    int m_height{48};
};

} // namespace yt
