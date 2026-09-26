#include "pch.h"
#include "mod.h"
#include "logger.h"
#include "path_utils.h"
#include "hotkey_utils.h"
#include "hooks/hook_manager.h"
#include "hooks/engine_camera_hook.h"
#include "hooks/input_hook.h"
#include "hooks/dx_hook.h"
#include "hooks/crosshair_hook.h"
#include "ui/notification.h"
#include "legacy_config/legacy_config.h"

#include <cameraunlock/time/qpc_clock.h>

namespace DL2HT {

Mod& Mod::Instance() {
    static Mod instance;
    return instance;
}

bool Mod::Initialize() {
    if (m_initialized.load()) {
        Logger::Instance().Warning("Mod already initialized");
        return true;
    }

    Logger::Instance().Info("DL2 Head Tracking v%s initializing...", DL2HT_VERSION);

    // Load config
    if (!LoadConfig()) {
        Logger::Instance().Warning("Using default configuration");
    }

    // Configure the tracking session's rotation pipeline
    cameraunlock::SensitivitySettings sensitivity;
    sensitivity.yaw = m_config.yawMultiplier;
    sensitivity.pitch = m_config.pitchMultiplier;
    sensitivity.roll = m_config.rollMultiplier;
    m_session.GetProcessor().SetSensitivity(sensitivity);

    Logger::Instance().Info("TrackingProcessor initialized with sensitivity: yaw=%.2f pitch=%.2f roll=%.2f",
                            sensitivity.yaw, sensitivity.pitch, sensitivity.roll);

    // Initialize reticle state
    m_reticleEnabled = m_config.reticleEnabled;

    // Initialize yaw rotation frame from config
    m_worldLockedYaw.store(m_config.worldLockedYaw);

    // Configure the session's position pipeline (6DOF). The config flag seeds
    // the tracking mode: positionEnabled=false starts in rotation-only mode.
    if (!m_config.positionEnabled) {
        m_session.SetMode(cameraunlock::TrackingMode::RotationOnly);
    }
    // Field-by-field rather than the positional constructor: PositionSettings
    // now carries two smoothing floats between the limits and the invert flags,
    // and a positional call that drops them binds the bools to the smoothing
    // parameters without a compile error.
    cameraunlock::PositionSettings posSettings;
    posSettings.sensitivity_x = m_config.positionSensitivityX;
    posSettings.sensitivity_y = m_config.positionSensitivityY;
    posSettings.sensitivity_z = m_config.positionSensitivityZ;
    posSettings.limit_x = m_config.positionLimitX;
    // The clamp is [-limit_y_down, +limit_y] and limit_y_down carries its own
    // default, so mirror the one configured vertical limit the way
    // PositionSettings::Symmetric does. Left unset, raising LimitY widened the
    // upward budget only and downward travel stayed pinned at 0.20m.
    posSettings.limit_y = m_config.positionLimitY;
    posSettings.limit_y_down = m_config.positionLimitY;
    posSettings.limit_z = m_config.positionLimitZ;
    posSettings.limit_z_back = m_config.positionLimitZBack;
    posSettings.invert_x = m_config.positionInvertX;
    posSettings.invert_y = m_config.positionInvertY;
    posSettings.invert_z = m_config.positionInvertZ;
    m_session.GetPositionProcessor().SetSettings(posSettings);

    // After SetSettings, which would otherwise overwrite the smoothing fields.
    // The session forwards both values to the rotation AND position processors
    // and re-reads the receiver's connection locality inside every Update() to
    // pick the one that applies. Without IsRemoteConnection() on the receiver
    // that selection silently pins to local, so assert the trait.
    static_assert(decltype(m_session)::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection()");
    m_session.SetLocalSmoothing(m_config.localSmoothing);
    m_session.SetRemoteSmoothing(m_config.remoteSmoothing);
    Logger::Instance().Info("Position processor initialized (%s, sens=%.1f/%.1f/%.1f, limits=%.2f/%.2f/%.2f)",
                            m_session.IsPositionActive() ? "6DOF" : "3DOF only",
                            posSettings.sensitivity_x, posSettings.sensitivity_y, posSettings.sensitivity_z,
                            posSettings.limit_x, posSettings.limit_y, posSettings.limit_z);

    // Initialize hooks - we continue even if camera hook fails
    // This prevents the mod from crashing the game on pattern mismatch
    if (!InitializeHooks()) {
        Logger::Instance().Warning("Some hooks failed to initialize - mod may have limited functionality");
        Logger::Instance().Warning("The game will continue loading but head tracking may not work");
    }

    // The receiver's own diagnostics are all one-shot latched (first packet,
    // bind retry, parse failure), so forwarding them costs a handful of lines
    // and answers "did tracker data ever arrive" from the log alone.
    m_udpReceiver.SetLog([](const std::string& msg) {
        Logger::Instance().Info("UDP: %s", msg.c_str());
    });

    // Start() launches its own supervisor thread that keeps retrying the bind
    // even when the initial attempt fails (port held by another game's tracker
    // instance), so a false return here is not fatal - the socket is picked up
    // once it frees. Aborting init would stop that retry from ever reaching a
    // live camera hook.
    if (!m_udpReceiver.Start(m_config.udpPort)) {
        Logger::Instance().Warning("UDP receiver could not bind port %d yet - retrying in the background",
                                   m_config.udpPort);
    } else {
        Logger::Instance().Info("UDP receiver started on port %d", m_config.udpPort);
    }

    // Set initial enabled state based on auto-enable config
    if (m_config.autoEnable) {
        m_enabled.store(true);
        SetCameraHookEnabled(true);
        SetCrosshairEnabled(m_reticleEnabled);
        Logger::Instance().Info("Head tracking auto-enabled at startup");
    } else {
        m_enabled.store(false);
        SetCameraHookEnabled(false);
        SetCrosshairEnabled(false);
        Logger::Instance().Info("Head tracking disabled at startup (auto-enable is off)");
    }

    m_initialized.store(true);

    Logger::Instance().Info("Initialization complete (camera:%s, input:%s)",
                            m_cameraHookInstalled ? "OK" : "FAILED",
                            m_inputHookInstalled ? "OK" : "FAILED");

    // Log hotkey configuration for user reference
    Logger::Instance().Info("Hotkeys: %s=Toggle", VirtualKeyToString(m_config.toggleKey));

    // Show startup notification if enabled
    if (m_config.showNotifications) {
        std::string startupMsg = "DL2 Head Tracking v";
        startupMsg += DL2HT_VERSION;
        startupMsg += " - ";
        startupMsg += m_enabled.load() ? "ENABLED" : "DISABLED";
        ShowNotification(startupMsg.c_str());

        // Show hotkey hint after a delay
        std::string hotkeyHint = VirtualKeyToString(m_config.toggleKey);
        hotkeyHint += "=Toggle";
        ShowNotification(hotkeyHint.c_str());
    }

    return true;
}

void Mod::Shutdown() {
    if (!m_initialized.load()) {
        return;
    }

    Logger::Instance().Info("Shutting down...");

    // Stop UDP receiver
    m_udpReceiver.Stop();

    // Remove hooks
    ShutdownHooks();

    m_initialized.store(false);
    Logger::Instance().Info("Shutdown complete");
}

bool Mod::LoadConfig() {
    // Get path to config file (same directory as DLL)
    std::string configPath = GetModulePath("HeadTracking.ini");

    legacy::Config read;
    if (legacy::Read(configPath.c_str(), read) != legacy::ReadStatus::Read) {
        // Create default config file
        m_config.SetDefaults();
        // A file written before WorldLockedYaw existed has to keep reading as
        // camera-local, so only a freshly created file starts world-locked.
        m_config.worldLockedYaw = true;
        m_config.Save(configPath.c_str());
        return false;
    }

    m_config.udpPort = read.udpPort;
    m_config.yawMultiplier = read.yawMultiplier;
    m_config.pitchMultiplier = read.pitchMultiplier;
    m_config.rollMultiplier = read.rollMultiplier;
    m_config.toggleKey = read.toggleKey;
    m_config.trackingModeKey = read.trackingModeKey;
    m_config.yawModeKey = read.yawModeKey;
    m_config.reticleToggleKey = read.reticleToggleKey;
    m_config.worldLockedYaw = read.worldLockedYaw;
    m_config.positionSensitivityX = read.positionSensitivityX;
    m_config.positionSensitivityY = read.positionSensitivityY;
    m_config.positionSensitivityZ = read.positionSensitivityZ;
    m_config.positionLimitX = read.positionLimitX;
    m_config.positionLimitY = read.positionLimitY;
    m_config.positionLimitZ = read.positionLimitZ;
    m_config.positionLimitZBack = read.positionLimitZBack;
    m_config.positionInvertX = read.positionInvertX;
    m_config.positionInvertY = read.positionInvertY;
    m_config.positionInvertZ = read.positionInvertZ;
    m_config.positionEnabled = read.positionEnabled;
    m_config.localSmoothing = read.localSmoothing;
    m_config.remoteSmoothing = read.remoteSmoothing;
    m_config.reticleEnabled = read.reticleEnabled;
    m_config.autoEnable = read.autoEnable;
    m_config.showNotifications = read.showNotifications;
    return true;
}

bool Mod::InitializeHooks() {
    if (!HookManager::Instance().Initialize()) {
        Logger::Instance().Error("MinHook initialization failed");
        return false;
    }

    if (!InstallEngineCameraHook()) {
        Logger::Instance().Warning("Engine camera hook failed - head tracking disabled");
        m_cameraHookInstalled = false;
    } else {
        m_cameraHookInstalled = true;
        Logger::Instance().Info("Engine camera hook installed");
    }

    if (!InstallInputHook()) {
        Logger::Instance().Warning("Input hook failed - hotkeys won't work");
        m_inputHookInstalled = false;
    } else {
        m_inputHookInstalled = true;
        Logger::Instance().Info("Input hook installed");
    }

    // Crosshair hook for hiding stock crosshair - deferred
    // Will scan for GuiCrosshairData after game is fully loaded
    // DX hook for overlay is installed from camera hook on first frame
    InstallCrosshairHook();

    // Enable all hooks that were successfully installed
    __try {
        if (!HookManager::Instance().EnableAllHooks()) {
            Logger::Instance().Warning("Failed to enable some hooks");
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Logger::Instance().Error("CRASH enabling hooks - exception caught");
    }

    // Return true if at least input hook works (for hotkeys)
    return m_inputHookInstalled;
}

void Mod::ShutdownHooks() {
    // Always try to remove DX hook (may have been installed via deferred init)
    RemoveDXHook();

    // Remove crosshair hook
    RemoveCrosshairHook();

    if (m_inputHookInstalled) {
        RemoveInputHook();
        m_inputHookInstalled = false;
    }

    if (m_cameraHookInstalled) {
        RemoveEngineCameraHook();
        m_cameraHookInstalled = false;
    }

    HookManager::Instance().Shutdown();
}

void Mod::SetEnabled(bool enabled) {
    bool wasEnabled = m_enabled.exchange(enabled);
    if (wasEnabled != enabled) {
        // Update hooks directly
        SetCameraHookEnabled(enabled);
        SetCrosshairEnabled(enabled && m_reticleEnabled);
        SetStockCrosshairVisible(!enabled);

        if (enabled) {
            Logger::Instance().Info("Head tracking enabled");
            if (m_config.showNotifications) {
                ShowNotification("Head Tracking: ON");
                ShowNotification("Disable stock crosshair in Options>HUD");
            }
        } else {
            Logger::Instance().Info("Head tracking disabled");
            if (m_config.showNotifications) {
                ShowNotification("Head Tracking: OFF");
            }
        }
    }
}

void Mod::Toggle() {
    SetEnabled(!m_enabled.load());
}

void Mod::ToggleReticle() {
    m_reticleEnabled = !m_reticleEnabled;
    SetCrosshairEnabled(m_enabled.load() && m_reticleEnabled);
    Logger::Instance().Info("Reticle %s", m_reticleEnabled ? "enabled" : "disabled");
    if (m_config.showNotifications) {
        ShowNotification(m_reticleEnabled ? "Reticle: ON" : "Reticle: OFF");
    }
}

void Mod::CycleTrackingMode() {
    cameraunlock::TrackingMode mode = m_session.CycleMode();

    const char* label = nullptr;
    const char* notify = nullptr;
    switch (mode) {
        case cameraunlock::TrackingMode::RotationAndPosition:
            label = "Normal (rotation + position)";
            notify = "Tracking: Rotation + Position";
            break;
        case cameraunlock::TrackingMode::RotationOnly:
            label = "Rotation only";
            notify = "Tracking: Rotation Only";
            break;
        case cameraunlock::TrackingMode::PositionOnly:
            label = "Position only";
            notify = "Tracking: Position Only";
            break;
    }
    Logger::Instance().Info("Tracking mode: %s", label);
    if (m_config.showNotifications) {
        ShowNotification(notify);
    }
}

void Mod::ToggleYawMode() {
    bool nowWorldLocked = !m_worldLockedYaw.load();
    m_worldLockedYaw.store(nowWorldLocked);
    m_config.worldLockedYaw = nowWorldLocked;

    // Persist so the mode survives a relaunch.
    std::string configPath = GetModulePath("HeadTracking.ini");
    m_config.Save(configPath.c_str());

    const char* label = nowWorldLocked ? "WorldLocked" : "CameraLocal";
    Logger::Instance().Info("Yaw mode: %s", label);
    if (m_config.showNotifications) {
        ShowNotification(nowWorldLocked ? "Yaw Mode: World-Locked" : "Yaw Mode: Camera-Local");
    }
}

bool Mod::GetProcessedRotation(float& yaw, float& pitch, float& roll) {
    // Guard against multiple calls per frame (shadows, reflections, etc.)
    // A 1000μs threshold separates intra-frame passes from distinct frames.
    uint64_t now = cameraunlock::time::QpcNowMicros();
    if (m_lastProcessTime > 0 && (now - m_lastProcessTime) < 1000) {
        yaw = m_cachedYaw;
        pitch = m_cachedPitch;
        roll = m_cachedRoll;
        return m_cachedValid;
    }

    // Calculate delta time for frame-rate independent smoothing
    float deltaTime = 0.016f;  // Default for first call
    if (m_lastProcessTime > 0) {
        deltaTime = (now - m_lastProcessTime) / 1000000.0f;  // microseconds → seconds
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        if (deltaTime < 0.0001f) deltaTime = 0.0001f;
    }
    m_lastProcessTime = now;

    // Run the full tracking pipeline (rotation + position) once per frame
    m_cachedValid = m_session.Update(deltaTime);
    m_session.GetRotation(m_cachedYaw, m_cachedPitch, m_cachedRoll);

    yaw = m_cachedYaw;
    pitch = m_cachedPitch;
    roll = m_cachedRoll;
    return m_cachedValid;
}

bool Mod::GetPositionOffset(float& x, float& y, float& z) {
    // Position is computed by the session in GetProcessedRotation's Update call
    return m_session.GetPositionOffset(x, y, z);
}

} // namespace DL2HT
