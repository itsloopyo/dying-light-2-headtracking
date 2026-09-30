#include "pch.h"
#include "engine_camera_hook.h"
#include "aim_state.h"
#include "dx_hook.h"
#include "hook_manager.h"
#include "lean_trace.h"
#include "core/aim_pose.h"
#include "core/aim_projection.h"
#include "core/mod.h"
#include "core/logger.h"
#include "core/rotation_math.h"

#include <cameraunlock/camera/lean_clamp.h>
#include <cameraunlock/camera/lean_line_sweep.h>
#include <cameraunlock/memory/pattern_scanner.h>
#include <cameraunlock/memory/safe_memory.h>
#include <cameraunlock/time/qpc_clock.h>

namespace DL2HT {

// Constants
static constexpr ULONGLONG GAMEPLAY_DETECTION_THRESHOLD_MS = 100;
static constexpr ULONGLONG POST_LOADING_WARMUP_MS = 1500;
// Below ~0.1mm the offset is below floor noise; treat as no movement.
static constexpr float POSITION_THRESHOLD = 0.0001f;

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

// Head tracking through the aim, and what the camera hook has already logged about it. Touched
// only from the camera update.
static AimPose g_aimPose;
static bool g_zoomLogged = false;
static float g_zoomLoggedFactor = 1.0f;
static ULONGLONG g_zoomLoggedTick = 0;
static bool g_wasAiming = false;

// The lean kept out of the level. Run once per tracking update on the view camera; every camera
// moved in that update takes the lean at the fraction of it the view camera was allowed.
static bool g_leanTraceReady = false;
static cameraunlock::camera::LeanClamp g_leanClamp;
static lean_trace::Context g_leanContext;
static cameraunlock::camera::LineSweep g_leanSweep;
static float g_leanScale = 1.0f;
static uint64_t g_leanFrame = 0;
static uint64_t g_leanClampMicros = 0;
static cameraunlock::math::Vec3 g_lastEye;
static bool g_haveLastEye = false;
static bool g_leanNearLogged = false;
static bool g_leanContact = false;
static bool g_leanFailed = false;
static ULONGLONG g_leanSampleTick = 0;
// An eye that moved further than this between two view camera updates was cut or teleported,
// and the allowance it carries belongs to the previous place.
static constexpr float kCameraCutDistance = 1.0f;
static constexpr ULONGLONG kLeanSampleMs = 10000;
// How far the aim dot looks along the clean aim for the surface it lands on. Past it the aim point is
// far enough that a lean moves it by less than a pixel, and the dot shows the aim direction.
static constexpr float kAimTraceLength = 500.0f;
static bool g_aimTraceFailLogged = false;
static ULONGLONG g_aimSampleTick = 0;

static void ForgetLeanAllowance() {
    g_leanClamp.Reset();
    g_leanScale = 1.0f;
    g_haveLastEye = false;
}

// The fraction of `desired` (the lean in world metres) the level leaves room for at `eye`.
static float LeanCollisionScale(const FppCameraSample& fpp, void* engineCamera, void* levelDI,
                                const cameraunlock::math::Vec3& eye, const cameraunlock::math::Vec3& desired) {
    using cameraunlock::math::Vec3;
    if (!g_leanTraceReady || !Mod::Instance().GetConfig().collision_enabled) return 1.0f;
    if (!fpp.view) return g_leanScale;
    const uint64_t frame = Mod::Instance().GetPoseFrame();
    if (frame == g_leanFrame) return g_leanScale;
    g_leanFrame = frame;

    if (!g_leanNearLogged) {
        g_leanNearLogged = true;
        const float margin = g_leanClamp.Settings().skin;
        float nearClip = 0.0f;
        if (!lean_trace::NearClip(engineCamera, nearClip)) {
            Logger::Instance().Warning("Lean collision: margin %.3f m, near clip unreadable", margin);
        } else if (margin <= nearClip) {
            Logger::Instance().Warning("Lean collision: margin %.3f m does not exceed the near clip %.3f m, so a wall "
                                       "held at the margin is still culled",
                                       margin, nearClip);
        } else {
            Logger::Instance().Info("Lean collision: margin %.3f m, near clip %.3f m", margin, nearClip);
        }
    }

    if (g_haveLastEye && (eye - g_lastEye).SqrMagnitude() > kCameraCutDistance * kCameraCutDistance) {
        g_leanClamp.Reset();
    }
    g_lastEye = eye;
    g_haveLastEye = true;

    const uint64_t now = cameraunlock::time::QpcNowMicros();
    float dt = g_leanClampMicros ? static_cast<float>(now - g_leanClampMicros) * 1e-6f : 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    g_leanClampMicros = now;

    lean_trace::Validate(fpp.viewCamera, levelDI);
    const float desiredLength = desired.Magnitude();
    g_leanContext.viewCamera = fpp.viewCamera;
    g_leanContext.eye = eye;
    g_leanContext.leanDirection = desiredLength > 0.0f ? desired * (1.0f / desiredLength) : Vec3::Zero();
    const Vec3 allowed = g_leanClamp.Apply(eye, desired, dt, &cameraunlock::camera::LineSweepQuery, &g_leanSweep);
    g_leanScale = desiredLength > 0.0f ? allowed.Magnitude() / desiredLength : 1.0f;

    // Transitions alone cannot tell "the casts run and the room is open" from "the casts are not
    // running", so a sample goes out every few seconds as well.
    const bool contact = g_leanClamp.InContact();
    const bool failed = g_leanClamp.LastQueryFailed();
    const ULONGLONG tick = GetTickCount64();
    if (contact != g_leanContact || failed != g_leanFailed || tick - g_leanSampleTick >= kLeanSampleMs) {
        unsigned casts = 0;
        double micros = 0.0;
        lean_trace::TakeStats(casts, micros);
        const double seconds = g_leanSampleTick ? static_cast<double>(tick - g_leanSampleTick) / 1000.0 : 0.0;
        Logger::Instance().Info("Lean collision: %s%s, lean %.3f m allowed %.3f m (%u casts in %.2f ms over the "
                                "last %.1f s)",
                                contact ? "held off a surface" : "clear", failed ? ", CAST NOT RUN" : "",
                                desiredLength, allowed.Magnitude(), casts, micros / 1000.0, seconds);
        g_leanContact = contact;
        g_leanFailed = failed;
        g_leanSampleTick = tick;
    }
    return g_leanScale;
}

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

