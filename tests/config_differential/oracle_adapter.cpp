#include "oracle_adapter.h"

#include "pch.h"
#include "core/config.h"

namespace oracle_api {

namespace {

Config Copy(const dl2_oracle::Config& c) {
    Config out;
    out.udpPort = c.udpPort;
    out.yawMultiplier = c.yawMultiplier;
    out.pitchMultiplier = c.pitchMultiplier;
    out.rollMultiplier = c.rollMultiplier;
    out.toggleKey = c.toggleKey;
    out.trackingModeKey = c.trackingModeKey;
    out.yawModeKey = c.yawModeKey;
    out.reticleToggleKey = c.reticleToggleKey;
    out.worldLockedYaw = c.worldLockedYaw;
    out.positionSensitivityX = c.positionSensitivityX;
    out.positionSensitivityY = c.positionSensitivityY;
    out.positionSensitivityZ = c.positionSensitivityZ;
    out.positionLimitX = c.positionLimitX;
    out.positionLimitY = c.positionLimitY;
    out.positionLimitZ = c.positionLimitZ;
    out.positionLimitZBack = c.positionLimitZBack;
    out.positionInvertX = c.positionInvertX;
    out.positionInvertY = c.positionInvertY;
    out.positionInvertZ = c.positionInvertZ;
    out.positionEnabled = c.positionEnabled;
    out.localSmoothing = c.localSmoothing;
    out.remoteSmoothing = c.remoteSmoothing;
    out.reticleEnabled = c.reticleEnabled;
    out.autoEnable = c.autoEnable;
    out.showNotifications = c.showNotifications;
    return out;
}

}  // namespace

Config Defaults() {
    return Copy(dl2_oracle::Config{});
}

// v1.4.0's Mod::LoadConfig (src/core/mod.cpp at 3421cad), less its logging.
LoadStatus LoadOrCreate(const char* iniPath, Config& out) {
    const bool existed = GetFileAttributesA(iniPath) != INVALID_FILE_ATTRIBUTES;
    dl2_oracle::Config c;
    LoadStatus status = LoadStatus::Read;
    if (!c.Load(iniPath)) {
        c.SetDefaults();
        c.Save(iniPath);
        status = existed ? LoadStatus::OpenFailed : LoadStatus::Created;
    }
    out = Copy(c);
    return status;
}

}  // namespace oracle_api
