#include "platform/windows/WindowsPlayerBackend.h"
#include "core/Logger.h"
#include "core/Config.h"
#include "core/PerformanceMonitor.h"
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace yt {

namespace {

long long steadyNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

#if defined(_WIN32)
// The IPC pipe is opened with FILE_FLAG_OVERLAPPED. This matters a lot: on a handle opened
// for synchronous I/O Windows serializes all operations, so while the reader thread sits in a
// blocking ReadFile() every WriteFile() from the writer thread waits until mpv sends
// *something*. During playback mpv spams time-pos so it looked fine, but once paused mpv goes
// silent - and the resume / seek commands never reached it ("pause'dan sonra video donuyor").
bool overlappedWrite(HANDLE pipe, const std::string& data, DWORD timeoutMs) {
    if (pipe == INVALID_HANDLE_VALUE || data.empty()) return false;
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(pipe, data.data(), static_cast<DWORD>(data.size()), nullptr, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        if (WaitForSingleObject(ov.hEvent, timeoutMs) != WAIT_OBJECT_0) {
            CancelIoEx(pipe, &ov);
        }
        ok = GetOverlappedResult(pipe, &ov, &written, TRUE);
    } else if (ok) {
        ok = GetOverlappedResult(pipe, &ov, &written, FALSE);
    }
    CloseHandle(ov.hEvent);
    return ok && written == data.size();
}
#endif

bool extractJsonNumber(const std::string& line, double& out) {
    size_t dPos = line.find("\"data\":");
    if (dPos == std::string::npos) return false;
    std::string numStr = line.substr(dPos + 7);
    size_t endNum = numStr.find_first_of(",}");
    if (endNum != std::string::npos) numStr = numStr.substr(0, endNum);
    if (numStr.empty() || numStr == "null") return false;
    try {
        out = std::stod(numStr);
        return true;
    } catch (...) {
        return false;
    }
}

bool extractJsonString(const std::string& line, std::string& out) {
    size_t dPos = line.find("\"data\":\"");
    if (dPos == std::string::npos) return false;
    size_t start = dPos + 8;
    size_t end = start;
    while (end < line.size() && !(line[end] == '"' && line[end - 1] != '\\')) ++end;
    if (end >= line.size()) return false;
    out = line.substr(start, end - start);
    return true;
}

std::string envOr(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return (v && v[0] != '\0') ? std::string(v) : fallback;
}

std::wstring widen(const std::string& s) {
    return std::wstring(s.begin(), s.end()); // config values are ASCII
}

} // namespace

std::wstring WindowsPlayerBackend::buildYtdlFormat() const {
    const auto& cfg = ConfigManager::instance().getConfig();
    // Environment variables override the config file (used by tools/bench).
    std::string codec = envOr("YTC_VIDEO_CODEC", cfg.videoCodec);
    std::string height = envOr("YTC_MAX_HEIGHT", std::to_string(cfg.maxVideoHeight > 0 ? cfg.maxVideoHeight : 720));
    const std::string h = "[height<=" + height + "]";

    // Note: YouTube only offers a combined audio+video file at 360p nowadays, so a plain
    // "best[height<=720]" silently plays 360p. Ask for separate video + audio streams first.
    std::string format;
    if (codec == "h264") {
        // H.264 (avc1) is hardware-decoded by practically every GPU since ~2011, including old
        // laptops that can't decode VP9/AV1 in hardware and would otherwise burn CPU.
        format = "bestvideo" + h + "[vcodec^=avc1]+bestaudio[acodec^=mp4a]"
                 "/bestvideo" + h + "[vcodec^=avc1]+bestaudio"
                 "/best" + h + "[vcodec^=avc1]"
                 "/bestvideo" + h + "+bestaudio/best" + h + "/best";
    } else if (codec == "vp9" || codec == "av1") {
        // Mainly for benchmarking: force the codecs browsers usually get from YouTube.
        if (codec == "vp9") {
            format = "bestvideo" + h + "[vcodec^=vp9]+bestaudio/bestvideo" + h + "[vcodec^=vp09]+bestaudio/";
        } else {
            format = "bestvideo" + h + "[vcodec^=av01]+bestaudio/";
        }
        format += "bestvideo" + h + "+bestaudio/best" + h + "/best";
    } else {
        // "any": let yt-dlp choose (it prefers AV1 > VP9 > H.264 when a video has them).
        format = "bestvideo" + h + "+bestaudio/best" + h + "/best";
    }
    return widen(format);
}

std::wstring WindowsPlayerBackend::hwdecMode() const {
    const auto& cfg = ConfigManager::instance().getConfig();
    std::string mode = envOr("YTC_HWDEC", cfg.hardwareDecoding ? "auto-safe" : "no");
    return widen(mode);
}

void WindowsPlayerBackend::updatePlaybackInfo(bool forceWrite) {
    std::string label;
    std::ostringstream file;
    {
        std::lock_guard<std::mutex> lock(m_infoMutex);
        long long now = steadyNowMs();
        if (!forceWrite && now - m_lastInfoWriteMs < 1000) return;
        m_lastInfoWriteMs = now;

        std::string codecShort = m_infoCodecShort;
        if (codecShort.empty() && !m_infoCodec.empty()) {
            if (m_infoCodec.find("H.264") != std::string::npos) codecShort = "h264";
            else if (m_infoCodec.find("VP9") != std::string::npos) codecShort = "vp9";
            else if (m_infoCodec.find("AV1") != std::string::npos) codecShort = "av1";
            else codecShort = m_infoCodec.substr(0, m_infoCodec.find(' '));
        }
        bool hw = !m_infoHwdec.empty() && m_infoHwdec != "no";
        if (!codecShort.empty()) {
            label = codecShort;
            if (m_infoHeight > 0) label += " " + std::to_string(m_infoHeight) + "p";
            if (m_infoFps > 0.5) label += std::to_string(static_cast<int>(m_infoFps + 0.5));
            label += hw ? " HW" : " SW";
        }

        file << "codec=" << codecShort << "\n"
             << "codec_long=" << m_infoCodec << "\n"
             << "width=" << m_infoWidth << "\n"
             << "height=" << m_infoHeight << "\n"
             << "fps=" << m_infoFps << "\n"
             << "position=" << m_infoPosition << "\n"
             << "hwdec=" << (m_infoHwdec.empty() ? "no" : m_infoHwdec) << "\n"
             << "frame_drops=" << m_infoFrameDrops << "\n"
             << "decoder_frame_drops=" << m_infoDecoderDrops << "\n";
    }

    PerformanceMonitor::instance().setPlayerInfo(label);

#if defined(_WIN32)
    wchar_t exePathBuf[MAX_PATH];
    GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    std::wstring dir = exePathBuf;
    size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir = dir.substr(0, slash);
    std::wstring path = dir + L"\\playback_info.txt";
    const std::string text = file.str();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        CloseHandle(h);
    }
#endif
}

