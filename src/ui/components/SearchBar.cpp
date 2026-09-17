#include "ui/components/SearchBar.h"
#include <algorithm>

namespace yt {

SearchBar::SearchBar() = default;

void SearchBar::render(IRenderer& renderer) {
    int buttonWidth = 80;
    Rect inputRect{m_bounds.x, m_bounds.y, m_bounds.width - buttonWidth - 8, m_bounds.height};
    m_buttonBounds = Rect{inputRect.x + inputRect.width + 8, m_bounds.y, buttonWidth, m_bounds.height};

    // Draw Input Field Box
    Color bg = m_focused ? Color{32, 32, 40, 255} : Color{25, 25, 30, 255};
    Color border = m_focused ? Color{70, 130, 240, 255} : Color{55, 55, 65, 255};
    renderer.drawRect(inputRect, bg, true);
    renderer.drawRect(inputRect, border, false);

    // Draw Text or Placeholder
    int textY = inputRect.y + (inputRect.height - 16) / 2;
    if (m_query.empty() && !m_focused) {
        renderer.drawText(m_placeholder, inputRect.x + 12, textY, Color{120, 120, 130, 255}, 14);
    } else {
        renderer.drawText(m_query, inputRect.x + 12, textY, Color{240, 240, 245, 255}, 14);
        if (m_focused) {
            Point p = renderer.measureText(m_query, 14);
            Rect caret{inputRect.x + 12 + p.x + 2, textY + 2, 2, 14};
            renderer.drawRect(caret, Color{255, 255, 255, 255}, true);
        }
    }

    // Draw Search Button
    Color btnBg = m_buttonHovered ? Color{70, 70, 85, 255} : Color{50, 50, 60, 255};
    renderer.drawRect(m_buttonBounds, btnBg, true);
    Point btnTextSize = renderer.measureText("Search", 13, true);
    renderer.drawText("Search",
                      m_buttonBounds.x + (buttonWidth - btnTextSize.x) / 2,
                      m_buttonBounds.y + (m_bounds.height - btnTextSize.y) / 2,
                      Color{240, 240, 240, 255}, 13, true);
}

bool SearchBar::handleEvent(const UIEvent& event) {
    if (event.type == EventType::MOUSE_MOVE) {
        m_buttonHovered = m_buttonBounds.contains(event.mouseX, event.mouseY);
        return false;
    }

    if (event.type == EventType::MOUSE_DOWN && event.button == 0) {
        Rect inputRect{m_bounds.x, m_bounds.y, m_bounds.width - 88, m_bounds.height};
        if (inputRect.contains(event.mouseX, event.mouseY)) {
            m_focused = true;
            return true;
        } else {
            m_focused = false;
        }

        if (m_buttonBounds.contains(event.mouseX, event.mouseY)) {
            if (m_callback && !m_query.empty()) {
                m_callback(m_query);
            }
            return true;
        }
    }

    if (m_focused) {
        if (event.type == EventType::KEY_DOWN) {
            if (event.keyCode == 8) { // Backspace
                if (!m_query.empty()) {
                    // Remove one whole UTF-8 character (continuation bytes are 10xxxxxx).
                    while (m_query.size() > 1 &&
                           (static_cast<unsigned char>(m_query.back()) & 0xC0) == 0x80) {
                        m_query.pop_back();
                    }
                    m_query.pop_back();
                    return true;
                }
            } else if (event.keyCode == 13) { // Enter
                if (m_callback && !m_query.empty()) {
                    m_callback(m_query);
                }
                return true;
            } else if (event.keyCode == 27) { // Escape
                m_focused = false;
                return true;
            }
        } else if (event.type == EventType::CHAR_INPUT) {
            unsigned char c = static_cast<unsigned char>(event.charCode);
            if ((c >= 32 && c <= 126) || c >= 0x80) { // ASCII or part of a UTF-8 sequence
                m_query.push_back(static_cast<char>(c));
                return true;
            }
        }
    }
    return false;
}

} // namespace yt
