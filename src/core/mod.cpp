#include "pch.h"
#include "mod.h"
#include "logger.h"
#include "path_utils.h"
#include "hooks/hook_manager.h"
#include "hooks/engine_camera_hook.h"
#include "hooks/input_hook.h"
#include "hooks/dx_hook.h"
#include "hooks/crosshair_hook.h"
#include "ui/notification.h"

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

    LoadConfig();

    // Initialize yaw rotation frame from config
    m_worldLockedYaw.store(m_config.world_space_yaw);

    m_session.SetMode(StartupTrackingMode(m_config));
    cameraunlock::PositionSettings posSettings = m_config.position;
    posSettings.sensitivity_x = kPositionSensitivity;
    posSettings.sensitivity_y = kPositionSensitivity;
    posSettings.sensitivity_z = kPositionSensitivity;
    m_session.GetPositionProcessor().SetSettings(posSettings);

    // After SetSettings, which would otherwise overwrite the smoothing fields.
    // The session forwards both values to the rotation AND position processors
    // and re-reads the receiver's connection locality inside every Update() to
    // pick the one that applies. Without IsRemoteConnection() on the receiver
    // that selection silently pins to local, so assert the trait.
    static_assert(decltype(m_session)::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection()");
    m_session.SetLocalSmoothing(m_config.local_smoothing);
    m_session.SetRemoteSmoothing(m_config.remote_smoothing);
    Logger::Instance().Info("Position processor initialized (%s, limits x=%.2f up=%.2f down=%.2f fwd=%.2f back=%.2f)",
                            m_session.IsPositionActive() ? "6DOF" : "3DOF only",
                            posSettings.limit_x, posSettings.limit_y, posSettings.limit_y_down,
                            posSettings.limit_z, posSettings.limit_z_back);

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
    if (!m_udpReceiver.Start(static_cast<uint16_t>(m_config.udp_port))) {
        Logger::Instance().Warning("UDP receiver could not bind port %d yet - retrying in the background",
                                   m_config.udp_port);
    } else {
        Logger::Instance().Info("UDP receiver started on port %d", m_config.udp_port);
    }

    if (m_config.enable_on_startup) {
        m_enabled.store(true);
        SetCameraHookEnabled(true);
        SetCrosshairEnabled(true);
        Logger::Instance().Info("Head tracking enabled at startup");
    } else {
        m_enabled.store(false);
        SetCameraHookEnabled(false);
        SetCrosshairEnabled(false);
        Logger::Instance().Info("Head tracking disabled at startup (EnableOnStartup is false)");
    }

    m_initialized.store(true);

    Logger::Instance().Info("Initialization complete (camera:%s, input:%s)",
                            m_cameraHookInstalled ? "OK" : "FAILED",
                            m_inputHookInstalled ? "OK" : "FAILED");

    // Every binding, not just the toggle: the log is the only place a player can read back what
    // this build is bound to.
    Logger::Instance().Info("Hotkeys: toggle=[%s] cycle tracking mode=[%s] yaw mode=[%s]",
                            m_config.toggle_key_name.c_str(),
                            m_config.cycle_tracking_mode_key_name.c_str(),
                            m_config.yaw_mode_key_name.c_str());

    // Show startup notification if enabled
    if (m_config.show_notifications) {
        std::string startupMsg = "DL2 Head Tracking v";
        startupMsg += DL2HT_VERSION;
        startupMsg += " - ";
        startupMsg += m_enabled.load() ? "ENABLED" : "DISABLED";
        ShowNotification(startupMsg.c_str());

        // Show hotkey hint after a delay
        std::string hotkeyHint = m_config.toggle_key_name;
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

void Mod::LoadConfig() {
    namespace cfg = cameraunlock::config;
    const std::wstring folder = GetModuleDirectoryW();
    if (folder.empty()) {
        // Refuse a CWD-relative config, which would read and write the wrong file.
        Logger::Instance().Error("Could not resolve the mod's folder for CameraUnlock.ini - using "
                                 "built-in defaults, and nothing is saved this session");
        m_config = MakeConfigTable().defaults();
        return;
    }

    m_configOwner.emplace(MakeConfigOwnerOptions(folder, cfg::DefaultsFile::PerUser()));
    const cfg::ConfigLoadResult<Config> loaded = m_configOwner->Load();
    for (const std::string& line : loaded.log) Logger::Instance().Info("%s", line.c_str());
    Logger::Instance().Info("Config: %s", cfg::ConfigLoadStatusName(loaded.status));
    if (!loaded.reason.empty()) Logger::Instance().Warning("%s", loaded.reason.c_str());
    // Every status hands back the settings to run on. A file the last version could not open ran
    // it on its defaults, and a LegacyRefused load gives exactly those.
    m_config = loaded.config;
}

void Mod::SaveToggle(const std::function<void(Config&)>& change) {
    if (!m_configOwner) {
        Logger::Instance().Warning("Not saved: CameraUnlock.ini has no known folder this session");
        return;
    }
    const cameraunlock::config::ConfigSaveResult saved = m_configOwner->Save(change);
    // A save that succeeds can carry a line too, naming a row that stopped following Defaults.ini.
    for (const std::string& line : saved.log) Logger::Instance().Info("%s", line.c_str());
    if (saved.status != cameraunlock::config::ConfigSaveStatus::Saved) {
        Logger::Instance().Warning("%s", saved.reason.c_str());
    }
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
            if (m_config.show_notifications) {
                ShowNotification("Head Tracking: ON");
                ShowNotification("Disable stock crosshair in Options>HUD");
            }
        } else {
            Logger::Instance().Info("Head tracking disabled");
            if (m_config.show_notifications) {
                ShowNotification("Head Tracking: OFF");
            }
        }
    }
}

void Mod::Toggle() {
    SetEnabled(!m_enabled.load());
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
    if (m_config.show_notifications) {
        ShowNotification(notify);
    }
    const cameraunlock::TrackingModeChannels channels = cameraunlock::EncodeTrackingMode(mode);
    SaveToggle([channels](Config& c) {
        c.rotation_enabled = channels.rotation_enabled;
        c.position_enabled = channels.position_enabled;
    });
}

void Mod::ToggleYawMode() {
    bool nowWorldLocked = !m_worldLockedYaw.load();
    m_worldLockedYaw.store(nowWorldLocked);

    const char* label = nowWorldLocked ? "WorldLocked" : "CameraLocal";
    Logger::Instance().Info("Yaw mode: %s", label);
    if (m_config.show_notifications) {
        ShowNotification(nowWorldLocked ? "Yaw Mode: World-Locked" : "Yaw Mode: Camera-Local");
    }
    SaveToggle([nowWorldLocked](Config& c) { c.world_space_yaw = nowWorldLocked; });
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
