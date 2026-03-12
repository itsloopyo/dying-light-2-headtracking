#include "pch.h"
#include "engine_camera_hook.h"
#include "dx_hook.h"
#include "hook_manager.h"
#include "core/mod.h"
#include "core/logger.h"
#include "core/rotation_math.h"
#include <cameraunlock/memory/pattern_scanner.h>

namespace DL2HT {

// Constants
static constexpr ULONGLONG GAMEPLAY_DETECTION_THRESHOLD_MS = 100;
static constexpr ULONGLONG POST_LOADING_WARMUP_MS = 1500;

// Game structure offsets (validated via pointer chain dump)
// Pattern scan → CLobbySteam global → +0xF8 → CGame → +0x390 → CLevel → +0x20 → LevelDI
static constexpr int CLOBBYSTEAM_TO_CGAME_OFFSET = 0xF8;
static constexpr int CGAME_TO_CLEVEL_OFFSET = 0x390;
static constexpr int CLEVEL_TO_LEVELDI_OFFSET = 0x20;
static constexpr int CGAME_LEVEL_NAME_OFFSET = 0x3B0;  // inline string, "menu_level" on main menu

// Function signatures
typedef bool (*IsLoadingFunc_t)(void* pLevel);
typedef bool (*IsTimerFrozenFunc_t)(void* pLevel);
typedef void (*MoveCameraFunc_t)(void* thisCamera, void* forward, void* up, void* position);

// Hook state - MinHook function pointers and state
struct CameraHookState {
    void* pMoveCameraFunc = nullptr;
    void* pMoveCameraOriginal = nullptr;
    bool hookInstalled = false;
    bool dxHookInitialized = false;
};

// Tracking state - only the enabled flag is needed since rotation/position
// are now processed directly in MoveCameraHook (not cached from DX thread)
struct TrackingState {
    std::atomic<bool> enabled{false};
};

// Cached gameplay state - updated once per frame from DX hook
// Relaxed ordering is safe - these are just boolean flags for state checking
struct GameplayStateCache {
    std::atomic<bool> levelLoading{true};
    std::atomic<bool> timerFrozen{true};             // game timer frozen (paused/menu)
    std::atomic<bool> onMainMenu{true};              // on main menu (level name = "menu_level")
    std::atomic<bool> wasLoading{false};             // loading state on previous frame
    std::atomic<ULONGLONG> loadingEndedTick{0};      // when loading transitioned true→false
    std::atomic<uintptr_t> lastLevelDI{0};           // track LevelDI pointer changes
    std::atomic<ULONGLONG> levelChangedTick{0};      // when LevelDI pointer changed
    std::atomic<ULONGLONG> lastFrameTick{0};
    std::atomic<ULONGLONG> lastCameraHookTick{0};
    std::atomic<bool> headTrackingAppliedThisFrame{false};
    std::atomic<ULONGLONG> lastHeadTrackingAppliedTick{0};
};

// Game state detection pointers
struct GameStateDetection {
    void** pCLobbySteamPtr = nullptr;
    IsLoadingFunc_t pIsLoadingFunc = nullptr;
    IsTimerFrozenFunc_t pIsTimerFrozenFunc = nullptr;
    bool initialized = false;
};

// Global state instances
static CameraHookState g_hook;
static TrackingState g_tracking;
static GameplayStateCache g_gameplayCache;
static GameStateDetection g_gameState;

// Initialize game state detection
// Pattern: 48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 83 C0
// This is: mov rax, [rip+offset]; test rax,rax; jz; add rax,...
static bool InitializeGameStateDetection() {
    HMODULE engineModule = GetModuleHandleA("engine_x64_rwdi.dll");
    if (!engineModule) return false;

    // Find CLobbySteam pointer pattern using core library
    void* result = cameraunlock::memory::ScanPattern(
        engineModule,
        "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 83 C0"
    );

    if (result) {
        // Resolve RIP-relative address: offset at position 3, instruction length 7
        g_gameState.pCLobbySteamPtr = static_cast<void**>(
            cameraunlock::memory::ResolveRIPRelative(result, 3, 7)
        );
        Logger::Instance().Info("Found CLobbySteam ptr at %p", g_gameState.pCLobbySteamPtr);
    }

    // Get IsLoading function from engine exports
    g_gameState.pIsLoadingFunc = (IsLoadingFunc_t)GetProcAddress(engineModule, "?IsLoading@ILevel@@QEBA_NXZ");
    if (g_gameState.pIsLoadingFunc) {
        Logger::Instance().Info("Found ILevel::IsLoading at %p", g_gameState.pIsLoadingFunc);
    }

    // ILevel::IsTimerFrozen() const — timer frozen when game is paused
    g_gameState.pIsTimerFrozenFunc = (IsTimerFrozenFunc_t)GetProcAddress(engineModule, "?IsTimerFrozen@ILevel@@QEBA_NXZ");
    if (g_gameState.pIsTimerFrozenFunc) {
        Logger::Instance().Info("Found ILevel::IsTimerFrozen at %p", g_gameState.pIsTimerFrozenFunc);
    }

    g_gameState.initialized = (g_gameState.pCLobbySteamPtr != nullptr);
    return g_gameState.initialized;
}

// Get the LevelDI pointer via the pattern-scanned global
// Chain: CLobbySteam → +0xF8 → CGame → +0x390 → CLevel → +0x20 → LevelDI
// Returns nullptr if any pointer in the chain is invalid
static void* GetLevelDI() {
    if (!g_gameState.initialized || !g_gameState.pCLobbySteamPtr)
        return nullptr;

    __try {
        void* pCLobbySteam = *g_gameState.pCLobbySteamPtr;
        if (!pCLobbySteam) return nullptr;

        void* pCGame = *(void**)((BYTE*)pCLobbySteam + CLOBBYSTEAM_TO_CGAME_OFFSET);
        if (!pCGame) return nullptr;

        void* pCLevel = *(void**)((BYTE*)pCGame + CGAME_TO_CLEVEL_OFFSET);
        if (!pCLevel) return nullptr;

        void* pLevelDI = *(void**)((BYTE*)pCLevel + CLEVEL_TO_LEVELDI_OFFSET);
        return pLevelDI;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// Check if level is currently loading using game's own IsLoading function
// Returns true (loading) when state cannot be determined - this disables
// head tracking during uncertain states, which is the safe behavior
static bool IsLevelLoading() {
    if (!g_gameState.pIsLoadingFunc) return true;

    void* pLevelDI = GetLevelDI();
    if (!pLevelDI) return true;  // null LevelDI = no level loaded (main menu)

    __try {
        return g_gameState.pIsLoadingFunc(pLevelDI);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
}

// Check if game timer is frozen (paused) using game's own IsTimerFrozen function
// Returns true (paused) when state cannot be determined - safe default
static bool IsTimerFrozen() {
    if (!g_gameState.pIsTimerFrozenFunc) return true;

    void* pLevelDI = GetLevelDI();
    if (!pLevelDI) return true;  // null LevelDI = not in gameplay

    __try {
        return g_gameState.pIsTimerFrozenFunc(pLevelDI);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
}

// Get CGame pointer (shared helper for menu detection and diagnostics)
static void* GetCGame() {
    if (!g_gameState.initialized || !g_gameState.pCLobbySteamPtr)
        return nullptr;

    __try {
        void* pCLobbySteam = *g_gameState.pCLobbySteamPtr;
        if (!pCLobbySteam) return nullptr;

        return *(void**)((BYTE*)pCLobbySteam + CLOBBYSTEAM_TO_CGAME_OFFSET);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// Check if the current level is the main menu by scanning for "menu" in the level path
// embedded near CGame+0x3B0. On main menu: "aps/menu_level/menu_level.exp"
// Returns false when state cannot be determined - let other checks (frozen) handle it
static bool IsOnMainMenu() {
    void* pCGame = GetCGame();
    if (!pCGame) return false;

    __try {
        // Level path string starts at CGame+0x3B1 (byte at 0x3B0 is NUL prefix)
        // Scan region for "menu" substring
        const BYTE* region = (const BYTE*)pCGame + 0x3B0;
        for (int i = 0; i < 32; i++) {
            if (region[i] == 'm' && region[i + 1] == 'e' &&
                region[i + 2] == 'n' && region[i + 3] == 'u')
                return true;
        }
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Pattern from EGameTools: 48 89 5C 24 ?? 57 48 83 EC ?? 49 8B C1 48 8B F9
// mov [rsp+??], rbx; push rdi; sub rsp, ??; mov rax, r9; mov rdi, rcx
static void* FindMoveCameraFunction() {
    HMODULE engineModule = GetModuleHandleA("engine_x64_rwdi.dll");
    if (!engineModule) {
        Logger::Instance().Error("engine_x64_rwdi.dll not loaded");
        return nullptr;
    }

    Logger::Instance().Info("Scanning engine_x64_rwdi.dll for MoveCameraFromForwardUpPos...");

    // Use core library pattern scanner
    void* result = cameraunlock::memory::ScanPattern(
        engineModule,
        "48 89 5C 24 ?? 57 48 83 EC ?? 49 8B C1 48 8B F9"
    );

    if (result) {
        Logger::Instance().Info("Found MoveCameraFromForwardUpPos at %p", result);
    } else {
        Logger::Instance().Warning("MoveCameraFromForwardUpPos pattern not found");
    }

    return result;
}

void __fastcall MoveCameraHook(void* thisCamera, void* forward, void* up, void* position) {
    // Record that camera hook is running (for gameplay detection)
    // Relaxed ordering - just a timestamp, no synchronization needed
    g_gameplayCache.lastCameraHookTick.store(GetTickCount64(), std::memory_order_relaxed);

    // Deferred DX hook initialization - D3D12 is definitely loaded now
    if (!g_hook.dxHookInitialized) {
        g_hook.dxHookInitialized = true;  // Only try once
        if (InstallDXHook()) {
            Logger::Instance().Info("DX hook installed (deferred init)");
            // Enable crosshair if tracking is enabled
            if (g_tracking.enabled.load(std::memory_order_relaxed)) {
                SetCrosshairEnabled(true);
            }
        } else {
            Logger::Instance().Warning("DX hook failed (deferred init)");
        }
    }

    // Extract game camera pitch from forward vector (before head tracking)
    // This must happen before early return so crosshair projection always has valid data
    // DL2 coordinate system: X=forward, Y=up, Z=left
    // Pitch = angle from horizontal plane, positive = looking down
    // forward.Y = -sin(pitch), so pitch = asin(-forward.Y)
    float* fwdIn = (float*)forward;
    if (fwdIn) {
        float fwdY = fwdIn[1];
        // Clamp to valid asin range to prevent NaN
        if (fwdY < -1.0f) fwdY = -1.0f;
        if (fwdY > 1.0f) fwdY = 1.0f;
        float gamePitch = asinf(-fwdY);
        SetGameCameraPitch(gamePitch);
    }

    // Skip head tracking if not in gameplay (menu, loading, paused, post-load warmup)
    {
        bool hasFwd = (forward != nullptr);
        bool hasUp = (up != nullptr);
        bool enabled = g_tracking.enabled.load(std::memory_order_relaxed);
        bool loading = g_gameplayCache.levelLoading.load(std::memory_order_relaxed);
        bool frozen = g_gameplayCache.timerFrozen.load(std::memory_order_relaxed);
        bool mainMenu = g_gameplayCache.onMainMenu.load(std::memory_order_relaxed);
        bool skip = !hasFwd || !hasUp || !enabled || loading || frozen || mainMenu;

        bool warmupBlock = false;
        if (!skip) {
            // Post-loading warmup: block for 1.5s after loading/level-change ends
            ULONGLONG loadEnded = g_gameplayCache.loadingEndedTick.load(std::memory_order_relaxed);
            ULONGLONG levelChanged = g_gameplayCache.levelChangedTick.load(std::memory_order_relaxed);
            ULONGLONG warmupRef = (levelChanged > loadEnded) ? levelChanged : loadEnded;
            if (warmupRef > 0 && (GetTickCount64() - warmupRef < POST_LOADING_WARMUP_MS)) {
                warmupBlock = true;
                skip = true;
            }
        }

        if (skip) {
            ((MoveCameraFunc_t)g_hook.pMoveCameraOriginal)(thisCamera, forward, up, position);
            return;
        }
    }

    // Get fresh processed rotation directly from the tracking pipeline.
    // Runs interpolation and smoothing on every camera update (not just DX Present),
    // so head tracking is as smooth as mouse-driven camera rotation.
    float processedYaw, processedPitch, processedRoll;
    if (!Mod::Instance().GetProcessedRotation(processedYaw, processedPitch, processedRoll)) {
        ((MoveCameraFunc_t)g_hook.pMoveCameraOriginal)(thisCamera, forward, up, position);
        return;
    }

    // Update crosshair with same processed values (read by DX Present for rendering)
    SetCrosshairOffset(processedYaw, processedPitch, processedRoll);

    // Mark gameplay state
    g_gameplayCache.headTrackingAppliedThisFrame.store(true, std::memory_order_relaxed);
    g_gameplayCache.lastHeadTrackingAppliedTick.store(GetTickCount64(), std::memory_order_relaxed);

    // Convert to radians and apply direction conventions
    float yaw = -processedYaw * DEG_TO_RAD;
    float pitch = processedPitch * DEG_TO_RAD;
    float roll = processedRoll * DEG_TO_RAD;

    // Skip if no significant rotation
    if (fabsf(yaw) < ROTATION_THRESHOLD && fabsf(pitch) < ROTATION_THRESHOLD && fabsf(roll) < ROTATION_THRESHOLD) {
        ((MoveCameraFunc_t)g_hook.pMoveCameraOriginal)(thisCamera, forward, up, position);
        return;
    }

    // Copy vectors for modification (game expects float[4] with w=0)
    float* upIn = (float*)up;
    float myFwd[4] = { fwdIn[0], fwdIn[1], fwdIn[2], 0 };
    float myUp[4] = { upIn[0], upIn[1], upIn[2], 0 };

    ApplyHeadTrackingRotation(myFwd, myUp, yaw, pitch, roll);

    // Apply position offset in horizon-locked space
    float* posIn = (float*)position;
    float myPos[4] = { posIn[0], posIn[1], posIn[2], posIn[3] };

    float posOffX, posOffY, posOffZ;
    if (Mod::Instance().GetPositionOffset(posOffX, posOffY, posOffZ)) {
        // Flatten forward to horizontal plane and normalize
        float flatFwdX = fwdIn[0];
        float flatFwdZ = fwdIn[2];
        float flatLen = sqrtf(flatFwdX * flatFwdX + flatFwdZ * flatFwdZ);
        if (flatLen > 0.0001f) {
            flatFwdX /= flatLen;
            flatFwdZ /= flatLen;
        }

        // DL2 left-handed coords: X=forward, Y=up, Z=left
        // Left vector in XZ plane: (-flatFwdZ, flatFwdX)
        float leftX = -flatFwdZ;
        float leftZ = flatFwdX;

        // Map tracker axes to game directions:
        // posOffZ → forward/back, posOffX → lateral (right positive)
        // -left * posOffX: positive tracker X (head right) → negative left → RIGHT in game
        myPos[0] += flatFwdX * posOffZ + leftX * posOffX;
        myPos[1] += posOffY;
        myPos[2] += flatFwdZ * posOffZ + leftZ * posOffX;
    }

    ((MoveCameraFunc_t)g_hook.pMoveCameraOriginal)(thisCamera, myFwd, myUp, myPos);
}

bool InstallEngineCameraHook() {
    if (g_hook.hookInstalled) {
        Logger::Instance().Info("Engine camera hook already installed");
        return true;
    }

    g_hook.pMoveCameraFunc = FindMoveCameraFunction();
    if (!g_hook.pMoveCameraFunc) {
        return false;
    }

    MH_STATUS status = MH_CreateHook(g_hook.pMoveCameraFunc, (void*)MoveCameraHook, &g_hook.pMoveCameraOriginal);
    if (status != MH_OK) {
        Logger::Instance().Error("MH_CreateHook failed: %d", status);
        return false;
    }

    status = MH_EnableHook(g_hook.pMoveCameraFunc);
    if (status != MH_OK) {
        Logger::Instance().Error("MH_EnableHook failed: %d", status);
        MH_RemoveHook(g_hook.pMoveCameraFunc);
        return false;
    }

    g_hook.hookInstalled = true;
    Logger::Instance().Info("Engine camera hook installed successfully");

    // Initialize game state detection for IsLoading checks
    // If unavailable, IsLevelLoading() returns true (assumes loading) which is safe
    if (!InitializeGameStateDetection()) {
        Logger::Instance().Warning("Game state detection unavailable - head tracking disabled during loading by default");
    }

    return true;
}

void RemoveEngineCameraHook() {
    if (!g_hook.hookInstalled) return;

    if (g_hook.pMoveCameraFunc) {
        MH_DisableHook(g_hook.pMoveCameraFunc);
        MH_RemoveHook(g_hook.pMoveCameraFunc);
    }

    g_hook.pMoveCameraFunc = nullptr;
    g_hook.pMoveCameraOriginal = nullptr;
    g_hook.hookInstalled = false;
    Logger::Instance().Info("Engine camera hook removed");
}

void SetCameraHookEnabled(bool enabled) {
    g_tracking.enabled.store(enabled, std::memory_order_relaxed);
    // Smoothing state is now managed by TrackingProcessor in Mod class
}

// Refresh cached gameplay state - call once per frame from DX hook
// Tracks loading transitions (warmup timer) and timer frozen state (paused/menu)
void RefreshGameplayStateCache() {
    ULONGLONG now = GetTickCount64();
    ULONGLONG lastFrame = g_gameplayCache.lastFrameTick.load(std::memory_order_relaxed);

    // Only refresh once per frame (throttle to avoid redundant calls)
    if (now == lastFrame) return;
    g_gameplayCache.lastFrameTick.store(now, std::memory_order_relaxed);

    // Update loading state and detect transitions
    bool loading = IsLevelLoading();
    bool wasLoading = g_gameplayCache.wasLoading.load(std::memory_order_relaxed);
    g_gameplayCache.levelLoading.store(loading, std::memory_order_relaxed);
    g_gameplayCache.wasLoading.store(loading, std::memory_order_relaxed);

    if (wasLoading && !loading) {
        // Loading just ended — start warmup timer
        g_gameplayCache.loadingEndedTick.store(now, std::memory_order_relaxed);
    }

    // Update timer frozen state (paused/menu detection)
    bool frozen = IsTimerFrozen();
    g_gameplayCache.timerFrozen.store(frozen, std::memory_order_relaxed);

    // Update main menu detection
    bool mainMenu = IsOnMainMenu();
    g_gameplayCache.onMainMenu.store(mainMenu, std::memory_order_relaxed);

    // Track LevelDI pointer changes (level transitions)
    void* pLevelDI = GetLevelDI();
    uintptr_t currentLevelDI = (uintptr_t)pLevelDI;
    uintptr_t prevLevelDI = g_gameplayCache.lastLevelDI.load(std::memory_order_relaxed);
    if (currentLevelDI != prevLevelDI) {
        g_gameplayCache.lastLevelDI.store(currentLevelDI, std::memory_order_relaxed);
        // Any pointer change (including null→new) triggers warmup, except initial assignment
        static bool firstAssignment = true;
        if (!firstAssignment) {
            g_gameplayCache.levelChangedTick.store(now, std::memory_order_relaxed);
            Logger::Instance().Info("Level transition: LevelDI %p -> %p", (void*)prevLevelDI, pLevelDI);
        }
        firstAssignment = false;
    }

    // Reset per-frame head tracking flag
    // It will be set to true by MoveCameraHook if rotation is actually applied
    g_gameplayCache.headTrackingAppliedThisFrame.store(false, std::memory_order_relaxed);
}

bool IsInGameplay() {
    // The most reliable check: did we actually apply head tracking recently?
    // This is true only when all gate conditions passed in MoveCameraHook:
    // enabled, not loading, not paused (timer frozen), past warmup
    ULONGLONG now = GetTickCount64();
    ULONGLONG lastApplied = g_gameplayCache.lastHeadTrackingAppliedTick.load(std::memory_order_relaxed);

    // Head tracking must have been applied within the last 100ms
    return (lastApplied > 0) && (now - lastApplied < GAMEPLAY_DETECTION_THRESHOLD_MS);
}

} // namespace DL2HT
