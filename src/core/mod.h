#pragma once

#include "config.h"
#include <cameraunlock/protocol/udp_receiver.h>
#include <cameraunlock/processing/tracking_processor.h>
#include <cameraunlock/processing/pose_interpolator.h>
#include <cameraunlock/processing/position_processor.h>
#include <cameraunlock/processing/position_interpolator.h>

namespace DL2HT {

class Mod {
public:
    static Mod& Instance();

    bool Initialize();
    void Shutdown();

    bool IsEnabled() const { return m_enabled.load(); }
    void SetEnabled(bool enabled);
    void Toggle();

    void Recenter();
    void TogglePosition();
    void ToggleReticle();

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
    Mod() = default;
    ~Mod() = default;

    bool LoadConfig();
    bool InitializeHooks();
    void ShutdownHooks();

    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_initialized{false};

    Config m_config;
    cameraunlock::UdpReceiver m_udpReceiver;
    cameraunlock::PoseInterpolator m_poseInterpolator;
    cameraunlock::TrackingProcessor m_processor;
    int64_t m_lastReceiveTimestamp = 0;

    // Position processing (6DOF)
    cameraunlock::PositionProcessor m_positionProcessor;
    cameraunlock::PositionInterpolator m_positionInterpolator;
    bool m_positionEnabled = true;

    // Reticle overlay
    bool m_reticleEnabled = true;

    // Timing for frame-rate independent processing
    uint64_t m_lastProcessTime = 0;
    float m_lastDeltaTime = 0.016f;  // cached for GetPositionOffset to reuse

    // Cached rotation from last GetProcessedRotation
    // Used by GetPositionOffset and to prevent re-processing
    // when MoveCameraHook fires multiple times per frame (shadows, reflections, etc.)
    float m_cachedYaw = 0.0f;
    float m_cachedPitch = 0.0f;
    float m_cachedRoll = 0.0f;
    bool m_cachedValid = false;

    bool m_hasCentered = false;
    bool m_cameraHookInstalled = false;
    bool m_inputHookInstalled = false;
};

} // namespace DL2HT
