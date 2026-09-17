#pragma once

#include "platform/IPlatformWindow.h"

namespace yt {

class LinuxRendererStub : public IRenderer {
public:
    void clear(const Color&) override {}
    void drawRect(const Rect&, const Color&, bool) override {}
    void drawText(const std::string&, int, int, const Color&, int, bool) override {}
    void drawImage(const uint32_t*, int, int, const Rect&) override {}
    void drawProgressBar(const Rect&, float, const Color&, const Color&) override {}
    void drawButton(const Rect&, const std::string&, bool, bool) override {}
    Point measureText(const std::string& text, int, bool) override { return Point{static_cast<int>(text.length() * 8), 16}; }
    int getWidth() const override { return 960; }
    int getHeight() const override { return 640; }
};

class LinuxWindow : public IPlatformWindow {
public:
    LinuxWindow();
    ~LinuxWindow() override;

    bool create(const std::string& title, int width, int height) override;
    void show() override;
    bool processEvents(AppUI& ui) override;
    void present() override;
    bool shouldClose() const override { return m_shouldClose; }
    void close() override;

    IRenderer& getRenderer() override { return m_renderer; }
    int getWidth() const override { return m_width; }
    int getHeight() const override { return m_height; }

private:
    int m_width{960};
    int m_height{640};
    bool m_shouldClose{false};
    LinuxRendererStub m_renderer;
};

} // namespace yt