WindowsPlayerBackend::WindowsPlayerBackend() {
    m_running = true;
    m_writerThread = std::thread(&WindowsPlayerBackend::pipeWriterLoop, this);
    m_fallbackThread = std::thread(&WindowsPlayerBackend::fallbackTickLoop, this);
    m_launchThread = std::thread(&WindowsPlayerBackend::launchWorkerLoop, this);
    LOG_INFO("WindowsPlayerBackend initialized (mpv backend)");
}

WindowsPlayerBackend::~WindowsPlayerBackend() {
    m_running = false;
    m_cmdCv.notify_all();
    {
        std::lock_guard<std::mutex> lock(m_launchMutex);
        m_launchPending = false;
    }
    m_launchCv.notify_all();

    // Stop accepting new launch work first. launchWorkerLoop checks m_running in its wait
    // predicate and any in-flight launchMpvProcess() call also checks m_running inside its
    // pipe-connect retry loop, so this returns promptly instead of blocking shutdown.
    if (m_launchThread.joinable()) {
        m_launchThread.join();
    }

    terminateMpvProcess();

    if (m_writerThread.joinable()) {
        m_writerThread.join();
    }
    if (m_fallbackThread.joinable()) {
        m_fallbackThread.join();
    }
}

void WindowsPlayerBackend::setState(PlaybackState s) {
    m_state = s;
    if (m_stateCb) m_stateCb(s);
}

