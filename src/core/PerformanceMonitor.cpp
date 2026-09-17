#include "core/PerformanceMonitor.h"
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#endif

namespace yt {

PerformanceMonitor::PerformanceMonitor() {
    m_lastCpuCheck = std::chrono::steady_clock::now();
}

PerformanceMonitor& PerformanceMonitor::instance() {
    static PerformanceMonitor s_instance;
    return s_instance;
}

void PerformanceMonitor::initialize() {
    m_lastCpuCheck = std::chrono::steady_clock::now();
#if defined(_WIN32)
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    if (GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        ULARGE_INTEGER ulKernel, ulUser;
        ulKernel.LowPart = ftKernel.dwLowDateTime;
        ulKernel.HighPart = ftKernel.dwHighDateTime;
        ulUser.LowPart = ftUser.dwLowDateTime;
        ulUser.HighPart = ftUser.dwHighDateTime;
        m_lastKernelTime = ulKernel.QuadPart;
        m_lastUserTime = ulUser.QuadPart;
    }
#endif
}

MemoryStats PerformanceMonitor::getMemoryStats() {
    MemoryStats stats{};
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        stats.currentWorkingSetMb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        stats.peakWorkingSetMb = static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
        stats.privateBytesMb = static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    }
#else
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // maxrss is in kilobytes on Linux
        stats.peakWorkingSetMb = static_cast<double>(usage.ru_maxrss) / 1024.0;
        stats.currentWorkingSetMb = stats.peakWorkingSetMb;
    }
#endif
    return stats;
}

CpuStats PerformanceMonitor::getCpuStats() {
    CpuStats stats{0.0};
#if defined(_WIN32)
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    if (GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        ULARGE_INTEGER ulKernel, ulUser;
        ulKernel.LowPart = ftKernel.dwLowDateTime;
        ulKernel.HighPart = ftKernel.dwHighDateTime;
        ulUser.LowPart = ftUser.dwLowDateTime;
        ulUser.HighPart = ftUser.dwHighDateTime;

        auto now = std::chrono::steady_clock::now();
        double elapsedSeconds = std::chrono::duration<double>(now - m_lastCpuCheck).count();

        if (elapsedSeconds > 0.05) {
            uint64_t diffKernel = ulKernel.QuadPart - m_lastKernelTime;
            uint64_t diffUser = ulUser.QuadPart - m_lastUserTime;
            uint64_t totalDiff = diffKernel + diffUser; // 100-nanosecond intervals

            // totalDiff is in 100ns units -> seconds = totalDiff / 10,000,000
            double cpuSeconds = static_cast<double>(totalDiff) / 10000000.0;
            stats.cpuUsagePercent = (cpuSeconds / elapsedSeconds) * 100.0;

            m_lastKernelTime = ulKernel.QuadPart;
            m_lastUserTime = ulUser.QuadPart;
            m_lastCpuCheck = now;
        }
    }
#else
    stats.cpuUsagePercent = 0.0; // Not implemented on this platform yet
#endif
    return stats;
}

void PerformanceMonitor::recordStartupTime(double milliseconds) {
    m_startupTimeMs = milliseconds;
}

ChildProcessStats PerformanceMonitor::getChildProcessStats() {
    ChildProcessStats stats{};
#if defined(_WIN32)
    DWORD selfPid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return stats;

    uint64_t totalCpu = 0;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            // Only count the mpv player. The console host (conhost.exe) is also a child
            // of this process and must not be reported as "Player".
            if (entry.th32ParentProcessID != selfPid || _wcsicmp(entry.szExeFile, L"mpv.exe") != 0) continue;
            HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
            if (!proc) continue;
            PROCESS_MEMORY_COUNTERS pmc{};
            if (GetProcessMemoryInfo(proc, &pmc, sizeof(pmc))) {
                stats.workingSetMb += static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
            }
            FILETIME c, e, k, u;
            if (GetProcessTimes(proc, &c, &e, &k, &u)) {
                ULARGE_INTEGER uk, uu;
                uk.LowPart = k.dwLowDateTime; uk.HighPart = k.dwHighDateTime;
                uu.LowPart = u.dwLowDateTime; uu.HighPart = u.dwHighDateTime;
                totalCpu += uk.QuadPart + uu.QuadPart;
            }
            ++stats.processCount;
            CloseHandle(proc);
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);

    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - m_lastChildCpuCheck).count();
    if (m_lastChildCpuTime != 0 && totalCpu >= m_lastChildCpuTime && elapsed > 0.05) {
        double cpuSeconds = static_cast<double>(totalCpu - m_lastChildCpuTime) / 10000000.0;
        stats.cpuUsagePercent = (cpuSeconds / elapsed) * 100.0;
    }
    // If mpv restarted, the new process starts from a lower CPU time - just re-baseline.
    m_lastChildCpuTime = totalCpu;
    m_lastChildCpuCheck = now;
#endif
    return stats;
}

std::string PerformanceMonitor::getFormattedSummary() {
    auto now = std::chrono::steady_clock::now();
    if (!m_cachedSummary.empty() &&
        std::chrono::duration<double>(now - m_lastSummaryTime).count() < 1.0) {
        return m_cachedSummary;
    }
    m_lastSummaryTime = now;

    auto mem = getMemoryStats();
    auto cpu = getCpuStats();
    auto child = getChildProcessStats();

    // Normalize to all logical cores so the number matches Task Manager's CPU column.
    double cores = 1.0;
#if defined(_WIN32)
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    if (si.dwNumberOfProcessors > 0) cores = static_cast<double>(si.dwNumberOfProcessors);
#endif
    double appCpu = cpu.cpuUsagePercent / cores;
    double childCpu = child.cpuUsagePercent / cores;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "App: " << mem.currentWorkingSetMb << " MB";
    if (child.processCount > 0) {
        oss << " | Player (mpv): " << child.workingSetMb << " MB"
            << " | Total: " << (mem.currentWorkingSetMb + child.workingSetMb) << " MB"
            << " | CPU: " << (appCpu + childCpu) << "%";
    } else {
        oss << " | CPU: " << appCpu << "%";
    }
    m_cachedSummary = oss.str();
    return m_cachedSummary;
}

} // namespace yt
