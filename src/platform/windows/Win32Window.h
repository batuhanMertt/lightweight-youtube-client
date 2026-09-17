#pragma once

#include "platform/IPlatformWindow.h"
#include "platform/windows/Win32Renderer.h"
#if defined(_WIN32)
#include <windows.h>

namespace yt {

class Win32Window : public IPlatformWindow {
public:
    Win32Window();
    ~Win32Window() override;

    bool create(const std::string& title, int width, int height) override;
    void show() override;
    bool processEvents(AppUI& ui) override;
    void present() override;
    bool shouldClose() const override { return m_shouldClose; }
    void close() override;

    IRenderer& getRenderer() override { return m_renderer; }
    int getWidth() const override { return m_width; }
    int getHeight() const override { return m_height; }

    HWND getHwnd() const { return m_hwnd; }
    HWND getVideoHwnd() const { return m_videoHwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd{nullptr};
    HWND m_videoHwnd{nullptr};
    int m_width{960};
    int m_height{640};
    bool m_shouldClose{false};
    Win32Renderer m_renderer;
    AppUI* m_currentUI{nullptr};
    wchar_t m_pendingHighSurrogate{0};
};

} // namespace yt
#endif
