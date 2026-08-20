#pragma once

#include <cstdint>

namespace DL2HT {

struct Config {
    // Network settings
    uint16_t udpPort = DL2HT_DEFAULT_UDP_PORT;

    // Sensitivity multipliers
    float yawMultiplier = 1.0f;
    float pitchMultiplier = 1.0f;
    float rollMultiplier = 1.0f;

    // Hotkeys (Virtual Key codes)
    int toggleKey = DEFAULT_TOGGLE_KEY;
    int trackingModeKey = DEFAULT_TRACKING_MODE_KEY;  // Page Up - cycles tracking mode (legacy INI key: PositionToggleKey)
    int yawModeKey = DEFAULT_YAW_MODE_KEY;
    int reticleToggleKey = DEFAULT_RETICLE_TOGGLE_KEY;

    // Rotation settings
    bool worldLockedYaw = DEFAULT_WORLD_LOCKED_YAW;

    // Position settings (6DOF)
    float positionSensitivityX = 2.0f;
    float positionSensitivityY = 2.0f;
    float positionSensitivityZ = 2.0f;
    float positionLimitX = 0.30f;
    float positionLimitY = 0.20f;
    float positionLimitZ = 0.40f;
    float positionLimitZBack = 0.10f;  // backward lean limit (asymmetric)
    bool positionInvertX = false;
    bool positionInvertY = false;
    bool positionInvertZ = false;
    bool positionEnabled = true;

    // Smoothing settings. Chosen per connection from the packet source
    // address: a tracker on this machine (loopback) uses localSmoothing, a
    // remote network device uses remoteSmoothing. Both cover rotation and
    // position; there is no separate position smoothing setting.
    float localSmoothing = 0.0f;
    float remoteSmoothing = 0.15f;

    // Reticle settings
    bool reticleEnabled = true;

    // General settings
    bool autoEnable = true;
    bool showNotifications = true;

    // Load/Save
    bool Load(const char* path);
    bool Save(const char* path) const;
    void SetDefaults();
    void Validate();

private:
    static int ConfigHandler(void* user, const char* section, const char* name, const char* value);
};

} // namespace DL2HT
