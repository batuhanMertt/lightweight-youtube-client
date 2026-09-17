#include "ui/screens/AccountScreen.h"
#include "auth/AuthService.h"
#include "core/EventQueue.h"
#include "core/Logger.h"

namespace yt {

AccountScreen::AccountScreen(std::shared_ptr<IAuthService> authService, std::shared_ptr<ThreadPool> pool)
    : m_auth(authService), m_pool(pool) {
}

void AccountScreen::onEnter() {
    if (m_auth->isAuthenticated()) {
        m_statusMsg = "Connected as: " + m_auth->getProfile().email;
    } else {
        m_statusMsg = "Not logged in (Guest Mode)";
    }
}

void AccountScreen::startDeviceAuth() {
    m_statusMsg = "Requesting device code...";
    auto auth = m_auth;
    m_pool->enqueue([this, auth]() {
        auto res = auth->requestDeviceCode();
        EventQueue::instance().post([this, res]() {
            if (res.success) {
                m_codeInfo = res.data;
                m_flowActive = true;
                m_statusMsg = "Device authorization pending...";
            } else {
                m_statusMsg = "Failed to initiate device auth: " + res.message;
            }
        });
    });
}

void AccountScreen::render(IRenderer& renderer) {
    int screenW = renderer.getWidth();
    int startY = 70;

    renderer.drawText("YouTube Account & Authorization", 30, startY, Color{240, 240, 245, 255}, 16, true);

    // Profile Card
    Rect cardRect{30, startY + 30, screenW - 60, 110};
    renderer.drawRect(cardRect, Color{30, 30, 38, 255}, true);
    renderer.drawRect(cardRect, Color{50, 50, 65, 255}, false);

    bool isAuth = m_auth->isAuthenticated();
    if (isAuth) {
        auto prof = m_auth->getProfile();
        renderer.drawText("Status: [AUTHENTICATED]", cardRect.x + 20, cardRect.y + 16, Color{50, 205, 50, 255}, 13, true);
        renderer.drawText("Name:  " + prof.displayName, cardRect.x + 20, cardRect.y + 40, Color{240, 240, 240, 255}, 13);
        renderer.drawText("Email: " + prof.email, cardRect.x + 20, cardRect.y + 64, Color{180, 180, 195, 255}, 13);

        m_logoutBtnBounds = Rect{cardRect.x + cardRect.width - 130, cardRect.y + 36, 110, 34};
        Color logBg = m_logoutHovered ? Color{180, 50, 50, 255} : Color{140, 35, 35, 255};
        renderer.drawRect(m_logoutBtnBounds, logBg, true);
        Point txtP = renderer.measureText("Logout", 13, true);
        renderer.drawText("Logout",
                          m_logoutBtnBounds.x + (110 - txtP.x) / 2,
                          m_logoutBtnBounds.y + (34 - txtP.y) / 2,
                          Color{255, 255, 255, 255}, 13, true);
    } else {
        renderer.drawText("Status: [GUEST / ANONYMOUS]", cardRect.x + 20, cardRect.y + 16, Color{255, 180, 0, 255}, 13, true);
        renderer.drawText("Login using RFC 8628 OAuth Device Authorization flow without webview.", cardRect.x + 20, cardRect.y + 42, Color{180, 180, 190, 255}, 12);
        renderer.drawText("No browser engine or HTML view is used for login.", cardRect.x + 20, cardRect.y + 64, Color{140, 140, 150, 255}, 11);

        m_authBtnBounds = Rect{cardRect.x + cardRect.width - 190, cardRect.y + 36, 170, 34};
        Color authBg = m_authHovered ? Color{40, 120, 220, 255} : Color{30, 95, 180, 255};
        renderer.drawRect(m_authBtnBounds, authBg, true);
        Point txtP = renderer.measureText("Start Device Login", 12, true);
        renderer.drawText("Start Device Login",
                          m_authBtnBounds.x + (170 - txtP.x) / 2,
                          m_authBtnBounds.y + (34 - txtP.y) / 2,
                          Color{255, 255, 255, 255}, 12, true);
    }

    // Active Device Flow Card
    if (m_flowActive && !isAuth) {
        int flowY = cardRect.y + cardRect.height + 20;
        Rect flowRect{30, flowY, screenW - 60, 160};
        renderer.drawRect(flowRect, Color{25, 28, 35, 255}, true);
        renderer.drawRect(flowRect, Color{70, 100, 150, 255}, false);

        renderer.drawText("Step 1: On your phone, tablet, or PC, navigate to:", flowRect.x + 20, flowRect.y + 20, Color{220, 220, 230, 255}, 13);
        renderer.drawText(m_codeInfo.verificationUrl, flowRect.x + 20, flowRect.y + 42, Color{80, 160, 255, 255}, 14, true);

        renderer.drawText("Step 2: Enter this one-time pairing code:", flowRect.x + 20, flowRect.y + 70, Color{220, 220, 230, 255}, 13);
        renderer.drawText(m_codeInfo.userCode, flowRect.x + 20, flowRect.y + 92, Color{255, 215, 0, 255}, 18, true);

        m_confirmBtnBounds = Rect{flowRect.x + 20, flowRect.y + 120, 190, 30};
        Color confBg = m_confirmHovered ? Color{45, 140, 70, 255} : Color{35, 110, 55, 255};
        renderer.drawRect(m_confirmBtnBounds, confBg, true);
        Point cSize = renderer.measureText("Simulate User Paired", 12, true);
        renderer.drawText("Simulate User Paired",
                          m_confirmBtnBounds.x + (190 - cSize.x) / 2,
                          m_confirmBtnBounds.y + (30 - cSize.y) / 2,
                          Color{255, 255, 255, 255}, 12, true);
    }

    // Status Message at bottom
    if (!m_statusMsg.empty()) {
        renderer.drawText(m_statusMsg, 30, startY + 330, Color{160, 160, 175, 255}, 12);
    }
}

bool AccountScreen::handleEvent(const UIEvent& event) {
    if (event.type == EventType::MOUSE_MOVE) {
        m_authHovered = m_authBtnBounds.contains(event.mouseX, event.mouseY);
        m_confirmHovered = m_confirmBtnBounds.contains(event.mouseX, event.mouseY);
        m_logoutHovered = m_logoutBtnBounds.contains(event.mouseX, event.mouseY);
        return false;
    }

    if (event.type == EventType::MOUSE_DOWN && event.button == 0) {
        if (m_authBtnBounds.contains(event.mouseX, event.mouseY) && !m_auth->isAuthenticated()) {
            startDeviceAuth();
            return true;
        }

        if (m_confirmBtnBounds.contains(event.mouseX, event.mouseY) && m_flowActive) {
            // Simulate that user completed the flow on their external device
            AuthService::instance().simulateSuccessfulAuth();
            m_flowActive = false;
            m_statusMsg = "Logged in successfully via OAuth Device Flow!";
            return true;
        }

        if (m_logoutBtnBounds.contains(event.mouseX, event.mouseY) && m_auth->isAuthenticated()) {
            m_auth->logout();
            m_flowActive = false;
            m_statusMsg = "Logged out successfully";
            return true;
        }
    }

    return false;
}

} // namespace yt