std::wstring WindowsPlayerBackend::findMpvBinary() {
#if defined(_WIN32)
    // Check possible locations for mpv.exe
    wchar_t exePathBuf[MAX_PATH];
    GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    std::wstring exeDir = exePathBuf;
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }

    std::vector<std::wstring> candidates = {
        exeDir + L"\\tools\\mpv\\mpv.exe",
        exeDir + L"\\mpv.exe",
        L"tools\\mpv\\mpv.exe",
        L"build\\tools\\mpv\\mpv.exe",
        L"D:\\youtube-client\\tools\\mpv\\mpv.exe",
        L"D:\\youtube-client\\build\\tools\\mpv\\mpv.exe"
    };

    for (const auto& path : candidates) {
        DWORD attrib = GetFileAttributesW(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            return path;
        }
    }
#endif
    return L"";
}

std::wstring WindowsPlayerBackend::findYtDlpBinary() {
#if defined(_WIN32)
    wchar_t exePathBuf[MAX_PATH];
    GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    std::wstring exeDir = exePathBuf;
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }

    std::vector<std::wstring> candidates = {
        exeDir + L"\\tools\\mpv\\yt-dlp.exe",
        exeDir + L"\\yt-dlp.exe",
        L"tools\\mpv\\yt-dlp.exe",
        L"build\\tools\\mpv\\yt-dlp.exe",
        L"D:\\youtube-client\\tools\\mpv\\yt-dlp.exe",
        L"D:\\youtube-client\\build\\tools\\mpv\\yt-dlp.exe",
        L"C:\\Users\\User\\AppData\\Local\\Packages\\PythonSoftwareFoundation.Python.3.11_qbz5n2kfra8p0\\LocalCache\\local-packages\\Python311\\Scripts\\yt-dlp.exe"
    };

    for (const auto& path : candidates) {
        DWORD attrib = GetFileAttributesW(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            return path;
        }
    }
#endif
    return L"";
}

bool WindowsPlayerBackend::isMpvAlive() {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(m_procMutex);
    if (m_hProcess == nullptr) return false;
    DWORD exitCode = 0;
    return GetExitCodeProcess(m_hProcess, &exitCode) && exitCode == STILL_ACTIVE
           && m_hPipe != INVALID_HANDLE_VALUE && m_mpvActive;
#else
    return false;
#endif
}

void WindowsPlayerBackend::launchWorkerLoop() {
    // Runs for the lifetime of the backend. All the slow, blocking parts of starting mpv
    // (spawning the process, waiting for its IPC pipe to come up, tearing down a stale
    // session) happen exclusively on this thread so the UI/render thread (which calls
    // play()/stop()/seek() straight out of Win32 message handling) is never blocked.
    while (m_running) {
        std::string uri;
        {
            std::unique_lock<std::mutex> lock(m_launchMutex);
            m_launchCv.wait(lock, [this] { return !m_running || m_launchPending; });
            if (!m_running) break;
            uri = m_pendingLaunchUri;
            m_launchPending = false;
        }
        launchMpvProcess(uri);
    }
}

