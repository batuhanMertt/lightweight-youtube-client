#pragma once

#include "auth/IAuthService.h"
#include "network/IHttpClient.h"
#include <memory>

namespace yt {

class AuthService : public IAuthService {
public:
    static AuthService& instance();

    void setHttpClient(std::shared_ptr<IHttpClient> client) { m_http = client; }
    void setClientId(const std::string& clientId) { m_clientId = clientId; }
    void setClientSecret(const std::string& clientSecret) { m_clientSecret = clientSecret; }

    bool isAuthenticated() const override;
    UserProfile getProfile() const override;

    Result<DeviceCodeInfo> requestDeviceCode() override;
    Result<bool> pollDeviceToken(const std::string& deviceCode) override;

    void logout() override;

    // Helper for testing/offline simulation
    void simulateSuccessfulAuth(const std::string& email = "guest@example.com",
                                const std::string& name = "Guest User");

private:
    AuthService();
    std::shared_ptr<IHttpClient> m_http;
    UserProfile m_profile;
    std::string m_clientId;
    std::string m_clientSecret;
};

} // namespace yt
