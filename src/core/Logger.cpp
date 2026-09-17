#include "core/Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <regex>

namespace yt {

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minLevel = level;
}

std::string Logger::sanitize(const std::string& msg) {
    std::string result = msg;
    static const std::vector<std::pair<std::regex, std::string>> patterns = {
        {std::regex(R"((access_token|refresh_token|token|secret|client_secret)=([^\s&]+))", std::regex_constants::icase), "$1=[REDACTED]"},
        {std::regex(R"(("access_token"|"refresh_token"|"client_secret")\s*:\s*"[^"]+")", std::regex_constants::icase), "$1:\"[REDACTED]\""},
        {std::regex(R"(Bearer\s+[A-Za-z0-9_\-\.]+)", std::regex_constants::icase), "Bearer [REDACTED]"}
    };

    for (const auto& p : patterns) {
        result = std::regex_replace(result, p.first, p.second);
    }
    return result;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < m_minLevel) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    const char* levelStr = "[INFO]";
    switch (level) {
        case LogLevel::LEVEL_DEBUG: levelStr = "[DEBUG]"; break;
        case LogLevel::LEVEL_INFO:  levelStr = "[INFO]";  break;
        case LogLevel::LEVEL_WARN:  levelStr = "[WARN]";  break;
        case LogLevel::LEVEL_ERROR: levelStr = "[ERROR]"; break;
    }

    std::string safeMsg = sanitize(message);
    std::cout << levelStr << " " << safeMsg << std::endl;
}

void Logger::info(const std::string& msg) {
    log(LogLevel::LEVEL_INFO, msg);
}

void Logger::warn(const std::string& msg) {
    log(LogLevel::LEVEL_WARN, msg);
}

void Logger::error(const std::string& msg) {
    log(LogLevel::LEVEL_ERROR, msg);
}

void Logger::debug(const std::string& msg) {
    log(LogLevel::LEVEL_DEBUG, msg);
}

} // namespace yt
