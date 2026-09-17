#include <iostream>
#include <string>

void runJsonTests();
void runCacheTests();
void runYouTubeMockTests();
void runPerformanceTests();
void runNetworkTests();

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Running YouTube Embedded Client Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        runJsonTests();
        runCacheTests();
        runYouTubeMockTests();
        runPerformanceTests();
        runNetworkTests();

        std::cout << "\n========================================" << std::endl;
        std::cout << "ALL TESTS PASSED SUCCESSFULLY!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "\n[TEST FAILED]: " << ex.what() << std::endl;
        return 1;
    }
}
