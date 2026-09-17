#pragma once

#include "ui/screens/IScreen.h"
#include "auth/IAuthService.h"
#include "core/ThreadPool.h"
#include <memory>

namespace yt {

class AccountScreen : public IScreen {
public:
    AccountScreen(std::shared_ptr<IAuthService> authService, std::shared_ptr<ThreadPool> pool);

    void onEnter() override;
    void render(IRenderer& renderer) override;
    bool handleEvent(const UIEvent& event) override;

    void startDeviceAuth();

private:
    std::shared_ptr<IAuthService> m_auth;
    std::shared_ptr<ThreadPool> m_pool;

    DeviceCodeInfo m_codeInfo;
    bool m_flowActive{false};
    std::string m_statusMsg;

    Rect m_authBtnBounds{};
    Rect m_confirmBtnBounds{};
    Rect m_logoutBtnBounds{};

    bool m_authHovered{false};
    bool m_confirmHovered{false};
    bool m_logoutHovered{false};
};

} // namespace yt
