#include "platform/windows/Win32Renderer.h"
#if defined(_WIN32)
#include <algorithm>
#include <vector>

namespace yt {

static std::wstring utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size > 1) {
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
        if (!wstr.empty() && wstr.back() == L'\0') {
            wstr.pop_back();
        }
        return wstr;
    }
    // Fallback to ANSI code page if UTF-8 conversion fails
    size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    if (size > 1) {
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], size);
        if (!wstr.empty() && wstr.back() == L'\0') {
            wstr.pop_back();
        }
        return wstr;
    }
    return std::wstring(str.begin(), str.end());
}

Win32Renderer::Win32Renderer() = default;

Win32Renderer::~Win32Renderer() {
    if (m_memDC) {
        SelectObject(m_memDC, m_oldBmp);
        DeleteObject(m_memBmp);
        DeleteDC(m_memDC);
    }
    for (auto& pair : m_fontCache) {
        DeleteObject(pair.second);
    }
}

void Win32Renderer::initialize(HWND hwnd, int width, int height) {
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    HDC screenDC = GetDC(hwnd);
    m_memDC = CreateCompatibleDC(screenDC);
    m_memBmp = CreateCompatibleBitmap(screenDC, width, height);
    m_oldBmp = (HBITMAP)SelectObject(m_memDC, m_memBmp);
    ReleaseDC(hwnd, screenDC);
}

void Win32Renderer::resize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == m_width && height == m_height)) {
        return;
    }
    m_width = width;
    m_height = height;

    if (m_memDC) {
        SelectObject(m_memDC, m_oldBmp);
        DeleteObject(m_memBmp);

        HDC screenDC = GetDC(m_hwnd);
        m_memBmp = CreateCompatibleBitmap(screenDC, width, height);
        m_oldBmp = (HBITMAP)SelectObject(m_memDC, m_memBmp);
        ReleaseDC(m_hwnd, screenDC);
    }
}

void Win32Renderer::beginFrame() {
}

void Win32Renderer::endFrame(HDC targetDC) {
    if (m_memDC && targetDC) {
        BitBlt(targetDC, 0, 0, m_width, m_height, m_memDC, 0, 0, SRCCOPY);
    }
}

void Win32Renderer::clear(const Color& color) {
    RECT r{0, 0, m_width, m_height};
    HBRUSH brush = CreateSolidBrush(RGB(color.r, color.g, color.b));
    FillRect(m_memDC, &r, brush);
    DeleteObject(brush);
}

void Win32Renderer::drawRect(const Rect& rect, const Color& color, bool fill) {
    RECT r{rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
    HBRUSH brush = CreateSolidBrush(RGB(color.r, color.g, color.b));
    if (fill) {
        FillRect(m_memDC, &r, brush);
    } else {
        FrameRect(m_memDC, &r, brush);
    }
    DeleteObject(brush);
}

HFONT Win32Renderer::getFont(int size, bool bold) {
    int key = size * 10 + (bold ? 1 : 0);
    auto it = m_fontCache.find(key);
    if (it != m_fontCache.end()) {
        return it->second;
    }

    HFONT font = CreateFontW(
        -size, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    m_fontCache[key] = font;
    return font;
}

void Win32Renderer::drawText(const std::string& text, int x, int y, const Color& color, int fontSize, bool bold) {
    if (text.empty()) return;

    std::wstring wText = utf8ToWide(text);
    if (wText.empty()) return;

    HFONT font = getFont(fontSize, bold);
    HFONT oldFont = (HFONT)SelectObject(m_memDC, font);

    SetTextColor(m_memDC, RGB(color.r, color.g, color.b));
    SetBkMode(m_memDC, TRANSPARENT);

    TextOutW(m_memDC, x, y, wText.c_str(), static_cast<int>(wText.length()));

    SelectObject(m_memDC, oldFont);
}

void Win32Renderer::drawImage(const uint32_t* pixels, int srcW, int srcH, const Rect& dstRect) {
    if (!pixels || srcW <= 0 || srcH <= 0) return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = srcW;
    bmi.bmiHeader.biHeight = -srcH; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(m_memDC, COLORONCOLOR);
    StretchDIBits(
        m_memDC,
        dstRect.x, dstRect.y, dstRect.width, dstRect.height,
        0, 0, srcW, srcH,
        pixels,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

void Win32Renderer::drawProgressBar(const Rect& rect, float progress, const Color& bg, const Color& fg) {
    drawRect(rect, bg, true);
    int fillW = static_cast<int>(rect.width * std::clamp(progress, 0.0f, 1.0f));
    if (fillW > 0) {
        Rect filledRect{rect.x, rect.y, fillW, rect.height};
        drawRect(filledRect, fg, true);
    }
}

void Win32Renderer::drawButton(const Rect& rect, const std::string& text, bool hovered, bool /*active*/) {
    Color bg = hovered ? Color{60, 60, 75, 255} : Color{45, 45, 55, 255};
    drawRect(rect, bg, true);
    drawRect(rect, Color{70, 70, 85, 255}, false);

    Point txtSize = measureText(text, 13, true);
    int tx = rect.x + (rect.width - txtSize.x) / 2;
    int ty = rect.y + (rect.height - txtSize.y) / 2;
    drawText(text, tx, ty, Color{240, 240, 240, 255}, 13, true);
}

Point Win32Renderer::measureText(const std::string& text, int fontSize, bool bold) {
    if (text.empty()) return Point{0, 0};

    std::wstring wText = utf8ToWide(text);
    if (wText.empty()) return Point{0, 0};

    HFONT font = getFont(fontSize, bold);
    HFONT oldFont = (HFONT)SelectObject(m_memDC, font);

    SIZE sz{};
    GetTextExtentPoint32W(m_memDC, wText.c_str(), static_cast<int>(wText.length()), &sz);

    SelectObject(m_memDC, oldFont);
    return Point{static_cast<int>(sz.cx), static_cast<int>(sz.cy)};
}

} // namespace yt
#endif
