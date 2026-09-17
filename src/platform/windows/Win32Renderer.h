#pragma once

#include "ui/IRenderer.h"
#if defined(_WIN32)
#include <windows.h>
#include <unordered_map>

namespace yt {

class Win32Renderer : public IRenderer {
public:
    Win32Renderer();
    ~Win32Renderer() override;

    void initialize(HWND hwnd, int width, int height);
    void resize(int width, int height);
    void beginFrame();
    void endFrame(HDC targetDC);

    void clear(const Color& color) override;
    void drawRect(const Rect& rect, const Color& color, bool fill = true) override;
    void drawText(const std::string& text, int x, int y, const Color& color, int fontSize = 14, bool bold = false) override;
    void drawImage(const uint32_t* pixels, int srcW, int srcH, const Rect& dstRect) override;
    void drawProgressBar(const Rect& rect, float progress, const Color& bg, const Color& fg) override;
    void drawButton(const Rect& rect, const std::string& text, bool hovered = false, bool active = false) override;

    Point measureText(const std::string& text, int fontSize = 14, bool bold = false) override;

    int getWidth() const override { return m_width; }
    int getHeight() const override { return m_height; }

    HDC getMemDC() const { return m_memDC; }

private:
    HFONT getFont(int size, bool bold);

    HWND m_hwnd{nullptr};
    HDC m_memDC{nullptr};
    HBITMAP m_memBmp{nullptr};
    HBITMAP m_oldBmp{nullptr};
    int m_width{0};
    int m_height{0};

    std::unordered_map<int, HFONT> m_fontCache;
};

} // namespace yt
#endif
