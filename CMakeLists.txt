cmake_minimum_required(VERSION 3.16)
project(youtube-client VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and warning flags
if(MSVC)
    add_compile_options(/W4 /EHsc /utf-8)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /GL)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O2)
    endif()
endif()

# Embedded / Lightweight definitions
add_definitions(-DAPP_VERSION="1.0.0")

# Project directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Core Sources
set(CORE_SOURCES
    src/core/Logger.cpp
    src/core/ThreadPool.cpp
    src/core/Config.cpp
    src/core/PerformanceMonitor.cpp
)

# Cache Sources
set(CACHE_SOURCES
    src/cache/ThumbnailCache.cpp
    src/cache/ImageDecoder.cpp
)

# YouTube Client Sources
set(YOUTUBE_SOURCES
    src/youtube/JsonParser.cpp
    src/youtube/MockYouTubeClient.cpp
    src/youtube/YouTubeClient.cpp
    src/youtube/LiveYouTubeClient.cpp
)

# Auth Sources
set(AUTH_SOURCES
    src/auth/AuthService.cpp
    src/auth/TokenStorage.cpp
)

# Player Sources
set(PLAYER_SOURCES
    src/player/PlayerController.cpp
    src/player/MockPlayerBackend.cpp
)

# UI Sources
set(UI_SOURCES
    src/ui/AppUI.cpp
    src/ui/components/NavigationBar.cpp
    src/ui/components/SearchBar.cpp
    src/ui/screens/HomeScreen.cpp
    src/ui/screens/SearchScreen.cpp
    src/ui/screens/SubscriptionsScreen.cpp
    src/ui/screens/VideoScreen.cpp
    src/ui/screens/AccountScreen.cpp
)

# Platform Sources
if(WIN32)
    set(PLATFORM_SOURCES
        src/platform/windows/Win32Window.cpp
        src/platform/windows/Win32Renderer.cpp
        src/platform/windows/WindowsPlayerBackend.cpp
        src/platform/windows/WinHttpClient.cpp
    )
    set(PLATFORM_LIBS
        gdi32
        user32
        psapi
        ws2_32
        winhttp
        dwmapi
    )
else()
    set(PLATFORM_SOURCES
        src/platform/linux/LinuxWindow.cpp
        src/platform/linux/GStreamerPlayerBackend.cpp
    )
    set(PLATFORM_LIBS
        pthread
    )
endif()

# Main Application Executable
add_executable(youtube-client
    src/main.cpp
    ${CORE_SOURCES}
    ${CACHE_SOURCES}
    ${YOUTUBE_SOURCES}
    ${AUTH_SOURCES}
    ${PLAYER_SOURCES}
    ${UI_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(youtube-client PRIVATE ${PLATFORM_LIBS})

# Unit & Performance Tests Executable
add_executable(run_tests
    tests/main_test.cpp
    tests/test_json.cpp
    tests/test_cache.cpp
    tests/test_youtube_mock.cpp
    tests/test_performance.cpp
    tests/test_network.cpp
    ${CORE_SOURCES}
    ${CACHE_SOURCES}
    ${YOUTUBE_SOURCES}
    ${AUTH_SOURCES}
    ${PLAYER_SOURCES}
    ${UI_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(run_tests PRIVATE ${PLATFORM_LIBS})

# Enable static linking for MinGW/Clang on Windows to ensure portable standalone executables
if(MINGW OR (WIN32 AND NOT MSVC))
    target_link_options(youtube-client PRIVATE -static)
    target_link_options(run_tests PRIVATE -static)
endif()

# Copy mock assets, config, and tools to output directory
add_custom_command(TARGET youtube-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:youtube-client>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:youtube-client>/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
        $<TARGET_FILE_DIR:youtube-client>/tools
    COMMENT "Copying assets, config, and tools to runtime directory"
)

add_custom_command(TARGET run_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:run_tests>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:run_tests>/config
    COMMENT "Copying assets and config to test directory"
)