bool WindowsPlayerBackend::launchMpvProcess(const std::string& initialUri) {
#if defined(_WIN32)
    const std::string pauseCmd = std::string("{\"command\": [\"set_property\", \"pause\", ")
        + (m_mpvPaused ? "true" : "false") + "]}\n";

    if (isMpvAlive()) {
        if (!initialUri.empty()) {
            std::string loadCmd = "{\"command\": [\"loadfile\", \"" + initialUri + "\", \"replace\"]}\n";
            sendIpcCommand(loadCmd);
            sendIpcCommand(pauseCmd);
        }
        return true;
    }

    // Any previous session is either gone or unhealthy - clean it up fully before starting
    // a new one so we never end up with orphaned handles or a stuck reader thread.
    terminateMpvProcess();

    if (!m_videoHwnd) {
        LOG_WARN("Cannot launch embedded MPV: m_videoHwnd is null");
        setState(PlaybackState::ERROR);
        return false;
    }

    std::wstring mpvPath = findMpvBinary();
    if (mpvPath.empty()) {
        LOG_WARN("Embedded MPV binary not found. Falling back to internal timer.");
        setState(PlaybackState::ERROR);
        return false;
    }

    std::wstring ytdlPath = findYtDlpBinary();

    // Unique pipe name per launch attempt (not just per process) so a slow-to-release
    // named pipe from a just-terminated mpv instance can never collide with the new one.
    ++m_launchCounter;
    std::wstring pipeName = L"\\\\.\\pipe\\yt_client_mpv_" + std::to_wstring(GetCurrentProcessId())
        + L"_" + std::to_wstring(m_launchCounter);

    std::wstring cmd = L"\"" + mpvPath + L"\" "
        + L"--wid=" + std::to_wstring(reinterpret_cast<uintptr_t>(m_videoHwnd)) + L" "
        + L"--input-ipc-server=" + pipeName + L" "
        + L"--no-border --no-osc --no-osd-bar "
        // All keyboard/mouse handling belongs to our UI. Without this mpv's own key bindings
        // (q = quit, f = its own fullscreen, arrows = 5 s seek...) grabbed keys whenever the
        // video had focus, and our shortcuts (Esc, F, arrows) silently stopped working.
        + L"--input-default-bindings=no --input-vo-keyboard=no --input-cursor=no "
        + L"--keep-open=yes --idle=yes --force-window=yes "
        + L"--hwdec=" + hwdecMode() + L" "
        + L"--ytdl-format=" + buildYtdlFormat() + L" "
        // mpv keeps 50 MiB of already-played data for instant backward seeks by default.
        // 16 MiB still covers several -10 s jumps at 720p and saves memory.
        + L"--cache=yes --demuxer-max-bytes=32M --demuxer-max-back-bytes=16M --demuxer-readahead-secs=15 ";

    if (!ytdlPath.empty()) {
        std::wstring cleanYtdl = ytdlPath;
        std::replace(cleanYtdl.begin(), cleanYtdl.end(), L'\\', L'/');
        cmd += L"--script-opts=ytdl_hook-ytdl_path=" + cleanYtdl + L" ";
    }

    // This app is not DPI-aware, but mpv is. On displays with Windows scaling above 100% that
    // mismatch made mpv size its video surface in physical pixels inside our (DPI-virtualized)
    // child window, so the picture looked shifted and cropped. Run mpv DPI-unaware as well so
    // both processes agree on the same coordinate space. mpv inherits this variable.
    SetEnvironmentVariableW(L"__COMPAT_LAYER", L"DPIUNAWARE");

    STARTUPINFOW si{};
    si.cb = sizeof(STARTUPINFOW);
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL created = CreateProcessW(
        nullptr,
        cmdBuf.data(),
        nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi
    );

    if (!created) {
        LOG_ERROR("Failed to spawn MPV process. Error code: " + std::to_string(GetLastError()));
        setState(PlaybackState::ERROR);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_procMutex);
        m_hProcess = pi.hProcess;
        m_hThread = pi.hThread;
        m_pipeName = pipeName;
    }

    // Connect to MPV named pipe IPC. This retry loop can take up to ~3 seconds on a slow /
    // resource-constrained machine - that is exactly why it must run on this background
    // thread and never on the UI thread.
    HANDLE pipeHandle = INVALID_HANDLE_VALUE;
    for (int retry = 0; retry < 60 && m_running; ++retry) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        pipeHandle = CreateFileW(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr
        );
        if (pipeHandle != INVALID_HANDLE_VALUE) {
            break;
        }
    }

    if (pipeHandle == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to connect to MPV IPC pipe: " + std::to_string(GetLastError()));
        terminateMpvProcess();
        setState(PlaybackState::ERROR);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_procMutex);
        m_hPipe = pipeHandle;
    }
    m_mpvActive = true;
    LOG_INFO("MPV player process connected via IPC pipe");

    // Start background IPC reader thread
    if (m_readerThread.joinable()) {
        m_readerThread.join();
    }
    m_readerThread = std::thread(&WindowsPlayerBackend::pipeReaderLoop, this);

    // Subscribe to property changes via non-blocking async queue
    sendIpcCommand("{\"command\": [\"observe_property\", 1, \"time-pos\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 2, \"duration\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 3, \"pause\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 4, \"eof-reached\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 5, \"video-codec\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 6, \"hwdec-current\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 7, \"width\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 8, \"height\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 9, \"frame-drop-count\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 10, \"decoder-frame-drop-count\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 11, \"current-tracks/video/codec\"]}\n");
    sendIpcCommand("{\"command\": [\"observe_property\", 12, \"container-fps\"]}\n");
    sendIpcCommand("{\"command\": [\"set_property\", \"volume\", " + std::to_string(m_volume.load()) + "]}\n");

    if (!initialUri.empty()) {
        std::string loadCmd = "{\"command\": [\"loadfile\", \"" + initialUri + "\", \"replace\"]}\n";
        sendIpcCommand(loadCmd);
        sendIpcCommand(pauseCmd);
    }

    // Stay in BUFFERING until mpv reports file-loaded / playback-restart - that is what
    // actually tells us whether we're playing or paused.
    return true;
