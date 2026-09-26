#include "pch.h"
#include "legacy_config.h"
#include "core/logger.h"

extern "C" {
#include "ini.h"
}

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cmath>

namespace DL2HT::legacy {

namespace {

// Smoothing reaches cameraunlock::math::CalculateSmoothingFactor, which runs
// exp() on it. atof parses "nan" and "inf" without complaint, and std::clamp
// rejects neither because every comparison against NaN is false, so
// "LocalSmoothing=nan" would otherwise poison the smoothed pose for the rest of
// the session with nothing in the log. Reject it here instead. Validation only,
// never a floor: a configured 0.0 stays 0.0.
float SanitizeSmoothing(const char* key, float value, float fallback) {
    if (!std::isfinite(value)) {
        Logger::Instance().Warning("%s is not a finite number, using %.2f", key, fallback);
        return fallback;
    }
    if (value < 0.0f || value > 1.0f) {
        const float clamped = (value < 0.0f) ? 0.0f : 1.0f;
        Logger::Instance().Warning("%s=%g is outside [0,1], clamped to %.2f", key,
                                   static_cast<double>(value), clamped);
        return clamped;
    }
    return value;
}

// Warned once per process rather than once per load: config is reloadable, and
// repeating this on every reload buries it. The handler is called per key/value
// pair, so reaching this at all means the retired key is present in the user's
// INI - the one-shot flag is the only guard needed.
//
// The old value is deliberately NOT migrated into the new keys. The single
// Smoothing value carried a hidden 0.15 floor, so the number in an existing
// config does not mean what it used to: copying it across would hand a local
// user smoothing they never chose under the new semantics, and copying it into
// only one of the two keys would be a guess about which connection they were on.
void WarnRetiredSmoothingKey(const char* section, const char* key) {
    static bool warned = false;
    if (warned) return;
    warned = true;
    Logger::Instance().Warning(
        "Config key [%s] %s has been retired and is IGNORED. Smoothing is now two "
        "keys: LocalSmoothing (default 0, applies to a tracker on this machine) and "
        "RemoteSmoothing (default 0.15, applies to a tracker on the network). The "
        "old value is not migrated because the semantics changed - it carried a "
        "hidden 0.15 floor that no longer exists. Set the two new keys.",
        section, key);
}

void Validate(Config& c) {
    // Clamp sensitivity values
    c.yawMultiplier = std::clamp(c.yawMultiplier, 0.1f, 5.0f);
    c.pitchMultiplier = std::clamp(c.pitchMultiplier, 0.1f, 5.0f);
    c.rollMultiplier = std::clamp(c.rollMultiplier, 0.0f, 2.0f);

    // Clamp position sensitivity
    c.positionSensitivityX = std::clamp(c.positionSensitivityX, 0.1f, 10.0f);
    c.positionSensitivityY = std::clamp(c.positionSensitivityY, 0.1f, 10.0f);
    c.positionSensitivityZ = std::clamp(c.positionSensitivityZ, 0.1f, 10.0f);

    // Clamp position limits (meters)
    c.positionLimitX = std::clamp(c.positionLimitX, 0.01f, 2.0f);
    c.positionLimitY = std::clamp(c.positionLimitY, 0.01f, 2.0f);
    c.positionLimitZ = std::clamp(c.positionLimitZ, 0.01f, 2.0f);
    c.positionLimitZBack = std::clamp(c.positionLimitZBack, 0.01f, 2.0f);

    // Validation only (finite, range [0,1]). Not a floor: an in-range value
    // passes through exactly as the user set it. Each key falls back to its own
    // default, never to a shared one - a bad RemoteSmoothing dropping to the
    // local 0.0 would leave a phone's network jitter entirely unsmoothed.
    c.localSmoothing = SanitizeSmoothing("LocalSmoothing", c.localSmoothing, kDefaultLocalSmoothing);
    c.remoteSmoothing = SanitizeSmoothing("RemoteSmoothing", c.remoteSmoothing, kDefaultRemoteSmoothing);

    // Validate port range
    if (c.udpPort < 1024) {
        Logger::Instance().Warning("UDP port %d is in reserved range, using default %d",
                                   c.udpPort, kDefaultUdpPort);
        c.udpPort = kDefaultUdpPort;
    }
}

int ConfigHandler(void* user, const char* section, const char* name, const char* value) {
    Config* config = static_cast<Config*>(user);

#define MATCH(s, n) (strcmp(section, s) == 0 && strcmp(name, n) == 0)

    // Network section
    if (MATCH("Network", "UDPPort")) {
        config->udpPort = static_cast<uint16_t>(atoi(value));
    }
    // Sensitivity section
    else if (MATCH("Sensitivity", "YawMultiplier")) {
        config->yawMultiplier = static_cast<float>(atof(value));
    } else if (MATCH("Sensitivity", "PitchMultiplier")) {
        config->pitchMultiplier = static_cast<float>(atof(value));
    } else if (MATCH("Sensitivity", "RollMultiplier")) {
        config->rollMultiplier = static_cast<float>(atof(value));
    }
    // Hotkeys section
    else if (MATCH("Hotkeys", "ToggleKey")) {
        config->toggleKey = static_cast<int>(strtol(value, nullptr, 0));
    } else if (MATCH("Hotkeys", "TrackingModeKey") || MATCH("Hotkeys", "PositionToggleKey")) {
        // PositionToggleKey is the legacy name for TrackingModeKey; both bind the same physical hotkey.
        config->trackingModeKey = static_cast<int>(strtol(value, nullptr, 0));
    } else if (MATCH("Hotkeys", "YawModeKey")) {
        config->yawModeKey = static_cast<int>(strtol(value, nullptr, 0));
    } else if (MATCH("Hotkeys", "ReticleToggleKey")) {
        config->reticleToggleKey = static_cast<int>(strtol(value, nullptr, 0));
    }
    // Rotation section
    else if (MATCH("Rotation", "WorldLockedYaw")) {
        config->worldLockedYaw = (strcmp(value, "true") == 0 || atoi(value) == 1);
    }
    // Position section
    else if (MATCH("Position", "SensitivityX")) {
        config->positionSensitivityX = static_cast<float>(atof(value));
    } else if (MATCH("Position", "SensitivityY")) {
        config->positionSensitivityY = static_cast<float>(atof(value));
    } else if (MATCH("Position", "SensitivityZ")) {
        config->positionSensitivityZ = static_cast<float>(atof(value));
    } else if (MATCH("Position", "LimitX")) {
        config->positionLimitX = static_cast<float>(atof(value));
    } else if (MATCH("Position", "LimitY")) {
        config->positionLimitY = static_cast<float>(atof(value));
    } else if (MATCH("Position", "LimitZ")) {
        config->positionLimitZ = static_cast<float>(atof(value));
    } else if (MATCH("Position", "LimitZBack")) {
        config->positionLimitZBack = static_cast<float>(atof(value));
    } else if (MATCH("Position", "InvertX")) {
        config->positionInvertX = (strcmp(value, "true") == 0 || atoi(value) == 1);
    } else if (MATCH("Position", "InvertY")) {
        config->positionInvertY = (strcmp(value, "true") == 0 || atoi(value) == 1);
    } else if (MATCH("Position", "InvertZ")) {
        config->positionInvertZ = (strcmp(value, "true") == 0 || atoi(value) == 1);
    } else if (MATCH("Position", "Enabled")) {
        config->positionEnabled = (strcmp(value, "true") == 0 || atoi(value) == 1);
    }
    // Retired key: present in older configs, deliberately not migrated.
    else if (MATCH("Position", "Smoothing")) {
        WarnRetiredSmoothingKey("Position", "Smoothing");
    }
    // Smoothing section
    else if (MATCH("Smoothing", "LocalSmoothing")) {
        config->localSmoothing = static_cast<float>(atof(value));
    } else if (MATCH("Smoothing", "RemoteSmoothing")) {
        config->remoteSmoothing = static_cast<float>(atof(value));
    }
    // Reticle section
    else if (MATCH("Reticle", "Enabled")) {
        config->reticleEnabled = (strcmp(value, "true") == 0 || atoi(value) == 1);
    }
    // General section
    else if (MATCH("General", "AutoEnable")) {
        config->autoEnable = (strcmp(value, "true") == 0 || atoi(value) == 1);
    } else if (MATCH("General", "ShowNotifications")) {
        config->showNotifications = (strcmp(value, "true") == 0 || atoi(value) == 1);
    }

#undef MATCH

    return 1; // Success
}

}  // namespace

ReadStatus Read(const char* iniPath, Config& cfg) {
    cfg = Config{};

    int result = ini_parse(iniPath, ConfigHandler, &cfg);
    if (result < 0) {
        Logger::Instance().Warning("Could not load config from %s, using defaults", iniPath);
        cfg = Config{};
        if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) return ReadStatus::Absent;
        return ReadStatus::OpenFailed;
    }
    if (result > 0) {
        Logger::Instance().Warning("Config parse error on line %d", result);
    }

