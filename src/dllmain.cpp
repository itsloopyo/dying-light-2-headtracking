#include "pch.h"
#include "core/mod.h"
#include "core/logger.h"
#include <process.h>

static HANDLE g_initThreadHandle = nullptr;

// Use _beginthreadex signature for proper CRT thread cleanup
unsigned __stdcall InitThread(void* lpParam) {
    (void)lpParam;

    // Wait for game DLL to be loaded
    int waitAttempts = 0;
    constexpr int maxWaitAttempts = 100; // 10 seconds max
    while (!GetModuleHandleA(DL2HT::DL2_GAME_DLL)) {
        Sleep(100);
        waitAttempts++;
        if (waitAttempts >= maxWaitAttempts) {
            // Not the game process (e.g. launcher) — exit silently
            return 1;
        }
    }

    // Game DLL found — now it's safe to open the debug console and start logging
    DL2HT::Logger::Instance().Initialize();
    DL2HT::Logger::Instance().Info("DL2 Head Tracking v%s attached to game process", DL2HT::DL2HT_VERSION);

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