#else
    return false;
#endif
}

void WindowsPlayerBackend::sendIpcCommand(const std::string& json) {
    std::lock_guard<std::mutex> lock(m_cmdMutex);
    m_pendingCommands.push(json);
    m_cmdCv.notify_one();
}

void WindowsPlayerBackend::pipeWriterLoop() {
    while (m_running) {
        std::string cmd;
        {
            std::unique_lock<std::mutex> lock(m_cmdMutex);
            m_cmdCv.wait(lock, [this] {
                return !m_running || !m_pendingCommands.empty();
            });
            if (!m_running) break;
            cmd = std::move(m_pendingCommands.front());
            m_pendingCommands.pop();
        }

#if defined(_WIN32)
        std::lock_guard<std::mutex> writeLock(m_writeMutex);
        HANDLE pipe = INVALID_HANDLE_VALUE;
        {
            std::lock_guard<std::mutex> procLock(m_procMutex);
            pipe = m_hPipe;
        }
        if (pipe != INVALID_HANDLE_VALUE && !cmd.empty()) {
            if (!overlappedWrite(pipe, cmd, 1000)) {
                LOG_WARN("MPV IPC write failed: " + cmd);
            }
        }
#endif
    }
}

void WindowsPlayerBackend::markSeekPending() {
    m_ignoreTimePosUntilMs = steadyNowMs() + 1500;
}

