#pragma once

#include "ui/IRenderer.h"
#include "ui/UIEvents.h"
#include <string>
#include <functional>

namespace yt {

class SearchBar {
public:
    using SearchCallback = std::function<void(const std::string& query)>;

    SearchBar();

    void setQuery(const std::string& query) { m_query = query; }
    const std::string& getQuery() const { return m_query; }
    void setCallback(SearchCallback cb) { m_callback = cb; }

    void setBounds(const Rect& bounds) { m_bounds = bounds; }
    void render(IRenderer& renderer);
    bool handleEvent(const UIEvent& event);

private:
    Rect m_bounds{40, 65, 500, 36};
    std::string m_query;
    std::string m_placeholder{"Search YouTube..."};
    bool m_focused{false};
    bool m_buttonHovered{false};
    SearchCallback m_callback;
    Rect m_buttonBounds{};
};

} // namespace yt