    Validate(cfg);
    Logger::Instance().Info("Config loaded from %s", iniPath);
    return ReadStatus::Read;
}

std::vector<cameraunlock::config::LegacyKey> ReadKeys() {
    return {
        {"Network", "UDPPort"},
        {"Sensitivity", "YawMultiplier"},
        {"Sensitivity", "PitchMultiplier"},
        {"Sensitivity", "RollMultiplier"},
        {"Hotkeys", "ToggleKey"},
        {"Hotkeys", "TrackingModeKey"},
        {"Hotkeys", "PositionToggleKey"},
        {"Hotkeys", "YawModeKey"},
        {"Hotkeys", "ReticleToggleKey"},
        {"Rotation", "WorldLockedYaw"},
        {"Position", "SensitivityX"},
        {"Position", "SensitivityY"},
        {"Position", "SensitivityZ"},
        {"Position", "LimitX"},
        {"Position", "LimitY"},
        {"Position", "LimitZ"},
        {"Position", "LimitZBack"},
        {"Position", "InvertX"},
        {"Position", "InvertY"},
        {"Position", "InvertZ"},
        {"Position", "Enabled"},
        {"Smoothing", "LocalSmoothing"},
        {"Smoothing", "RemoteSmoothing"},
        {"Reticle", "Enabled"},
        {"General", "AutoEnable"},
        {"General", "ShowNotifications"},
    };
}

} // namespace DL2HT::legacy