void WindowsPlayerBackend::handleIpcLine(const std::string& line) {
    if (line.find("\"name\":\"time-pos\"") != std::string::npos) {
        if (steadyNowMs() < m_ignoreTimePosUntilMs.load()) return;
        double cur = 0.0;
        if (extractJsonNumber(line, cur)) {
            int sec = std::max(0, static_cast<int>(cur));
            m_currentPosition = sec;
            {
                std::lock_guard<std::mutex> lock(m_infoMutex);
                m_infoPosition = cur;
            }
            updatePlaybackInfo(false);
            if (m_posCb) m_posCb(sec, m_duration.load());
        }
    } else if (line.find("\"name\":\"duration\"") != std::string::npos) {
        double dur = 0.0;
        if (extractJsonNumber(line, dur)) {
            int sec = static_cast<int>(dur);
            if (sec > 0) {
                m_duration = sec;
                if (m_posCb) m_posCb(m_currentPosition.load(), sec);
            }
        }
    } else if (line.find("\"name\":\"pause\"") != std::string::npos) {
        if (line.find("\"data\":true") != std::string::npos) {
            m_mpvPaused = true;
            if (!m_eofReached && m_state == PlaybackState::PLAYING) {
                setState(PlaybackState::PAUSED);
            }
        } else if (line.find("\"data\":false") != std::string::npos) {
            m_mpvPaused = false;
            // BUFFERING -> PLAYING is left to file-loaded / playback-restart so the badge
            // doesn't claim "PLAYING" while the stream is still being resolved.
            if (!m_eofReached && m_state == PlaybackState::PAUSED) {
                setState(PlaybackState::PLAYING);
            }
        }
    } else if (line.find("\"name\":\"video-codec\"") != std::string::npos ||
               line.find("\"name\":\"hwdec-current\"") != std::string::npos) {
        std::string value;
        if (!extractJsonString(line, value)) value.clear();
        bool isCodec = line.find("\"name\":\"video-codec\"") != std::string::npos;
        {
            std::lock_guard<std::mutex> lock(m_infoMutex);
            (isCodec ? m_infoCodec : m_infoHwdec) = value;
        }
        if (!value.empty()) {
            LOG_INFO(std::string(isCodec ? "MPV video codec: " : "MPV hardware decoding: ") + value);
        }
        updatePlaybackInfo(true);
    } else if (line.find("\"name\":\"current-tracks/video/codec\"") != std::string::npos) {
        std::string value;
        if (!extractJsonString(line, value)) value.clear();
        {
            std::lock_guard<std::mutex> lock(m_infoMutex);
            m_infoCodecShort = value;
        }
        updatePlaybackInfo(true);
    } else if (line.find("\"name\":\"container-fps\"") != std::string::npos) {
        double v = 0.0;
        {
            std::lock_guard<std::mutex> lock(m_infoMutex);
            m_infoFps = extractJsonNumber(line, v) ? v : 0.0;
        }
        updatePlaybackInfo(true);
    } else if (line.find("\"name\":\"width\"") != std::string::npos ||
               line.find("\"name\":\"height\"") != std::string::npos) {
        double v = 0.0;
        int px = extractJsonNumber(line, v) ? static_cast<int>(v) : 0;
        {
            std::lock_guard<std::mutex> lock(m_infoMutex);
            (line.find("\"name\":\"width\"") != std::string::npos ? m_infoWidth : m_infoHeight) = px;
        }
        updatePlaybackInfo(true);
    } else if (line.find("\"name\":\"frame-drop-count\"") != std::string::npos ||
               line.find("\"name\":\"decoder-frame-drop-count\"") != std::string::npos) {
        double v = 0.0;
        long long n = extractJsonNumber(line, v) ? static_cast<long long>(v) : -1;
        {
            std::lock_guard<std::mutex> lock(m_infoMutex);
            (line.find("\"name\":\"decoder-frame-drop-count\"") != std::string::npos ? m_infoDecoderDrops : m_infoFrameDrops) = n;
        }
        updatePlaybackInfo(false);
    } else if (line.find("\"name\":\"eof-reached\"") != std::string::npos) {
        if (line.find("\"data\":true") != std::string::npos) {
            m_eofReached = true;
            m_currentPosition = m_duration.load();
            setState(PlaybackState::STOPPED);
        } else {
            m_eofReached = false;
        }
    } else if (line.find("\"event\":\"file-loaded\"") != std::string::npos ||
               line.find("\"event\":\"playback-restart\"") != std::string::npos) {
        // Seek (or initial load) finished - time-pos is trustworthy again.
        m_ignoreTimePosUntilMs = 0;
        if (m_eofReached) return;
        if (m_state == PlaybackState::STOPPED && line.find("playback-restart") != std::string::npos) {
            return; // stale event after the user pressed Stop
        }
        // Never override a user pause here: mpv also emits playback-restart after every
        // seek, including seeks done while paused.
        setState(m_mpvPaused ? PlaybackState::PAUSED : PlaybackState::PLAYING);
    } else if (line.find("\"event\":\"end-file\"") != std::string::npos) {
        if (line.find("\"reason\":\"eof\"") != std::string::npos) {
            setState(PlaybackState::STOPPED);
        } else if (line.find("\"reason\":\"error\"") != std::string::npos) {
            LOG_ERROR("MPV failed to play file: " + line);
            setState(PlaybackState::ERROR);
        }
    }
}

void WindowsPlayerBackend::pipeReaderLoop() {
#if defined(_WIN32)
    char buffer[4096];
    std::string lineAcc;

    HANDLE pipe;
    {
        std::lock_guard<std::mutex> lock(m_procMutex);
        pipe = m_hPipe;
    }
    if (pipe == INVALID_HANDLE_VALUE) {
        m_mpvActive = false;
        return;
    }

    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        m_mpvActive = false;
        return;
    }

    // terminateMpvProcess() never closes the pipe handle before joining this thread, so the
    // local copy stays valid for the whole loop. Reads are overlapped and polled every 100 ms
    // so this thread notices shutdown promptly and never blocks writes on the same handle.
    while (m_running && m_mpvActive) {
        ResetEvent(ov.hEvent);
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(pipe, buffer, sizeof(buffer) - 1, nullptr, &ov);
        if (!ok) {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING && err != ERROR_MORE_DATA) {
                break;
            }
        }

        bool completed = false;
        while (m_running && m_mpvActive) {
            if (WaitForSingleObject(ov.hEvent, 100) == WAIT_OBJECT_0) {
                completed = true;
                break;
            }
        }
        if (!completed) {
            CancelIoEx(pipe, &ov);
            GetOverlappedResult(pipe, &ov, &bytesRead, TRUE);
            break;
        }

        if (!GetOverlappedResult(pipe, &ov, &bytesRead, FALSE)) {
            if (GetLastError() != ERROR_MORE_DATA) {
                break;
            }
        }
        if (bytesRead == 0) {
            continue;
        }

        lineAcc.append(buffer, bytesRead);

        size_t newlinePos = 0;
        while ((newlinePos = lineAcc.find('\n')) != std::string::npos) {
            std::string line = lineAcc.substr(0, newlinePos);
            lineAcc.erase(0, newlinePos + 1);
            handleIpcLine(line);
        }
    }

    CloseHandle(ov.hEvent);
    m_mpvActive = false;
