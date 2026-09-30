#include "pch.h"
#include "core/mod.h"
#include "core/logger.h"
#include <process.h>

static HANDLE g_initThreadHandle = nullptr;

static void CenterStartupWindow(HWND window) {
    if ((GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION) != WS_CAPTION ||
        IsZoomed(window) || IsIconic(window)) return;

    RECT work{}, rect{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0) ||
        !GetWindowRect(window, &rect)) {
        DL2HT::Logger::Instance().Error("Cannot read startup window/work area: Win32 %lu", GetLastError());
        return;
    }

    const LONG x = work.left + std::max(0L, (work.right - work.left - (rect.right - rect.left)) / 2);
    const LONG y = work.top + std::max(0L, (work.bottom - work.top - (rect.bottom - rect.top)) / 2);
    if (!SetWindowPos(window, nullptr, x, y, 0, 0,
                      SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS)) {
        DL2HT::Logger::Instance().Error("Cannot center startup window: Win32 %lu", GetLastError());
        return;
    }
    DL2HT::Logger::Instance().Info("Startup window centering: %ld,%ld, work area %ld,%ld-%ld,%ld",
                            x, y, work.left, work.top, work.right, work.bottom);
}

static BOOL CALLBACK CenterGameWindow(HWND window, LPARAM context) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(window)) return TRUE;
    wchar_t name[64]{};
    if (!GetClassNameW(window, name, 64) || wcscmp(name, L"techland_game_class") != 0) return TRUE;
    CenterStartupWindow(window);
    *reinterpret_cast<bool*>(context) = true;
    return TRUE;
}

// Use _beginthreadex signature for proper CRT thread cleanup
unsigned __stdcall InitThread(void* lpParam) {
    (void)lpParam;

    // The log opens (and rotates) before the wait below, not after it. A
    // renamed engine DLL or the ASI loading into a wrapper process used to
    // return from the wait with the log never opened and the previous run's
    // log never rotated, so the user sent an earlier launch's file believing
    // it was the current one, and "engine DLL never appeared" was
    // indistinguishable from "ASI loader not installed".
    if (!DL2HT::Logger::Instance().Initialize()) {
        return 1;
    }
    DL2HT::Logger::Instance().Info("DL2 Head Tracking v%s attached; waiting for %s",
                                   DL2HT::DL2HT_VERSION, DL2HT::DL2_GAME_DLL);

    // Wait for game DLL to be loaded
    int waitAttempts = 0;
    constexpr int maxWaitAttempts = 100; // 10 seconds max
    while (!GetModuleHandleA(DL2HT::DL2_GAME_DLL)) {
        Sleep(100);
        waitAttempts++;
        if (waitAttempts >= maxWaitAttempts) {
            DL2HT::Logger::Instance().Warning(
                "%s did not appear within %d seconds. This is not the game process "
                "(a launcher or wrapper), or the engine DLL has been renamed. "
                "Head tracking is inactive here.",
                DL2HT::DL2_GAME_DLL, maxWaitAttempts / 10);
            return 1;
        }
    }
    DL2HT::Logger::Instance().Info("%s loaded after %d ms", DL2HT::DL2_GAME_DLL,
                                   waitAttempts * 100);

    // Additional delay for game initialization
    Sleep(1000);

    // Initialize the mod
    if (!DL2HT::Mod::Instance().Initialize()) {
        DL2HT::Logger::Instance().Error("Mod initialization failed");
        return 1;
    }

    DL2HT::Logger::Instance().Info("DL2 Head Tracking v%s loaded successfully", DL2HT::DL2HT_VERSION);
    bool windowFound = false;
    for (int attempt = 0; attempt < 300 && !windowFound; ++attempt) {
        if (!EnumWindows(CenterGameWindow, reinterpret_cast<LPARAM>(&windowFound))) {
            DL2HT::Logger::Instance().Error("Cannot enumerate startup windows: Win32 %lu", GetLastError());
            return 1;
        }
        if (!windowFound) Sleep(100);
    }
    if (!windowFound) {
        DL2HT::Logger::Instance().Error("Cannot center startup window: game window did not appear within 30 seconds");
        return 1;
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);

            g_initThreadHandle = (HANDLE)_beginthreadex(nullptr, 0, InitThread, nullptr, 0, nullptr);
            break;

        case DLL_PROCESS_DETACH:
            // A non-null lpReserved means the process is exiting: every other thread is already
            // gone and DLLs such as d3d12 may have run their own detach, so releasing COM objects
            // or suspending threads to unhook here can crash the game on the way out.
            if (lpReserved) break;
            if (g_initThreadHandle) {
                WaitForSingleObject(g_initThreadHandle, 2000);
                CloseHandle(g_initThreadHandle);
                g_initThreadHandle = nullptr;
            }

            DL2HT::Mod::Instance().Shutdown();
            DL2HT::Logger::Instance().Shutdown();
            break;
    }
    return TRUE;
}
