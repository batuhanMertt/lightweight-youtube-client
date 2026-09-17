#pragma once

#include "ui/IRenderer.h"
#include "ui/UIEvents.h"

namespace yt {

class IScreen {
public:
    virtual ~IScreen() = default;

    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void update(float dt) { (void)dt; }
    virtual void render(IRenderer& renderer) = 0;
    virtual bool handleEvent(const UIEvent& event) = 0;
};

} // namespace yt
