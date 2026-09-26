#pragma once

#include "config.h"
#include "rotation_math.h"
#include <cameraunlock/config/config_owner.h>
#include <cameraunlock/protocol/udp_receiver.h>
#include <cameraunlock/tracking/head_tracking_session.h>

#include <functional>
#include <optional>

namespace DL2HT {

class Mod {
public:
    static Mod& Instance();

    bool Initialize();
    void Shutdown();

    bool IsEnabled() const { return m_enabled.load(); }
    void SetEnabled(bool enabled);
    void Toggle();

    void CycleTrackingMode();
    void ToggleYawMode();

    YawMode GetYawMode() const {
        return m_worldLockedYaw.load() ? YawMode::WorldLocked : YawMode::CameraLocal;
    }

    Config& GetConfig() { return m_config; }
    const Config& GetConfig() const { return m_config; }

    // Get processed (smoothed) rotation values for rendering
    // Returns true if data is valid, false otherwise
    bool GetProcessedRotation(float& yaw, float& pitch, float& roll);

    // Get processed position offset (meters)
    // Returns true if position data is valid
    bool GetPositionOffset(float& x, float& y, float& z);

    // Delete copy/move
    Mod(const Mod&) = delete;
    Mod& operator=(const Mod&) = delete;

private:
    Mod() : m_session(m_udpReceiver) {}
    ~Mod() = default;

    void LoadConfig();
    // Applies nothing: the caller has already changed the running state. A failed save is logged
    // and the session keeps the new value.
    void SaveToggle(const std::function<void(Config&)>& change);
    bool InitializeHooks();
    void ShutdownHooks();

    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_initialized{false};

    Config m_config;
    // The one reader and writer of CameraUnlock.ini. Hotkeys save through it after LoadConfig has
    // built it on the init thread.
    std::optional<cameraunlock::config::ConfigOwner<Config>> m_configOwner;
    cameraunlock::UdpReceiver m_udpReceiver;
    cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver> m_session;

    // Yaw rotation frame (PgDn / Ctrl+Shift+H toggles)
    std::atomic<bool> m_worldLockedYaw{false};

    // Timing for frame-rate independent processing
    uint64_t m_lastProcessTime = 0;

    // Cached rotation from last GetProcessedRotation
    // Used to prevent re-processing when MoveCameraHook fires multiple
    // times per frame (shadows, reflections, etc.)
    float m_cachedYaw = 0.0f;
    float m_cachedPitch = 0.0f;
    float m_cachedRoll = 0.0f;
    bool m_cachedValid = false;

    bool m_cameraHookInstalled = false;
    bool m_inputHookInstalled = false;
};

} // namespace DL2HT
