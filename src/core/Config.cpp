#include "core/Config.h"
#include "core/Logger.h"
#include <fstream>
#include <sstream>

namespace yt {

ConfigManager& ConfigManager::instance() {
    static ConfigManager s_instance;
    return s_instance;
}

void ConfigManager::loadFromFile(const std::string& configFilePath) {
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        LOG_WARN("Could not open config file: " + configFilePath + ", using built-in defaults.");
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Basic extraction to keep startup lightweight without requiring external parser at early boot
    auto extractInt = [&](const std::string& key, int defaultVal) -> int {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return defaultVal;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return defaultVal;
        size_t end = content.find_first_of(",}\n", pos);
        std::string valStr = content.substr(pos + 1, end - pos - 1);
        try {
            return std::stoi(valStr);
        } catch (...) {
            return defaultVal;
        }
    };

    m_config.maxHomeVideos = extractInt("max_home_videos", m_config.maxHomeVideos);
    m_config.maxSearchResults = extractInt("max_search_results", m_config.maxSearchResults);
    m_config.maxSubscriptionVideos = extractInt("max_subscription_videos", m_config.maxSubscriptionVideos);
    m_config.maxThumbnailCacheMb = extractInt("max_thumbnail_cache_mb", m_config.maxThumbnailCacheMb);
    m_config.networkTimeoutMs = extractInt("network_timeout_ms", m_config.networkTimeoutMs);
    m_config.uiFps = extractInt("ui_fps", m_config.uiFps);
    m_config.maxVideoHeight = extractInt("max_video_height", m_config.maxVideoHeight);

    auto extractString = [&](const std::string& key, const std::string& defaultVal) -> std::string {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return defaultVal;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return defaultVal;
        size_t start = content.find('"', pos);
        if (start == std::string::npos) return defaultVal;
        size_t end = content.find('"', start + 1);
        if (end == std::string::npos) return defaultVal;
        return content.substr(start + 1, end - start - 1);
    };
    auto extractBool = [&](const std::string& key, bool defaultVal) -> bool {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return defaultVal;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return defaultVal;
        size_t valuePos = content.find_first_not_of(" \t", pos + 1);
        if (valuePos == std::string::npos) return defaultVal;
        if (content.compare(valuePos, 4, "true") == 0) return true;
        if (content.compare(valuePos, 5, "false") == 0) return false;
        return defaultVal;
    };
    m_config.videoCodec = extractString("video_codec", m_config.videoCodec);
    m_config.hardwareDecoding = extractBool("hardware_decoding", m_config.hardwareDecoding);

    if (content.find("\"use_mock_data\": false") != std::string::npos ||
        content.find("\"use_mock_data\":false") != std::string::npos) {
        m_config.useMockData = false;
    } else if (content.find("\"use_mock_data\": true") != std::string::npos ||
               content.find("\"use_mock_data\":true") != std::string::npos) {
        m_config.useMockData = true;
    }

    LOG_INFO("Configuration loaded. Home limit: " + std::to_string(m_config.maxHomeVideos) +
             ", Search limit: " + std::to_string(m_config.maxSearchResults) +
             ", Thumbnail cache: " + std::to_string(m_config.maxThumbnailCacheMb) + " MB");
}

} // namespace yt
