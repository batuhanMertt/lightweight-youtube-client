#pragma once

#include "ui/IRenderer.h"
#include <string>
#include <memory>

namespace yt {

class AppUI;

class IPlatformWindow {
public:
    virtual ~IPlatformWindow() = default;

    virtual bool create(const std::string& title, int width, int height) = 0;
    virtual void show() = 0;
    virtual bool processEvents(AppUI& ui) = 0;
    virtual void present() = 0;
    virtual bool shouldClose() const = 0;
    virtual void close() = 0;

    virtual IRenderer& getRenderer() = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};

} // namespace yt
