#pragma once

#include <string>
#include <optional>

namespace yt {

class TokenStorage {
public:
    static TokenStorage& instance();

    bool saveTokens(const std::string& accessToken, const std::string& refreshToken, int expiresIn);
    std::optional<std::string> getAccessToken() const;
    std::optional<std::string> getRefreshToken() const;
    bool hasValidToken() const;
    void clear();

private:
    TokenStorage() = default;
    std::string m_accessToken;
    std::string m_refreshToken;
    int64_t m_expiryTimestamp{0};
};

} // namespace yt
