#include "pch.h"
#include "core/mod.h"
#include "core/logger.h"
#include <process.h>

static HANDLE g_initThreadHandle = nullptr;

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
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    (void)lpReserved;

    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);

            g_initThreadHandle = (HANDLE)_beginthreadex(nullptr, 0, InitThread, nullptr, 0, nullptr);
            break;

        case DLL_PROCESS_DETACH:
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
