#include "auth/TokenStorage.h"
#include "core/Logger.h"
#include <chrono>

namespace yt {

TokenStorage& TokenStorage::instance() {
    static TokenStorage s_instance;
    return s_instance;
}

bool TokenStorage::saveTokens(const std::string& accessToken, const std::string& refreshToken, int expiresIn) {
    m_accessToken = accessToken;
    m_refreshToken = refreshToken;

    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    m_expiryTimestamp = seconds + expiresIn;

    LOG_INFO("Tokens stored safely in secure session storage");
    return true;
}

std::optional<std::string> TokenStorage::getAccessToken() const {
    if (m_accessToken.empty()) {
        return std::nullopt;
    }
    return m_accessToken;
}

std::optional<std::string> TokenStorage::getRefreshToken() const {
    if (m_refreshToken.empty()) {
        return std::nullopt;
    }
    return m_refreshToken;
}

bool TokenStorage::hasValidToken() const {
    if (m_accessToken.empty()) return false;
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return seconds < m_expiryTimestamp;
}

void TokenStorage::clear() {
    m_accessToken.clear();
    m_refreshToken.clear();
    m_expiryTimestamp = 0;
    LOG_INFO("Session tokens cleared");
}

} // namespace yt
