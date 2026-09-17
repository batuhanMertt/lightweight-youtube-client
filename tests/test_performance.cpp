#include "core/PerformanceMonitor.h"
#include <iostream>
#include <cassert>

void runPerformanceTests() {
    std::cout << "\n[TEST] Starting Performance & RAM Profiling Tests..." << std::endl;

    auto& perf = yt::PerformanceMonitor::instance();
    perf.initialize();

    auto mem = perf.getMemoryStats();
    std::cout << "  - Current Working Set: " << mem.currentWorkingSetMb << " MB" << std::endl;
    std::cout << "  - Peak Working Set:    " << mem.peakWorkingSetMb << " MB" << std::endl;

    // Embedded budget test: Memory overhead must remain tiny (under 100 MB for core native client)
    // Target device has 4 GB RAM, but our lightweight client aims for < 50 MB
    assert(mem.currentWorkingSetMb < 100.0 && "RAM footprint must stay under embedded budget (<100MB)");

    std::cout << "  -> Performance & Memory Profile Tests Passed!" << std::endl;
}