#endif
}

void WindowsPlayerBackend::terminateMpvProcess() {
#if defined(_WIN32)
    HANDLE pipeToClose = INVALID_HANDLE_VALUE;
    {
        std::lock_guard<std::mutex> lock(m_procMutex);
        pipeToClose = m_hPipe;
        m_hPipe = INVALID_HANDLE_VALUE; // writer thread can no longer pick this handle up
    }

    {
        // Wait for any in-flight write (bounded by its own timeout) before touching the pipe.
        std::lock_guard<std::mutex> writeLock(m_writeMutex);
        if (pipeToClose != INVALID_HANDLE_VALUE) {
            overlappedWrite(pipeToClose, "{\"command\": [\"quit\"]}\n", 300);
        }
    }

    // Reader polls m_mpvActive every 100 ms and cancels its own pending read.
    m_mpvActive = false;
    if (m_readerThread.joinable()) {
        m_readerThread.join();
    }

    if (pipeToClose != INVALID_HANDLE_VALUE) {
        CloseHandle(pipeToClose);
    }

    HANDLE proc = nullptr;
    HANDLE thr = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_procMutex);
        proc = m_hProcess;
        thr = m_hThread;
        m_hProcess = nullptr;
        m_hThread = nullptr;
    }

    if (proc != nullptr) {
        WaitForSingleObject(proc, 500);
        DWORD exitCode = 0;
        if (GetExitCodeProcess(proc, &exitCode) && exitCode == STILL_ACTIVE) {
            TerminateProcess(proc, 0);
        }
        CloseHandle(proc);
    }

    if (thr != nullptr) {
        CloseHandle(thr);
    }
#else
    m_mpvActive = false;
#endif
}

bool WindowsPlayerBackend::play(const VideoSource& source) {
    m_source = source;
    if (source.durationSeconds > 0) {
        m_duration = source.durationSeconds;
    }
    m_currentPosition = 0;
    m_mpvPaused = false;
    m_eofReached = false;
    m_ignoreTimePosUntilMs = 0;

    LOG_INFO("WindowsPlayerBackend starting playback: " + source.streamUri);

    setVideoVisible(true);
    setState(PlaybackState::BUFFERING);

    if (isMpvAlive()) {
        // mpv is already up - just hand it the new file over IPC. This is fast/non-blocking
        // and safe to do straight from the UI thread.
        std::string loadCmd = "{\"command\": [\"loadfile\", \"" + source.streamUri + "\", \"replace\"]}\n";
        sendIpcCommand(loadCmd);
        sendIpcCommand("{\"command\": [\"set_property\", \"pause\", false]}\n");
    } else {
        // No live mpv session (first play, or the previous one died/was stopped). Spawning
        // the process and waiting for its IPC pipe can take seconds - hand it off to the
        // background launch thread instead of blocking the UI thread.
        {
            std::lock_guard<std::mutex> lock(m_launchMutex);
            m_pendingLaunchUri = source.streamUri;
            m_launchPending = true;
        }
        m_launchCv.notify_one();
    }

    return true;
}

void WindowsPlayerBackend::pause() {
    PlaybackState s = m_state;
    if (s != PlaybackState::PLAYING && s != PlaybackState::BUFFERING) {
        return;
    }
    m_mpvPaused = true;
    if (m_mpvActive) {
        sendIpcCommand("{\"command\": [\"set_property\", \"pause\", true]}\n");
    }
    LOG_INFO("WindowsPlayerBackend paused");
    if (s == PlaybackState::PLAYING) {
        setState(PlaybackState::PAUSED);
    }
}

