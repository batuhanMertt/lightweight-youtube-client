#include "platform/linux/LinuxWindow.h"
#include "core/Logger.h"

namespace yt {

LinuxWindow::LinuxWindow() = default;
LinuxWindow::~LinuxWindow() = default;

bool LinuxWindow::create(const std::string& title, int width, int height) {
    m_width = width;
    m_height = height;
    LOG_INFO("Linux Window initialized (Target: DRM/KMS or SDL2 framebuffer)");
    return true;
}

void LinuxWindow::show() {
    LOG_INFO("Linux Window display mapped");
}

bool LinuxWindow::processEvents(AppUI& /*ui*/) {
    return !m_shouldClose;
}

void LinuxWindow::present() {
    // Framebuffer / DRM page flip
}

void LinuxWindow::close() {
    m_shouldClose = true;
}

} // namespace yt
