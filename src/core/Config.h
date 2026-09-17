#pragma once

#include <string>

namespace yt {

struct AppConfig {
    std::string appName{"Lightweight YouTube Client"};
    std::string version{"1.0.0"};
    int maxHomeVideos{20};
    int maxSearchResults{10};
    int maxSubscriptionVideos{20};
    int maxThumbnailCacheMb{16};
    int networkTimeoutMs{5000};
    bool enableProfiling{true};
    int uiFps{30};
    bool useMockData{true};
    std::string assetsDir{"assets"};
};

class ConfigManager {
public:
    static ConfigManager& instance();

    const AppConfig& getConfig() const { return m_config; }
    void loadFromFile(const std::string& configFilePath);

private:
    ConfigManager() = default;
    AppConfig m_config;
};

} // namespace yt