    // ILevel::IsTimerFrozen() const - timer frozen when game is paused
    g_gameState.pIsTimerFrozenFunc = (IsTimerFrozenFunc_t)GetProcAddress(engineModule, "?IsTimerFrozen@ILevel@@QEBA_NXZ");
    if (g_gameState.pIsTimerFrozenFunc) {
        Logger::Instance().Info("Found ILevel::IsTimerFrozen at %p", g_gameState.pIsTimerFrozenFunc);
    }

    g_gameState.initialized = (g_gameState.pCLobbySteamPtr != nullptr);
    return g_gameState.initialized;
}

// Get CGame pointer (shared helper for menu detection and diagnostics)
static void* GetCGame() {
    if (!g_gameState.initialized || !g_gameState.pCLobbySteamPtr)
        return nullptr;

    __try {
        void* pCLobbySteam = *g_gameState.pCLobbySteamPtr;
        if (!pCLobbySteam) return nullptr;

        return *(void**)((BYTE*)pCLobbySteam + CLOBBYSTEAM_TO_CGAME_OFFSET);
    } __except (cameraunlock::memory::AccessViolationFilter(GetExceptionCode())) {
        return nullptr;
    }
}

// Chain: CLobbySteam → +0xF8 → CGame → +0x390 → CLevel → +0x20 → LevelDI
// Returns nullptr if any pointer in the chain is invalid
static void* GetLevelDI(void* pCGame) {
    if (!pCGame) return nullptr;

    __try {
        void* pCLevel = *(void**)((BYTE*)pCGame + CGAME_TO_CLEVEL_OFFSET);
        if (!pCLevel) return nullptr;

        return *(void**)((BYTE*)pCLevel + CLEVEL_TO_LEVELDI_OFFSET);
    } __except (cameraunlock::memory::AccessViolationFilter(GetExceptionCode())) {
        return nullptr;
    }
}

// Check if level is currently loading using game's own IsLoading function
// Returns true (loading) when state cannot be determined - this disables
// head tracking during uncertain states, which is the safe behavior
static bool IsLevelLoading(void* pLevelDI) {
    if (!g_gameState.pIsLoadingFunc) return true;
    if (!pLevelDI) return true;  // null LevelDI = no level loaded (main menu)

    __try {
        return g_gameState.pIsLoadingFunc(pLevelDI);
    } __except (cameraunlock::memory::AccessViolationFilter(GetExceptionCode())) {
        return true;
    }
}

// Check if game timer is frozen (paused) using game's own IsTimerFrozen function
// Returns true (paused) when state cannot be determined - safe default
static bool IsTimerFrozen(void* pLevelDI) {
    if (!g_gameState.pIsTimerFrozenFunc) return true;
    if (!pLevelDI) return true;  // null LevelDI = not in gameplay

    __try {
        return g_gameState.pIsTimerFrozenFunc(pLevelDI);
    } __except (cameraunlock::memory::AccessViolationFilter(GetExceptionCode())) {
        return true;
    }
}

