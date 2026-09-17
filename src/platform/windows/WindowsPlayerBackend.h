#pragma once

#include "player/IVideoPlayer.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace yt {

class WindowsPlayerBackend : public IVideoPlayer {
public:
    WindowsPlayerBackend();
    ~WindowsPlayerBackend() override;

    bool play(const VideoSource& source) override;
    void pause() override;
    void resume() override;
    void stop() override;
    void seek(int seconds) override;
    void seekRelative(int seconds) override;

    PlaybackState getState() const override { return m_state; }
    int getCurrentPosition() const override { return m_currentPosition; }
    int getDuration() const override { return m_duration; }
    void setVolume(int volumePercent) override;
    int getVolume() const override { return m_volume; }

    void setStateCallback(StateCallback cb) override { m_stateCb = std::move(cb); }
    void setPositionCallback(PositionCallback cb) override { m_posCb = std::move(cb); }

    void setVideoOutputWindow(void* nativeWindowHandle) override;
    void setVideoOutputBounds(int x, int y, int width, int height) override;
    void setVideoVisible(bool visible) override;

private:
    void pipeReaderLoop();
    void pipeWriterLoop();
    void fallbackTickLoop();
    void launchWorkerLoop();

    void sendIpcCommand(const std::string& json);
    bool launchMpvProcess(const std::string& initialUri = "");
    void terminateMpvProcess();
    bool isMpvAlive();
    void setState(PlaybackState s);
    void handleIpcLine(const std::string& line);
    void markSeekPending();
    std::wstring findMpvBinary();
    std::wstring findYtDlpBinary();

    std::atomic<PlaybackState> m_state{PlaybackState::IDLE};
    VideoSource m_source;
    std::atomic<int> m_currentPosition{0};
    std::atomic<int> m_duration{0};
    std::atomic<int> m_volume{100};

    // Last known value of mpv's own "pause" property (or the value we asked for before mpv
    // was up). Used so file-loaded / playback-restart events never override a user pause.
    std::atomic<bool> m_mpvPaused{false};
    // mpv runs with --keep-open=yes, so at the end of a file it pauses instead of emitting
    // end-file. Track eof-reached so that pause event doesn't show up as a user "PAUSED".
    std::atomic<bool> m_eofReached{false};
    // While a seek is in flight mpv still emits a few stale time-pos values; ignore them
    // until playback-restart (or this deadline) so the seek bar doesn't jump back and forth.
    std::atomic<long long> m_ignoreTimePosUntilMs{0};

    StateCallback m_stateCb;
    PositionCallback m_posCb;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_mpvActive{false};

    std::thread m_readerThread;
    std::thread m_writerThread;
    std::thread m_fallbackThread;
    std::thread m_launchThread; // Owns all slow mpv spawn / pipe-connect work so the UI thread never blocks

    std::queue<std::string> m_pendingCommands;
    std::mutex m_cmdMutex;
    std::condition_variable m_cmdCv;

    // Hand-off queue from play() (called on the UI thread) to launchWorkerLoop() (background thread).
    // play() only ever pushes a request here and returns immediately - it never spawns or waits
    // on the mpv process/pipe itself.
    std::mutex m_launchMutex;
    std::condition_variable m_launchCv;
    bool m_launchPending{false};
    std::string m_pendingLaunchUri;

    // Guards m_hProcess / m_hPipe / m_pipeName so the UI thread (isMpvAlive/play) and the
    // launch/writer threads never race on the raw handles.
    std::mutex m_procMutex;
    // Serializes actual pipe writes. Kept separate from m_procMutex so the UI thread
    // (isMpvAlive) never waits on pipe I/O.
    std::mutex m_writeMutex;

    unsigned long long m_launchCounter{0};

#if defined(_WIN32)
    HWND m_videoHwnd{nullptr};
    HANDLE m_hProcess{nullptr};
    HANDLE m_hThread{nullptr};
    HANDLE m_hPipe{INVALID_HANDLE_VALUE};
    std::wstring m_pipeName;
    int m_lastX{-1};
    int m_lastY{-1};
    int m_lastW{-1};
    int m_lastH{-1};
    int m_lastVisible{-1};
#endif
};

} // namespace yt
