#pragma once

#include <cameraunlock/config/legacy_import.h>

#include <cstdint>
#include <vector>

// The HeadTracking.ini reader as src/core/config.cpp held it at 852d20e, the last commit before
// the canonical config format, frozen so an old file converts exactly as the builds before it
// read it. Never edited: a change here changes what a player's old file means.
//
// It differs from that commit's Config::Load in three ways only. It fills this frozen copy of
// that commit's Config rather than the runtime one. It writes nothing: where the build wrote a
// file of defaults over a missing or unopenable one, this reports Absent or OpenFailed and
// leaves the frozen defaults, and the caller does what the build did next. And it tells a
// missing file from one that exists and cannot be opened, which the build's Load reported
// alike.
//
// The defaults are literals rather than the constants the runtime took them from (constants.h,
// and cameraunlock-core's PositionSettings and smoothing defaults), so a later default cannot
// move what an old file without the key means.

namespace DL2HT::legacy {

constexpr uint16_t kDefaultUdpPort             = 4242;
constexpr float    kDefaultMultiplier          = 1.0f;
constexpr int      kDefaultToggleKey           = 0x23;
constexpr int      kDefaultTrackingModeKey     = 0x21;
constexpr int      kDefaultYawModeKey          = 0x22;
constexpr int      kDefaultReticleToggleKey    = 0x2D;
constexpr bool     kDefaultWorldLockedYaw      = false;
constexpr float    kDefaultPositionSensitivity = 2.0f;
constexpr float    kDefaultPositionLimitX      = 0.30f;
constexpr float    kDefaultPositionLimitY      = 0.20f;
constexpr float    kDefaultPositionLimitZ      = 0.40f;
constexpr float    kDefaultPositionLimitZBack  = 0.10f;
constexpr bool     kDefaultPositionInvert      = false;
constexpr bool     kDefaultPositionEnabled     = true;
constexpr float    kDefaultLocalSmoothing      = static_cast<float>(0.0);
constexpr float    kDefaultRemoteSmoothing     = static_cast<float>(0.15);
constexpr bool     kDefaultReticleEnabled      = true;
constexpr bool     kDefaultAutoEnable          = true;
constexpr bool     kDefaultShowNotifications   = true;

struct Config {
    uint16_t udpPort = kDefaultUdpPort;

    float yawMultiplier = kDefaultMultiplier;
    float pitchMultiplier = kDefaultMultiplier;
    float rollMultiplier = kDefaultMultiplier;

    int toggleKey = kDefaultToggleKey;
    int trackingModeKey = kDefaultTrackingModeKey;
    int yawModeKey = kDefaultYawModeKey;
    int reticleToggleKey = kDefaultReticleToggleKey;

    bool worldLockedYaw = kDefaultWorldLockedYaw;

    float positionSensitivityX = kDefaultPositionSensitivity;
    float positionSensitivityY = kDefaultPositionSensitivity;
    float positionSensitivityZ = kDefaultPositionSensitivity;
    float positionLimitX = kDefaultPositionLimitX;
    float positionLimitY = kDefaultPositionLimitY;
    float positionLimitZ = kDefaultPositionLimitZ;
    float positionLimitZBack = kDefaultPositionLimitZBack;
    bool positionInvertX = kDefaultPositionInvert;
    bool positionInvertY = kDefaultPositionInvert;
    bool positionInvertZ = kDefaultPositionInvert;
    bool positionEnabled = kDefaultPositionEnabled;

    float localSmoothing = kDefaultLocalSmoothing;
    float remoteSmoothing = kDefaultRemoteSmoothing;

    bool reticleEnabled = kDefaultReticleEnabled;

    bool autoEnable = kDefaultAutoEnable;
    bool showNotifications = kDefaultShowNotifications;
};

enum class ReadStatus {
    // The file was read into the Config and validated as the build validated it.
    Read,
    // There is no file at the path. The Config holds the frozen defaults.
    Absent,
    // The file exists and could not be opened. The Config holds the frozen defaults.
    OpenFailed,
};

// Reads the file at the ANSI path through inih (extern/ini.c), as the build did, and validates
// what it read as its Validate did. Writes nothing. Logs through the mod's Logger as the build
// did.
ReadStatus Read(const char* iniPath, Config& cfg);

// Every section and key Read takes a value from. The retired [Position] Smoothing, which it
// only warns about, is not here: the build ignored its value.
std::vector<cameraunlock::config::LegacyKey> ReadKeys();

} // namespace DL2HT::legacy
