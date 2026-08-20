#include "pch.h"
#include "config.h"
#include "logger.h"

extern "C" {
#include "ini.h"
}

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cmath>

namespace DL2HT {

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

}  // namespace

void Config::SetDefaults() {
    udpPort = DL2HT_DEFAULT_UDP_PORT;

    yawMultiplier = 1.0f;
    pitchMultiplier = 1.0f;
    rollMultiplier = 1.0f;

    toggleKey = DEFAULT_TOGGLE_KEY;
    trackingModeKey = DEFAULT_TRACKING_MODE_KEY;
    yawModeKey = DEFAULT_YAW_MODE_KEY;
    reticleToggleKey = DEFAULT_RETICLE_TOGGLE_KEY;

    worldLockedYaw = DEFAULT_WORLD_LOCKED_YAW;

    positionSensitivityX = 2.0f;
    positionSensitivityY = 2.0f;
    positionSensitivityZ = 2.0f;
    positionLimitX = 0.30f;
    positionLimitY = 0.20f;
    positionLimitZ = 0.40f;
    positionLimitZBack = 0.10f;
    positionInvertX = false;
    positionInvertY = false;
    positionInvertZ = false;
    positionEnabled = true;

    localSmoothing = 0.0f;
    remoteSmoothing = 0.15f;

    reticleEnabled = true;

    autoEnable = true;
    showNotifications = true;
    skipSplash = true;
}

void Config::Validate() {
    // Clamp sensitivity values
    yawMultiplier = std::clamp(yawMultiplier, 0.1f, 5.0f);
    pitchMultiplier = std::clamp(pitchMultiplier, 0.1f, 5.0f);
    rollMultiplier = std::clamp(rollMultiplier, 0.0f, 2.0f);

    // Clamp position sensitivity
    positionSensitivityX = std::clamp(positionSensitivityX, 0.1f, 10.0f);
    positionSensitivityY = std::clamp(positionSensitivityY, 0.1f, 10.0f);
    positionSensitivityZ = std::clamp(positionSensitivityZ, 0.1f, 10.0f);

    // Clamp position limits (meters)
    positionLimitX = std::clamp(positionLimitX, 0.01f, 2.0f);
    positionLimitY = std::clamp(positionLimitY, 0.01f, 2.0f);
    positionLimitZ = std::clamp(positionLimitZ, 0.01f, 2.0f);
    positionLimitZBack = std::clamp(positionLimitZBack, 0.01f, 2.0f);

    // Validation only (finite, range [0,1]). Not a floor: an in-range value
    // passes through exactly as the user set it. Each key falls back to its own
    // default, never to a shared one - a bad RemoteSmoothing dropping to the
    // local 0.0 would leave a phone's network jitter entirely unsmoothed.
    localSmoothing = SanitizeSmoothing("LocalSmoothing", localSmoothing, 0.0f);
    remoteSmoothing = SanitizeSmoothing("RemoteSmoothing", remoteSmoothing, 0.15f);

    // Validate port range
    if (udpPort < 1024) {
        Logger::Instance().Warning("UDP port %d is in reserved range, using default %d",
                                   udpPort, DL2HT_DEFAULT_UDP_PORT);
        udpPort = DL2HT_DEFAULT_UDP_PORT;
    }
}

int Config::ConfigHandler(void* user, const char* section, const char* name, const char* value) {
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
    } else if (MATCH("General", "SkipSplash")) {
        config->skipSplash = (strcmp(value, "true") == 0 || atoi(value) == 1);
    }

#undef MATCH

    return 1; // Success
}

bool Config::Load(const char* path) {
    SetDefaults();

    int result = ini_parse(path, ConfigHandler, this);
    if (result < 0) {
        Logger::Instance().Warning("Could not load config from %s, using defaults", path);
        return false;
    }
    if (result > 0) {
        Logger::Instance().Warning("Config parse error on line %d", result);
    }

    Validate();
    Logger::Instance().Info("Config loaded from %s", path);
    return true;
}