void WindowsPlayerBackend::resume() {
    PlaybackState s = m_state;
    if (s != PlaybackState::PAUSED && s != PlaybackState::BUFFERING) {
        return;
    }
    m_mpvPaused = false;
    if (m_mpvActive) {
        sendIpcCommand("{\"command\": [\"set_property\", \"pause\", false]}\n");
    }
    LOG_INFO("WindowsPlayerBackend resumed");
    if (s == PlaybackState::PAUSED) {
        setState(PlaybackState::PLAYING);
    }
}

void WindowsPlayerBackend::stop() {
    m_currentPosition = 0;
    m_eofReached = false;
    m_ignoreTimePosUntilMs = 0;
    if (m_mpvActive) {
        sendIpcCommand("{\"command\": [\"stop\"]}\n");
    }
    LOG_INFO("WindowsPlayerBackend stopped");
    setState(PlaybackState::STOPPED);
}

void WindowsPlayerBackend::seek(int seconds) {
    int dur = m_duration.load();
    int target = std::clamp(seconds, 0, dur > 0 ? dur : std::max(0, seconds));
    m_currentPosition = target;
    if (m_mpvActive) {
        markSeekPending();
        std::string seekCmd = "{\"command\": [\"seek\", " + std::to_string(target) + ", \"absolute+exact\"]}\n";
        sendIpcCommand(seekCmd);
    }
    LOG_INFO("WindowsPlayerBackend seek to: " + std::to_string(target) + "s");
    if (m_posCb) m_posCb(target, dur);
}

void WindowsPlayerBackend::seekRelative(int seconds) {
    // Seek to an absolute target computed from our own (optimistic) position. Repeated
    // +10/-10 presses then accumulate predictably, and "exact" avoids snapping back to the
    // previous keyframe (which made -10s / +10s land in odd places or not move at all).
    seek(m_currentPosition.load() + seconds);
}

void WindowsPlayerBackend::setVolume(int volumePercent) {
    m_volume = std::clamp(volumePercent, 0, 100);
    if (m_mpvActive) {
        std::string volCmd = "{\"command\": [\"set_property\", \"volume\", " + std::to_string(m_volume.load()) + "]}\n";
        sendIpcCommand(volCmd);
    }
}

void WindowsPlayerBackend::setVideoOutputWindow(void* nativeWindowHandle) {
#if defined(_WIN32)
    m_videoHwnd = reinterpret_cast<HWND>(nativeWindowHandle);
    LOG_INFO("Attached child HWND to WindowsPlayerBackend: " + std::to_string(reinterpret_cast<uintptr_t>(m_videoHwnd)));
#else
    (void)nativeWindowHandle;
#endif
}

void WindowsPlayerBackend::setVideoOutputBounds(int x, int y, int width, int height) {
#if defined(_WIN32)
    if (m_lastX == x && m_lastY == y && m_lastW == width && m_lastH == height) {
        return;
    }
    m_lastX = x; m_lastY = y; m_lastW = width; m_lastH = height;
    if (m_videoHwnd) {
        MoveWindow(m_videoHwnd, x, y, width, height, TRUE);
    }
#else
    (void)x; (void)y; (void)width; (void)height;
#endif
}

void WindowsPlayerBackend::setVideoVisible(bool visible) {
#if defined(_WIN32)
    int cur = visible ? 1 : 0;
    if (m_lastVisible == cur) {
        return;
    }
    m_lastVisible = cur;
    if (m_videoHwnd) {
        ShowWindow(m_videoHwnd, visible ? SW_SHOW : SW_HIDE);
    }
#else
    (void)visible;
#endif
}

void WindowsPlayerBackend::fallbackTickLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        // Only run fallback timer if mpv is not active and state is PLAYING
        if (!m_mpvActive && m_state == PlaybackState::PLAYING) {
            static int counter = 0;
            if (++counter >= 4) {
                counter = 0;
                int cur = m_currentPosition.load();
                int dur = m_duration.load();
                if (cur < dur) {
                    m_currentPosition.fetch_add(1);
                    if (m_posCb) m_posCb(m_currentPosition.load(), dur);
                } else {
                    setState(PlaybackState::STOPPED);
                }
            }
        }
    }
}

} // namespace yt
