#pragma once

#include <string>
#include <mutex>
#include <iostream>

#if defined(_WIN32)
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace yt {

enum class LogLevel {
    LEVEL_DEBUG = 0,
    LEVEL_INFO = 1,
    LEVEL_WARN = 2,
    LEVEL_ERROR = 3
};

class Logger {
public:
    static Logger& instance();

    void setLogLevel(LogLevel level);
    void log(LogLevel level, const std::string& message);

    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);
    void debug(const std::string& msg);

    // Filter out credentials to satisfy security rules
    static std::string sanitize(const std::string& msg);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel m_minLevel{LogLevel::LEVEL_INFO};
    std::mutex m_mutex;
};

#define LOG_INFO(msg)  yt::Logger::instance().info(msg)
#define LOG_WARN(msg)  yt::Logger::instance().warn(msg)
#define LOG_ERROR(msg) yt::Logger::instance().error(msg)
#define LOG_DEBUG(msg) yt::Logger::instance().debug(msg)

} // namespace yt
