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

    // Initialize TrackingProcessor with sensitivity settings
    cameraunlock::SensitivitySettings sensitivity;
    sensitivity.yaw = m_config.yawMultiplier;
    sensitivity.pitch = m_config.pitchMultiplier;
    sensitivity.roll = m_config.rollMultiplier;
    m_processor.SetSensitivity(sensitivity);

    Logger::Instance().Info("TrackingProcessor initialized with sensitivity: yaw=%.2f pitch=%.2f roll=%.2f",
                            sensitivity.yaw, sensitivity.pitch, sensitivity.roll);

    // Initialize position processor (6DOF)
    m_positionEnabled = m_config.positionEnabled;
    cameraunlock::PositionSettings posSettings(
        m_config.positionSensitivityX, m_config.positionSensitivityY, m_config.positionSensitivityZ,
        m_config.positionLimitX, m_config.positionLimitY, m_config.positionLimitZ,
        m_config.positionSmoothing,
        m_config.positionInvertX, m_config.positionInvertY, m_config.positionInvertZ
    );
    m_positionProcessor.SetSettings(posSettings);
    m_positionProcessor.SetNeckModelSettings(cameraunlock::NeckModelSettings::Disabled());
    Logger::Instance().Info("Position processor initialized (%s, sens=%.1f/%.1f/%.1f, limits=%.2f/%.2f/%.2f)",
                            m_positionEnabled ? "6DOF" : "3DOF only",
                            posSettings.sensitivity_x, posSettings.sensitivity_y, posSettings.sensitivity_z,
                            posSettings.limit_x, posSettings.limit_y, posSettings.limit_z);

    // Initialize hooks - we continue even if camera hook fails
    // This prevents the mod from crashing the game on pattern mismatch
    if (!InitializeHooks()) {
        Logger::Instance().Warning("Some hooks failed to initialize - mod may have limited functionality");
        Logger::Instance().Warning("The game will continue loading but head tracking may not work");
    }

    if (!m_udpReceiver.Start(m_config.udpPort)) {
        Logger::Instance().Error("UDP receiver failed to start on port %d", m_config.udpPort);
        ShutdownHooks();
        return false;
    }
    Logger::Instance().Info("UDP receiver started on port %d", m_config.udpPort);

    // Set initial enabled state based on auto-enable config
    if (m_config.autoEnable) {
        m_enabled.store(true);
        SetCameraHookEnabled(true);
        Logger::Instance().Info("Head tracking auto-enabled at startup");
    } else {
        m_enabled.store(false);
        SetCameraHookEnabled(false);
        Logger::Instance().Info("Head tracking disabled at startup (auto-enable is off)");
    }

    m_initialized.store(true);

    Logger::Instance().Info("Initialization complete (camera:%s, input:%s)",
                            m_cameraHookInstalled ? "OK" : "FAILED",
                            m_inputHookInstalled ? "OK" : "FAILED");

    // Log hotkey configuration for user reference
    Logger::Instance().Info("Hotkeys: %s",
        FormatHotkeyConfig(m_config.toggleKey, m_config.recenterKey).c_str());

    // Show startup notification if enabled
    if (m_config.showNotifications) {
        std::string startupMsg = "DL2 Head Tracking v";
        startupMsg += DL2HT_VERSION;
        startupMsg += " - ";
        startupMsg += m_enabled.load() ? "ENABLED" : "DISABLED";
        ShowNotification(startupMsg.c_str());

        // Show hotkey hint after a delay
        std::string hotkeyHint = VirtualKeyToString(m_config.toggleKey);
        hotkeyHint += "=Toggle, ";
        hotkeyHint += VirtualKeyToString(m_config.recenterKey);
        hotkeyHint += "=Recenter";
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

    // Try to load config
    if (!m_config.Load(configPath.c_str())) {
        // Create default config file
        m_config.SetDefaults();
        m_config.Save(configPath.c_str());
        return false;
    }

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
        SetCrosshairEnabled(enabled);
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

void Mod::Recenter() {
    // Store current tracking values as neutral offset
    m_udpReceiver.Recenter();

    // Also reset the TrackingProcessor's and interpolator's smoothed values
    m_processor.Reset();
    m_poseInterpolator.Reset();
    m_lastProcessTime = 0;

    // Recenter position tracking
    float px, py, pz;
    if (m_udpReceiver.GetPosition(px, py, pz)) {
        cameraunlock::PositionData posCenter(px, py, pz);
        m_positionProcessor.SetCenter(posCenter);
    }
    m_positionInterpolator.Reset();

    Logger::Instance().Info("View recentered");
    if (m_config.showNotifications) {
        ShowNotification("View Recentered");
    }
}

void Mod::TogglePosition() {
    m_positionEnabled = !m_positionEnabled;
    if (!m_positionEnabled) {
        m_positionProcessor.Reset();
        m_positionInterpolator.Reset();
    }
    Logger::Instance().Info("Position tracking %s", m_positionEnabled ? "enabled" : "disabled");
    if (m_config.showNotifications) {
        ShowNotification(m_positionEnabled ? "Position Tracking: ON" : "Position Tracking: OFF");
    }
}

bool Mod::GetProcessedRotation(float& yaw, float& pitch, float& roll) {
    // Guard against multiple calls per frame (shadows, reflections, etc.)
    // Return cached result if the tick hasn't advanced.
    uint64_t now = GetTickCount64();
    if (m_lastProcessTime > 0 && now == m_lastProcessTime) {
        yaw = m_cachedYaw;
        pitch = m_cachedPitch;
        roll = m_cachedRoll;
        return m_cachedValid;
    }

    // Get raw data from UDP receiver
    float rawYaw, rawPitch, rawRoll;
    if (!m_udpReceiver.GetRotation(rawYaw, rawPitch, rawRoll)) {
        m_lastProcessTime = now;
        m_cachedValid = false;
        return false;
    }

    // Auto-recenter on first valid tracking frame
    if (!m_hasCentered) {
        m_hasCentered = true;
        Recenter();
    }

    // Calculate delta time for frame-rate independent smoothing
    float deltaTime = 0.016f;  // Default for first call
    if (m_lastProcessTime > 0) {
        deltaTime = (now - m_lastProcessTime) / 1000.0f;
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        if (deltaTime < 0.001f) deltaTime = 0.001f;
    }
    m_lastProcessTime = now;

    // Detect new tracking samples by comparing receive timestamps
    int64_t receiveTs = m_udpReceiver.GetLastReceiveTimestamp();
    bool isNewSample = (receiveTs != m_lastReceiveTimestamp);
    m_lastReceiveTimestamp = receiveTs;

    // Interpolate between tracking samples (fills in stale frames with extrapolation)
    cameraunlock::InterpolatedPose interpolated = m_poseInterpolator.Update(
        rawYaw, rawPitch, rawRoll, isNewSample, deltaTime);

    // Process through the pipeline (applies offset, smoothing, sensitivity)
    bool isRemote = m_udpReceiver.IsRemoteConnection();
    cameraunlock::TrackingPose processed = m_processor.Process(
        interpolated.yaw, interpolated.pitch, interpolated.roll, isRemote, deltaTime);

    yaw = processed.yaw;
    pitch = processed.pitch;
    roll = processed.roll;

    // Cache result (prevents re-processing on multiple calls per frame,
    // and provides rotation for GetPositionOffset neck model)
    m_cachedYaw = yaw;
    m_cachedPitch = pitch;
    m_cachedRoll = roll;
    m_cachedValid = true;

    return true;
}

bool Mod::GetPositionOffset(float& x, float& y, float& z) {
    if (!m_positionEnabled) {
        x = y = z = 0.0f;
        return false;
    }

    // Get raw position from UDP receiver
    float rawX, rawY, rawZ;
    if (!m_udpReceiver.GetPosition(rawX, rawY, rawZ)) {
        x = y = z = 0.0f;
        return false;
    }

    // Calculate delta time (reuse same timing as rotation)
    uint64_t now = GetTickCount64();
    float deltaTime = 0.016f;
    if (m_lastProcessTime > 0 && now > m_lastProcessTime) {
        deltaTime = (now - m_lastProcessTime) / 1000.0f;
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        if (deltaTime < 0.001f) deltaTime = 0.001f;
    }

    // Build PositionData
    cameraunlock::PositionData rawPos(rawX, rawY, rawZ);

    // Interpolate position
    cameraunlock::PositionData interpolatedPos = m_positionInterpolator.Update(rawPos, deltaTime);

    // Use cached rotation for neck model (set by last GetProcessedRotation call)
    float yaw = m_cachedYaw;
    float pitch = m_cachedPitch;
    float roll = m_cachedRoll;
    cameraunlock::math::Quat4 headRotQ = cameraunlock::math::Quat4::FromYawPitchRoll(
        yaw * cameraunlock::math::kDegToRad,
        pitch * cameraunlock::math::kDegToRad,
        roll * cameraunlock::math::kDegToRad);

    bool isRemote = m_udpReceiver.IsRemoteConnection();
    cameraunlock::math::Vec3 offset = m_positionProcessor.Process(interpolatedPos, headRotQ, isRemote, deltaTime);

    x = offset.x;
    y = offset.y;
    z = offset.z;
    return true;
}

} // namespace DL2HT
