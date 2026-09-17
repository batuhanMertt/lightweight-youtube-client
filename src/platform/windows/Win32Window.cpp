#include "platform/windows/Win32Window.h"
#if defined(_WIN32)
#include "ui/AppUI.h"
#include "core/Logger.h"
#include <windowsx.h>

namespace yt {

Win32Window::Win32Window() = default;

Win32Window::~Win32Window() {
    close();
}

bool Win32Window::create(const std::string& title, int width, int height) {
    m_width = width;
    m_height = height;

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32Window::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"LightweightYouTubeClientWindow";

    RegisterClassExW(&wc);

    RECT wr = {0, 0, width, height};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, FALSE);

    // Unicode window so WM_CHAR delivers UTF-16 (Turkish characters like ü, ş, ğ, İ work in
    // the search box regardless of the system ANSI code page).
    std::wstring wTitle;
    int titleLen = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
    if (titleLen > 0) {
        wTitle.resize(static_cast<size_t>(titleLen));
        MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, &wTitle[0], titleLen);
        wTitle.resize(static_cast<size_t>(titleLen - 1));
    }

    m_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        wTitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        nullptr,
        nullptr,
        hInstance,
        this
    );

    if (!m_hwnd) {
        LOG_ERROR("Failed to create Win32 native window");
        return false;
    }

    // Register dedicated video host window class with mouse forwarding
    WNDCLASSEXW vwc{};
    vwc.cbSize = sizeof(WNDCLASSEXW);
    vwc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    vwc.lpfnWndProc = [](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
        if (m == WM_ERASEBKGND) return 1;
        if (m == WM_PAINT) {
            // Paint the host black. Without this the area mpv hasn't covered yet (while
            // loading, letterbox, after Stop) kept showing stale pixels of the previous screen.
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            FillRect(dc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
            EndPaint(h, &ps);
            return 0;
        }
        HWND parent = GetParent(h);
        if (parent) {
            if (m == WM_MOUSEMOVE || m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK) {
                POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
                MapWindowPoints(h, parent, &pt, 1);
                SendMessageW(parent, m, w, MAKELPARAM(pt.x, pt.y));
                return 0;
            }
        }
        return DefWindowProcW(h, m, w, l);
    };
    vwc.hInstance = hInstance;
    vwc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    vwc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    vwc.lpszClassName = L"VideoHostClass";
    RegisterClassExW(&vwc);

    // Create child video canvas window for native embedded video rendering (e.g. mpv --wid)
    m_videoHwnd = CreateWindowExW(
        0,
        L"VideoHostClass",
        L"",
        WS_CHILD | WS_CLIPSIBLINGS,
        30, 96, width - 60, 340,
        m_hwnd,
        nullptr,
        hInstance,
        nullptr
    );

    m_renderer.initialize(m_hwnd, width, height);
    LOG_INFO("Win32 Native Window created (" + std::to_string(width) + "x" + std::to_string(height) + ") with child video host");
    return true;
}

void Win32Window::show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOWNORMAL);
        UpdateWindow(m_hwnd);
        SetForegroundWindow(m_hwnd);
        BringWindowToTop(m_hwnd);
    }
}

bool Win32Window::processEvents(AppUI& ui) {
    m_currentUI = &ui;
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_shouldClose = true;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !m_shouldClose;
}

void Win32Window::present() {
    if (m_hwnd) {
        HDC screenDC = GetDC(m_hwnd);
        m_renderer.endFrame(screenDC);
        ReleaseDC(m_hwnd, screenDC);
    }
}

void Win32Window::close() {
    if (m_videoHwnd) {
        DestroyWindow(m_videoHwnd);
        m_videoHwnd = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_shouldClose = true;
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32Window* self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<Win32Window*>(pCreate->lpCreateParams);
        if (self) {
            self->m_hwnd = hwnd;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    } else {
        self = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Win32Window::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    UIEvent event;

    switch (msg) {
        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE) SetFocus(hwnd);
            return 0;

        case WM_CLOSE:
            m_shouldClose = true;
            PostQuitMessage(0);
            return 0;

        case WM_SIZE: {
            int newW = LOWORD(lParam);
            int newH = HIWORD(lParam);
            if (newW > 0 && newH > 0) {
                m_width = newW;
                m_height = newH;
                m_renderer.resize(newW, newH);
                if (m_currentUI) {
                    event.type = EventType::WINDOW_RESIZE;
                    event.windowWidth = newW;
                    event.windowHeight = newH;
                    m_currentUI->handleEvent(event);
                }
            }
            return 0;
        }

        case WM_MOUSEMOVE:
            event.type = EventType::MOUSE_MOVE;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            if (m_currentUI) m_currentUI->handleEvent(event);
            return 0;

        case WM_LBUTTONDOWN:
            // Capture so we still get WM_LBUTTONUP if the user drags the seek bar outside
            // the window - otherwise the seek-drag state would get stuck.
            SetCapture(hwnd);
            // Clicking the video gives keyboard focus to mpv's (other-process) child window,
            // which then swallows keys like Esc. Keep focus on our own window.
            if (GetFocus() != hwnd) SetFocus(hwnd);
            event.type = EventType::MOUSE_DOWN;
            event.button = 0;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            if (m_currentUI) m_currentUI->handleEvent(event);
            return 0;

        case WM_LBUTTONUP:
            if (GetCapture() == hwnd) ReleaseCapture();
            event.type = EventType::MOUSE_UP;
            event.button = 0;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            if (m_currentUI) m_currentUI->handleEvent(event);
            return 0;

        case WM_LBUTTONDBLCLK:
            event.type = EventType::MOUSE_DBLCLICK;
            event.button = 0;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            if (m_currentUI) m_currentUI->handleEvent(event);
            return 0;

        case WM_MOUSEWHEEL: {
            event.type = EventType::MOUSE_SCROLL;
            int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            event.scrollDelta = delta;
            POINT pt{LOWORD(lParam), HIWORD(lParam)};
            ScreenToClient(hwnd, &pt);
            event.mouseX = pt.x;
            event.mouseY = pt.y;
            if (m_currentUI) m_currentUI->handleEvent(event);
            return 0;
        }

        case WM_KEYDOWN:
            event.type = EventType::KEY_DOWN;
            event.keyCode = static_cast<int>(wParam);
            if (m_currentUI) m_currentUI->handleEvent(event);
            return 0;

        case WM_CHAR: {
            // UTF-16 code unit -> UTF-8 bytes, delivered to the UI one byte per event.
            wchar_t wc = static_cast<wchar_t>(wParam);
            wchar_t units[2];
            int unitCount = 0;
            if (wc >= 0xD800 && wc <= 0xDBFF) {
                m_pendingHighSurrogate = wc;
                return 0;
            } else if (wc >= 0xDC00 && wc <= 0xDFFF) {
                if (m_pendingHighSurrogate == 0) return 0;
                units[0] = m_pendingHighSurrogate;
                units[1] = wc;
                unitCount = 2;
                m_pendingHighSurrogate = 0;
            } else {
                units[0] = wc;
                unitCount = 1;
            }
            char utf8[8];
            int byteCount = WideCharToMultiByte(CP_UTF8, 0, units, unitCount, utf8, sizeof(utf8), nullptr, nullptr);
            for (int i = 0; i < byteCount; ++i) {
                UIEvent ch;
                ch.type = EventType::CHAR_INPUT;
                ch.charCode = utf8[i];
                if (m_currentUI) m_currentUI->handleEvent(ch);
            }
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent flicker with double buffering
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace yt
#endif
