#pragma once

#include "core/Types.h"
#include <string>
#include <functional>

namespace yt {

struct DeviceCodeInfo {
    std::string deviceCode;
    std::string userCode;
    std::string verificationUrl;
    int expiresInSeconds{1800};
    int intervalSeconds{5};
};

struct UserProfile {
    std::string userId;
    std::string displayName;
    std::string email;
    bool isAuthenticated{false};
};

class IAuthService {
public:
    virtual ~IAuthService() = default;

    virtual bool isAuthenticated() const = 0;
    virtual UserProfile getProfile() const = 0;

    // Start RFC 8628 Device Authorization Flow (Ideal for embedded systems without web browsers)
    virtual Result<DeviceCodeInfo> requestDeviceCode() = 0;
    virtual Result<bool> pollDeviceToken(const std::string& deviceCode) = 0;

    virtual void logout() = 0;
};

} // namespace yt
