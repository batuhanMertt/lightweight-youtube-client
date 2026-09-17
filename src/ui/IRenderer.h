#pragma once

#include "core/Types.h"
#include <string>

namespace yt {

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void clear(const Color& color) = 0;
    virtual void drawRect(const Rect& rect, const Color& color, bool fill = true) = 0;
    virtual void drawText(const std::string& text, int x, int y, const Color& color, int fontSize = 14, bool bold = false) = 0;
    virtual void drawImage(const uint32_t* pixels, int srcW, int srcH, const Rect& dstRect) = 0;
    virtual void drawProgressBar(const Rect& rect, float progress, const Color& bg, const Color& fg) = 0;
    virtual void drawButton(const Rect& rect, const std::string& text, bool hovered = false, bool active = false) = 0;

    virtual Point measureText(const std::string& text, int fontSize = 14, bool bold = false) = 0;

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};

} // namespace yt
