#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace yt {

struct MemoryStats {
    double currentWorkingSetMb{0.0};
    double peakWorkingSetMb{0.0};
    double privateBytesMb{0.0};
};

struct CpuStats {
    double cpuUsagePercent{0.0};
};

// Resources used by child processes this app spawned (the mpv video player).
struct ChildProcessStats {
    int processCount{0};
    double workingSetMb{0.0};
    double cpuUsagePercent{0.0};
};

class PerformanceMonitor {
public:
    static PerformanceMonitor& instance();

    void initialize();
    MemoryStats getMemoryStats();
    CpuStats getCpuStats();

    void recordStartupTime(double milliseconds);
    double getStartupTime() const { return m_startupTimeMs; }

    ChildProcessStats getChildProcessStats();

    // Cached, refreshed at most once per second (it is drawn every frame).
    std::string getFormattedSummary();

private:
    PerformanceMonitor();
    double m_startupTimeMs{0.0};

    // CPU tracking state
    std::chrono::steady_clock::time_point m_lastCpuCheck;
    uint64_t m_lastKernelTime{0};
    uint64_t m_lastUserTime{0};

    std::chrono::steady_clock::time_point m_lastChildCpuCheck;
    uint64_t m_lastChildCpuTime{0};

    std::chrono::steady_clock::time_point m_lastSummaryTime;
    std::string m_cachedSummary;
};

} // namespace yt