bool Config::Save(const char* path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        Logger::Instance().Error("Failed to save config to %s", path);
        return false;
    }

    file << "; DL2 Head Tracking Configuration\n";
    file << "; Delete this file to reset to defaults\n\n";

    file << "[Network]\n";
    file << "; UDP port for OpenTrack data (default: 4242)\n";
    file << "UDPPort=" << udpPort << "\n\n";

    file << "[Sensitivity]\n";
    file << "; Rotation sensitivity multipliers (1.0 = 1:1)\n";
    file << "YawMultiplier=" << yawMultiplier << "\n";
    file << "PitchMultiplier=" << pitchMultiplier << "\n";
    file << "RollMultiplier=" << rollMultiplier << "\n\n";

    file << "[Position]\n";
    file << "; Position tracking sensitivity (0.1-10.0, higher = more movement)\n";
    file << "SensitivityX=" << positionSensitivityX << "\n";
    file << "SensitivityY=" << positionSensitivityY << "\n";
    file << "SensitivityZ=" << positionSensitivityZ << "\n";
    file << "; Position limits in meters (how far the camera can move)\n";
    file << "LimitX=" << positionLimitX << "\n";
    file << "LimitY=" << positionLimitY << "\n";
    file << "LimitZ=" << positionLimitZ << "\n";
    file << "; Backward lean limit (prevents camera clipping through player model)\n";
    file << "LimitZBack=" << positionLimitZBack << "\n";
    file << "; Invert position axes\n";
    file << "InvertX=" << (positionInvertX ? "true" : "false") << "\n";
    file << "InvertY=" << (positionInvertY ? "true" : "false") << "\n";
    file << "InvertZ=" << (positionInvertZ ? "true" : "false") << "\n";
    file << "; Enable/disable position tracking (6DOF)\n";
    file << "Enabled=" << (positionEnabled ? "true" : "false") << "\n\n";

    file << "[Smoothing]\n";
    file << "; Smoothing is chosen per connection from the tracker's source address\n";
    file << "; and covers rotation and position. 0.0 = none, 1.0 = heavy.\n";
    file << "; LocalSmoothing: tracker running on this machine (loopback)\n";
    file << "LocalSmoothing=" << localSmoothing << "\n";
    file << "; RemoteSmoothing: tracker on a remote network device, e.g. a phone\n";
    file << "RemoteSmoothing=" << remoteSmoothing << "\n\n";

    file << "[Hotkeys]\n";
    file << "; Virtual key codes (hex)\n";
    file << "ToggleKey=0x" << std::hex << toggleKey << "    ; End - Enable/disable\n";
    file << "TrackingModeKey=0x" << std::hex << trackingModeKey << " ; Page Up - Cycle tracking mode\n";
    file << "YawModeKey=0x" << std::hex << yawModeKey << " ; Page Down - Toggle yaw mode\n";
    file << "ReticleToggleKey=0x" << std::hex << reticleToggleKey << "  ; Insert - Toggle reticle\n\n";

    file << "[Rotation]\n";
    file << "; Yaw rotation frame. false = camera-local (default); true = world-up (horizon-locked).\n";
    file << "WorldLockedYaw=" << (worldLockedYaw ? "true" : "false") << "\n\n";

    file << "[Reticle]\n";
    file << "; Show the head tracking reticle overlay\n";
    file << "Enabled=" << (reticleEnabled ? "true" : "false") << "\n\n";

    file << "[General]\n";
    file << "; Auto-enable tracking on game start\n";
    file << "AutoEnable=" << (autoEnable ? "true" : "false") << "\n";
    file << "; Show on-screen notifications\n";
    file << "ShowNotifications=" << (showNotifications ? "true" : "false") << "\n";
    file << "; Auto-skip publisher splash + intro cutscenes by posting ESC/SPACE\n";
    file << "; until the main menu is reached. Stops automatically at the menu\n";
    file << "; or after a 60s timeout.\n";
    file << "SkipSplash=" << (skipSplash ? "true" : "false") << "\n";

    file.close();
    Logger::Instance().Info("Config saved to %s", path);
    return true;
}

} // namespace DL2HT
