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
    int recenterKey = DEFAULT_RECENTER_KEY;
    int positionToggleKey = DEFAULT_POSITION_TOGGLE_KEY;

    // Position settings (6DOF)
    float positionSensitivityX = 2.0f;
    float positionSensitivityY = 2.0f;
    float positionSensitivityZ = 2.0f;
    float positionLimitX = 0.30f;
    float positionLimitY = 0.20f;
    float positionLimitZ = 0.40f;
    float positionSmoothing = 0.15f;
    bool positionInvertX = false;
    bool positionInvertY = false;
    bool positionInvertZ = false;
    bool positionEnabled = true;

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
