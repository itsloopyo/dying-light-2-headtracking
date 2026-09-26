#pragma once

#include <cstdint>

// v1.4.0's Config, copied out field by field so the differential test can read it without
// including the oracle's config.h, whose names clash with this build's.
// oracle_adapter.cpp is compiled with the oracle, where DL2HT is renamed to dl2_oracle.

namespace oracle_api {

struct Config {
    uint16_t udpPort = 0;

    float yawMultiplier = 0.0f;
    float pitchMultiplier = 0.0f;
    float rollMultiplier = 0.0f;

    int toggleKey = 0;
    int trackingModeKey = 0;
    int yawModeKey = 0;
    int reticleToggleKey = 0;

    bool worldLockedYaw = false;

    float positionSensitivityX = 0.0f;
    float positionSensitivityY = 0.0f;
    float positionSensitivityZ = 0.0f;
    float positionLimitX = 0.0f;
    float positionLimitY = 0.0f;
    float positionLimitZ = 0.0f;
    float positionLimitZBack = 0.0f;
    bool positionInvertX = false;
    bool positionInvertY = false;
    bool positionInvertZ = false;
    bool positionEnabled = false;

    float localSmoothing = 0.0f;
    float remoteSmoothing = 0.0f;

    bool reticleEnabled = false;

    bool autoEnable = false;
    bool showNotifications = false;
};

// A default-constructed Config of v1.4.0.
Config Defaults();

enum class LoadStatus {
    // The file was read.
    Read,
    // There was no file, and the build wrote one of defaults.
    Created,
    // The file could not be opened. The build ran on its defaults and wrote them over it, which
    // fails where the file is held open with no sharing.
    OpenFailed,
};

// v1.4.0's Mod::LoadConfig on the file at `iniPath`: Config::Load, and when that fails
// Config::SetDefaults and Config::Save. The status says which of the three the build met.
LoadStatus LoadOrCreate(const char* iniPath, Config& out);

}  // namespace oracle_api