// Check if the current level is the main menu by scanning for "menu" in the level path
// embedded near CGame+0x3B0. On main menu: "aps/menu_level/menu_level.exp"
// Returns false when state cannot be determined - let other checks (frozen) handle it
static bool IsOnMainMenu(void* pCGame) {
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
    } __except (cameraunlock::memory::AccessViolationFilter(GetExceptionCode())) {
        return false;
    }
}

// Signature taken from EGameTools' offsets.h (MIT, (c) 2023-2024 EricPlayZ,
// https://github.com/EricPlayZ/EGameTools). See THIRD-PARTY-NOTICES.md.
// 48 89 5C 24 ?? 57 48 83 EC ?? 49 8B C1 48 8B F9
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

    float* fwdIn = (float*)forward;

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
            g_aimPose.Stop();
            ForgetLeanAllowance();
            if (g_wasAiming) {
                g_wasAiming = false;
                Logger::Instance().Info("Sights down");
            }
            ((MoveCameraFunc_t)g_hook.pMoveCameraOriginal)(thisCamera, forward, up, position);
            return;
        }
    }

    // Read after the gameplay gate, so a menu or a load reports its own reason and the game's
    // camera and firearm state are only asked for in gameplay.
    void* const levelDI = GetLevelDI(GetCGame());
    const FppCameraSample fpp = SampleFppCamera(thisCamera, levelDI);
    float zoomFactor = 1.0f;
    if (fpp.fovKnown) zoomFactor = ZoomFactor(fpp.liveFovDeg, fpp.baseFovDeg);
    // The aim dot follows the view camera alone. Other cameras moved through this call would
    // otherwise overwrite its projection with their own basis and an unscaled zoom.
    if (fpp.view) SetCrosshairFOV(fpp.liveFovKnown ? fpp.liveFovDeg : 0.0f);
    if (fpp.view && fpp.aiming != g_wasAiming) {
        g_wasAiming = fpp.aiming;
        Logger::Instance().Info("Sights %s", fpp.aiming ? "up" : "down");
    }

    // Every term once, on the first gameplay frame whether or not a tracker is sending, then the
    // factor again whenever it moves, at most once a second: the first gameplay frame can still be
    // a spawn animation's FOV, and ordinary play has to be seen reading 1.0000.
    if (fpp.fpp && g_zoomLogged && fpp.fovKnown && std::fabs(zoomFactor - g_zoomLoggedFactor) > 0.005f &&
        GetTickCount64() - g_zoomLoggedTick >= 1000) {
        g_zoomLoggedFactor = zoomFactor;
        g_zoomLoggedTick = GetTickCount64();
        Logger::Instance().Info("Zoom factor %.4f (live %.4f / base %.4f deg vertical)", zoomFactor, fpp.liveFovDeg,
                                fpp.baseFovDeg);
    }
    if (fpp.fpp && !g_zoomLogged) {
        g_zoomLogged = true;
        g_zoomLoggedFactor = zoomFactor;
        g_zoomLoggedTick = GetTickCount64();
        if (fpp.fovKnown) {
            Logger::Instance().Info("Zoom compensation: live FOV %.4f deg vertical (engine camera, radians x 57.2958), "
                                    "base %.4f deg vertical (first-person camera before zoom), both vertical so no "
                                    "aspect conversion, factor %.4f",
                                    fpp.liveFovDeg, fpp.baseFovDeg, zoomFactor);
        } else {
            Logger::Instance().Warning("Zoom compensation: the first-person camera's FOV is unreadable, so head "
                                       "tracking is not scaled to the zoom");
        }
    }

    // Get fresh processed rotation directly from the tracking pipeline.
    // Runs interpolation and smoothing on every camera update (not just DX Present),
    // so head tracking is as smooth as mouse-driven camera rotation.
    float processedYaw, processedPitch, processedRoll;
    if (!Mod::Instance().GetProcessedRotation(processedYaw, processedPitch, processedRoll)) {
        g_aimPose.Stop();
        ForgetLeanAllowance();
        ((MoveCameraFunc_t)g_hook.pMoveCameraOriginal)(thisCamera, forward, up, position);
        return;
    }

    g_gameplayCache.lastHeadTrackingAppliedTick.store(GetTickCount64(), std::memory_order_relaxed);

    // Fetch 6DOF position offset up front so the rotation-threshold skip can
    // also account for it. In tracking-mode 2 (position only) rotation is
    // always zero, so without this hoist the threshold path would bypass
    // position application.
    HeadPose tracked;
    tracked.yaw = processedYaw;
    tracked.pitch = processedPitch;
    tracked.roll = processedRoll;
    bool hasPosOffset = Mod::Instance().GetPositionOffset(tracked.lean.x, tracked.lean.y, tracked.lean.z);
    if (!hasPosOffset) {
        tracked.lean = cameraunlock::math::Vec3();
        if (fpp.view) ForgetLeanAllowance();
    }
    // GetTickCount64 steps in about 16 ms, a tenth of the fade.
    if (fpp.view) {
        g_aimPose.Update(fpp.aiming, Mod::Instance().IsTrueFreeLook(), cameraunlock::time::QpcNowMicros() / 1000);
    }
    const HeadPose applied = g_aimPose.Apply(tracked, zoomFactor);
    const float posOffX = applied.lean.x;
    const float posOffY = applied.lean.y;
    const float posOffZ = applied.lean.z;

    // Convert to radians and apply direction conventions
    float yaw = -applied.yaw * DEG_TO_RAD;
    float pitch = applied.pitch * DEG_TO_RAD;
    float roll = applied.roll * DEG_TO_RAD;
    bool posSignificant = hasPosOffset && (fabsf(posOffX) > POSITION_THRESHOLD ||
                                           fabsf(posOffY) > POSITION_THRESHOLD ||
                                           fabsf(posOffZ) > POSITION_THRESHOLD);
    bool rotSignificant = fabsf(yaw) >= ROTATION_THRESHOLD ||
                          fabsf(pitch) >= ROTATION_THRESHOLD ||
                          fabsf(roll) >= ROTATION_THRESHOLD;

    // Skip if neither rotation nor position would change the camera
    if (!rotSignificant && !posSignificant) {
        if (fpp.view) SetCrosshairProjection(0, 0);
        ((MoveCameraFunc_t)g_hook.pMoveCameraOriginal)(thisCamera, forward, up, position);
        return;
    }

    // Copy vectors for modification (game expects float[4] with w=0)
    float* upIn = (float*)up;
    float myFwd[4] = { fwdIn[0], fwdIn[1], fwdIn[2], 0 };
    float myUp[4] = { upIn[0], upIn[1], upIn[2], 0 };

    if (rotSignificant) {
        ApplyHeadTrackingRotation(myFwd, myUp, yaw, pitch, roll, Mod::Instance().GetYawMode());
    }

    // Compute horizon-locked basis from original forward (used by 6DOF position offset)
    // DL2 coords: X=forward, Y=up, Z=left
    float* posIn = (float*)position;
    float myPos[4] = { posIn[0], posIn[1], posIn[2], posIn[3] };

    float flatFwdX = fwdIn[0];
    float flatFwdZ = fwdIn[2];
    float flatLen = sqrtf(flatFwdX * flatFwdX + flatFwdZ * flatFwdZ);
    if (flatLen > 0.0001f) {
        flatFwdX /= flatLen;
        flatFwdZ /= flatLen;
    }
    float leftX = -flatFwdZ;
    float leftZ = flatFwdX;

    // Apply 6DOF position offset in horizon-locked space
    if (hasPosOffset) {
        const cameraunlock::math::Vec3 lean(flatFwdX * posOffZ + leftX * posOffX, posOffY,
                                            flatFwdZ * posOffZ + leftZ * posOffX);
        const float scale = LeanCollisionScale(fpp, thisCamera, levelDI,
                                               cameraunlock::math::Vec3(posIn[0], posIn[1], posIn[2]), lean);
        myPos[0] += lean.x * scale;
        myPos[1] += lean.y * scale;
        myPos[2] += lean.z * scale;
    }

    // --- Crosshair projection (Subnautica approach) ---
    // Project the aim world-point through the actual head-tracked camera,
    // exactly like Subnautica's ReticleCompensation:
    //   toAim = aimWorldPoint - headTrackedCameraPos
    //   project toAim onto head-tracked camera axes
    //   perspective divide → tangent-space offset
    //
    // This naturally handles both rotation and position because we use
    // the ACTUAL modified camera vectors, not a reconstruction from angles.
    if (fpp.view) {
        // The engine's "forward" is backward, so the shot travels along its negative.
        // Without a lean the depth cancels in the perspective divide and no cast is needed.
        const float leanX = myPos[0] - posIn[0];
        const float leanY = myPos[1] - posIn[1];
        const float leanZ = myPos[2] - posIn[2];
        float toAimX = -fwdIn[0];
        float toAimY = -fwdIn[1];
        float toAimZ = -fwdIn[2];
        bool aimKnown = true;
        bool hit = false;
        float depth = 0.0f;
        if (leanX != 0.0f || leanY != 0.0f || leanZ != 0.0f) {
            const cameraunlock::math::Vec3 aim =
                cameraunlock::math::Vec3(-fwdIn[0], -fwdIn[1], -fwdIn[2]).Normalized();
            lean_trace::Validate(fpp.viewCamera, levelDI);
            if (!lean_trace::AimDistance(fpp.viewCamera, cameraunlock::math::Vec3(posIn[0], posIn[1], posIn[2]), aim,
                                         kAimTraceLength, hit, depth)) {
                aimKnown = false;
                if (!g_aimTraceFailLogged) {
                    g_aimTraceFailLogged = true;
                    Logger::Instance().Warning("Aim dot: the aim cannot be traced, so the dot is hidden while you lean");
                }
            } else if (hit) {
                toAimX = aim.x * depth - leanX;
                toAimY = aim.y * depth - leanY;
                toAimZ = aim.z * depth - leanZ;
            }
        }
        if (aimKnown && g_aimTraceFailLogged) {
            g_aimTraceFailLogged = false;
            Logger::Instance().Info("Aim dot: projection available again");
        }

        float tanRight = 0.0f, tanUp = 0.0f;
        const bool projected = aimKnown && ProjectAim({toAimX, toAimY, toAimZ}, myFwd, myUp, tanRight, tanUp);
        SetCrosshairProjection(tanRight, tanUp);
        if (!projected) SetCrosshairFOV(0.0f);

        const ULONGLONG tick = GetTickCount64();
        if (tick - g_aimSampleTick >= 1000) {
            g_aimSampleTick = tick;
            float rotRight = 0.0f, rotUp = 0.0f;
            const bool rotationValid = ProjectAim({-fwdIn[0], -fwdIn[1], -fwdIn[2]}, myFwd, myUp, rotRight, rotUp);
            Logger::Instance().Info("AIMGEO trace=%s depth=%.4f leanWorld=(%.4f,%.4f,%.4f) "
                                    "pose=(%.3f,%.3f,%.3f) zoom=%.4f vfov=%.4f "
                                    "rotValid=%d rot=(%.6f,%.6f) valid=%d full=(%.6f,%.6f)",
                                    !aimKnown ? "failed" : hit ? "hit" :
                                    (leanX != 0.0f || leanY != 0.0f || leanZ != 0.0f) ? "miss" : "not-needed",
                                    depth, leanX, leanY, leanZ, applied.yaw, applied.pitch, applied.roll,
                                    zoomFactor, fpp.liveFovDeg, rotationValid, rotRight, rotUp,
                                    projected, tanRight, tanUp);
        }
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

    // Without it the sights read as down and nothing is scaled to the zoom; head tracking itself
    // is unaffected.
    InitializeAimState();

    const DL2HT::Config& config = Mod::Instance().GetConfig();
    g_leanTraceReady = lean_trace::Initialize();
    g_leanClamp.SetSettings(config.lean_clamp);
    g_leanSweep.cast = &lean_trace::Cast;
    g_leanSweep.cast_context = &g_leanContext;
    g_leanSweep.settings.radius = config.lean_clamp.skin;
    Logger::Instance().Info("Lean collision: %s (margin %.3f m, release smoothing %.2f)",
                            config.collision_enabled ? (g_leanTraceReady ? "on" : "unavailable") : "off (CollisionEnabled)",
                            config.lean_clamp.skin, config.lean_clamp.release_smoothing);

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

    void* pCGame = GetCGame();
    void* pLevelDI = GetLevelDI(pCGame);

    // Update loading state and detect transitions
    bool loading = IsLevelLoading(pLevelDI);
    bool wasLoading = g_gameplayCache.wasLoading.load(std::memory_order_relaxed);
    g_gameplayCache.levelLoading.store(loading, std::memory_order_relaxed);
    g_gameplayCache.wasLoading.store(loading, std::memory_order_relaxed);

    if (wasLoading && !loading) {
        // Loading just ended - start warmup timer
        g_gameplayCache.loadingEndedTick.store(now, std::memory_order_relaxed);
    }

    // Update timer frozen state (paused/menu detection)
    bool frozen = IsTimerFrozen(pLevelDI);
    g_gameplayCache.timerFrozen.store(frozen, std::memory_order_relaxed);

    // Update main menu detection
    bool mainMenu = IsOnMainMenu(pCGame);
    g_gameplayCache.onMainMenu.store(mainMenu, std::memory_order_relaxed);

    // Track LevelDI pointer changes (level transitions)
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
