#include "pch.h"
#include "splash_skipper.h"
#include "engine_camera_hook.h"
#include "core/logger.h"
#include <thread>
#include <atomic>

namespace DL2HT {

static std::thread g_skipperThread;
static std::atomic<bool> g_stopFlag{false};
static std::atomic<bool> g_running{false};

// Cap the splash-skip window. By 60 seconds the publisher logos, intro
// videos, and the load-to-main-menu transition have all completed even on
// a slow drive. After that we MUST stop posting keys or we'd start
// dismissing menus the user actually wanted.
static constexpr ULONGLONG kMaxSkipMs = 60000;
static constexpr DWORD kKeyCadenceMs = 400;

struct WindowSearch {
    DWORD pid = 0;
    HWND result = nullptr;
};

static BOOL CALLBACK EnumWindowProc(HWND hWnd, LPARAM lParam) {
    WindowSearch* search = reinterpret_cast<WindowSearch*>(lParam);
    DWORD wpid = 0;
    GetWindowThreadProcessId(hWnd, &wpid);
    if (wpid != search->pid) return TRUE;
    if (GetWindow(hWnd, GW_OWNER) != nullptr) return TRUE;
    if (!IsWindowVisible(hWnd)) return TRUE;

    wchar_t title[256] = {};
    if (GetWindowTextW(hWnd, title, 256) == 0) return TRUE;

    search->result = hWnd;
    return FALSE;
}

static HWND FindGameWindow() {
    WindowSearch search;
    search.pid = GetCurrentProcessId();
    EnumWindows(EnumWindowProc, reinterpret_cast<LPARAM>(&search));
    return search.result;
}

static void PostKey(HWND hWnd, WORD vk) {
    LPARAM scan = (LPARAM)(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16);
    PostMessageW(hWnd, WM_KEYDOWN, vk, 0x00000001 | scan);
    PostMessageW(hWnd, WM_KEYUP,   vk, 0xC0000001 | scan);
}

static void SkipperThread() {
    Logger::Instance().Info("Splash skipper started");

    HWND hWnd = nullptr;
    for (int i = 0; i < 100 && !g_stopFlag.load() && !hWnd; i++) {
        hWnd = FindGameWindow();
        if (!hWnd) Sleep(100);
    }

    if (!hWnd) {
        Logger::Instance().Warning("Splash skipper: never located game window, giving up");
        return;
    }
    Logger::Instance().Info("Splash skipper: targeting hwnd=%p", hWnd);

    ULONGLONG started = GetTickCount64();
    bool toggleSpace = false;

    while (!g_stopFlag.load()) {
        if (GetTickCount64() - started > kMaxSkipMs) {
            Logger::Instance().Info("Splash skipper: timeout reached, stopping");
            break;
        }
        if (IsAtMainMenu()) {
            Logger::Instance().Info("Splash skipper: main menu detected, stopping");
            break;
        }

        PostKey(hWnd, VK_ESCAPE);
        // Alternate ESC and SPACE so cutscene "press space to continue"
        // prompts also dismiss without us having to know which prompt is up.
        if (toggleSpace) PostKey(hWnd, VK_SPACE);
        toggleSpace = !toggleSpace;

        Sleep(kKeyCadenceMs);
    }

    Logger::Instance().Info("Splash skipper exiting");
}

void StartSplashSkipper() {
    if (g_running.exchange(true)) return;
    g_stopFlag.store(false);
    g_skipperThread = std::thread(SkipperThread);
}

void StopSplashSkipper() {
    if (!g_running.load()) return;
    g_stopFlag.store(true);
    if (g_skipperThread.joinable()) g_skipperThread.join();
    g_running.store(false);
}

} // namespace DL2HT
