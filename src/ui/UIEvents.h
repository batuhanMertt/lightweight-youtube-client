#pragma once

#include "core/Types.h"
#include <string>

namespace yt {

enum class EventType {
    NONE,
    MOUSE_MOVE,
    MOUSE_DOWN,
    MOUSE_UP,
    MOUSE_DBLCLICK,
    KEY_DOWN,
    CHAR_INPUT,
    MOUSE_SCROLL,
    WINDOW_RESIZE
};

struct UIEvent {
    EventType type{EventType::NONE};
    int mouseX{0};
    int mouseY{0};
    int button{0}; // 0 = left, 1 = right
    int keyCode{0};
    char charCode{0};
    int scrollDelta{0};
    int windowWidth{0};
    int windowHeight{0};
};

} // namespace yt
