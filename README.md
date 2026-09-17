# Lightweight YouTube Client

A native Windows YouTube client written in C++17. No Chromium, no Electron, no WebView.
The UI is plain Win32 + GDI, and video is played by [mpv](https://mpv.io) with
[yt-dlp](https://github.com/yt-dlp/yt-dlp).

> Unofficial project. Not affiliated with, endorsed by, or connected to YouTube or Google.

![Playing a video: the whole app, player included, uses about 250 MB](docs/images/video.png)

## Why I built this

My old laptop had **4 GB of RAM**. Watching YouTube on it was painful. A browser with a
single YouTube tab took a big share of that memory, the CPU was busy the whole time,
and everything else I had open slowed down with it.

The video itself was never the heavy part. The heavy part was the full web browser wrapped
around it: a JavaScript app, a page layout engine and several helper processes, all running
just to show one video.

So I wrote a client that does only the job I need: browse, search and play. It uses a small
native window for the UI and hands the video straight to mpv.

## How much lighter is it?

I measured the same video
([Three Adventures in Accessible Music Technology](https://www.youtube.com/watch?v=V5Asw04DsD8))
playing in this app and in Google Chrome. Chrome used a clean, separate profile with no
extensions and no other tabs. Each number is the average over 30 seconds, with the player
window in front.

| While playing the same video | This app (UI + mpv) | Chrome (clean profile) | Difference |
|---|---:|---:|---:|
| Processes | 2 | 11 | |
| Private memory | **217 MB** | **803 MB** | ~3.7× less (~590 MB saved) |
| Working set | 244 MB | 1,210 MB | ~5× less |
| CPU (% of all cores) | 1.2 % | 1.7 % | about the same |

When no video is playing, the app uses about **23 MB** (working set) and 5 MB of private
memory. mpv only starts when you open the first video. After that it stays running in the
background, so the next video opens faster.

**How to read these numbers**

- Private memory is the fairest comparison. Chrome's working set counts shared pages once
  for every process, so it makes Chrome look a bit bigger than it really is.
- CPU was low for both. The test machine is fast (Intel Core i5-13420H, 12 threads,
  16 GB RAM) and both players use hardware video decoding. So on a modern PC the real
  saving is memory, not CPU. I haven't measured CPU on an older machine yet.
- The app is limited to 720p. Chrome used YouTube's automatic quality, and it was not
  logged in.
- Measured on one machine, on 17 September 2026. Your numbers will be different.

You can repeat the test yourself. The scripts are in [`tools/bench`](tools/bench), and the
raw results are in [`tools/bench/results`](tools/bench/results):

1. Run the app, open a video, then run `measure_app_playing.bat`. Bring the app window to
   the front within 10 seconds.
2. Close the app, run `start_browser.bat`, then run `measure_browser_playing.bat`. Bring
   the browser window to the front within 10 seconds.

The browser script only counts processes that use the benchmark profile, so your normal
browser and apps that embed WebView2 are not included.

The app also shows its own numbers live in the status bar, for example:
`App: 22.0 MB | Player (mpv): 229.1 MB | Total: 251.1 MB | CPU: 1.1%`.

## Screenshots

| Home feed | Search |
|---|---|
| ![Home feed](docs/images/home.png) | ![Search](docs/images/search.png) |

## Features

- Home feed and search, including non-ASCII search text such as `lofi müzik`. No API key needed.
- Embedded mpv playback with play/pause, stop, ±10 s, a seek bar, mute and fullscreen
  (the controls come back when you move the mouse)
- Thumbnail cache with a fixed memory limit (16 MB by default)
- UI capped at 30 FPS, and all network work runs on background threads
- Live RAM/CPU readout for the app and the player

### Keyboard shortcuts (video screen)

| Key | Action |
|---|---|
| Space | Play / pause |
| ← / → | Seek −10 s / +10 s |
| F or Enter | Toggle fullscreen |
| M | Mute |
| Esc | Exit fullscreen / back to feed |

Double-click the video to toggle fullscreen.

## Building (Windows)

Requirements:

- A C++17 compiler: [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw) (what I use), MinGW-w64 or MSVC
- [CMake](https://cmake.org) 3.16 or newer, and [Ninja](https://ninja-build.org)
- **mpv** and **yt-dlp**. These are not in the repository (`mpv.exe` is larger than GitHub's
  100 MB file limit).
  - Download an mpv Windows build from https://mpv.io/installation/ and extract it into `tools/mpv/`
    (so you have `tools/mpv/mpv.exe`).
  - Put `yt-dlp.exe` from https://github.com/yt-dlp/yt-dlp/releases into `tools/mpv/` too.

```powershell
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# run (the build copies tools/, config/ and assets/ next to the exe)
.\build\youtube-client.exe

# unit tests
.\build\run_tests.exe
```

Settings are in [`config/app_config.json`](config/app_config.json): feed and search sizes,
cache size, FPS cap, and an optional YouTube Data API key.

## How it works

```
Win32 window + GDI renderer  (UI thread, 30 FPS)
 ├─ Screens: Home · Search · Subscriptions · Video · Account
 ├─ ThreadPool + EventQueue   (HTTP, parsing, thumbnail decoding off the UI thread)
 ├─ ThumbnailCache            (bounded LRU)
 └─ PlayerController → WindowsPlayerBackend
        └─ mpv.exe --wid=<child window>   (JSON IPC over a named pipe, overlapped I/O)
               └─ yt-dlp                  (resolves the stream, 720p max)
```

The UI and the player are separate processes. The UI never waits on the player: mpv is
started and controlled from background threads, so a slow stream never freezes the window.

## Known limitations

- Windows only.
- Subscriptions show sample channels, and login (OAuth device flow) is experimental. Real
  subscriptions need your own Google API credentials.
- View counts in lists are placeholders.
- The app isn't DPI-aware yet, so on displays with Windows scaling above 100 % the UI
  looks slightly soft (Windows stretches it).
- The feed and search results come from YouTube's public web pages, so a YouTube change can
  break them until the parser is updated.

## Credits

- [mpv](https://mpv.io) for playback
- [yt-dlp](https://github.com/yt-dlp/yt-dlp) for stream extraction
- [stb_image](https://github.com/nothings/stb) for thumbnail decoding
