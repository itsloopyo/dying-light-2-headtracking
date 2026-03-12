#include "pch.h"
#include "config.h"
#include "logger.h"

extern "C" {
#include "ini.h"
}

#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace DL2HT {

void Config::SetDefaults() {
    udpPort = DL2HT_DEFAULT_UDP_PORT;

    yawMultiplier = 1.0f;
    pitchMultiplier = 1.0f;
    rollMultiplier = 1.0f;

    toggleKey = DEFAULT_TOGGLE_KEY;
    recenterKey = DEFAULT_RECENTER_KEY;
    positionToggleKey = DEFAULT_POSITION_TOGGLE_KEY;
    reticleToggleKey = DEFAULT_RETICLE_TOGGLE_KEY;

    positionSensitivityX = 2.0f;
    positionSensitivityY = 2.0f;
    positionSensitivityZ = 2.0f;
    positionLimitX = 0.30f;
    positionLimitY = 0.20f;
    positionLimitZ = 0.40f;
    positionLimitZBack = 0.10f;
    positionSmoothing = 0.15f;
    positionInvertX = false;
    positionInvertY = false;
    positionInvertZ = false;
    positionEnabled = true;

    reticleEnabled = true;

    autoEnable = true;
    showNotifications = true;
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

    // Clamp position smoothing
    positionSmoothing = std::clamp(positionSmoothing, 0.0f, 0.99f);

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
    } else if (MATCH("Hotkeys", "RecenterKey")) {
        config->recenterKey = static_cast<int>(strtol(value, nullptr, 0));
    } else if (MATCH("Hotkeys", "PositionToggleKey")) {
        config->positionToggleKey = static_cast<int>(strtol(value, nullptr, 0));
    } else if (MATCH("Hotkeys", "ReticleToggleKey")) {
        config->reticleToggleKey = static_cast<int>(strtol(value, nullptr, 0));
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
    } else if (MATCH("Position", "Smoothing")) {
        config->positionSmoothing = static_cast<float>(atof(value));
    } else if (MATCH("Position", "InvertX")) {
        config->positionInvertX = (strcmp(value, "true") == 0 || atoi(value) == 1);
    } else if (MATCH("Position", "InvertY")) {
        config->positionInvertY = (strcmp(value, "true") == 0 || atoi(value) == 1);
    } else if (MATCH("Position", "InvertZ")) {
        config->positionInvertZ = (strcmp(value, "true") == 0 || atoi(value) == 1);
    } else if (MATCH("Position", "Enabled")) {
        config->positionEnabled = (strcmp(value, "true") == 0 || atoi(value) == 1);
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
    file << "; Smoothing factor (0.0 = none, 0.99 = maximum)\n";
    file << "Smoothing=" << positionSmoothing << "\n";
    file << "; Invert position axes\n";
    file << "InvertX=" << (positionInvertX ? "true" : "false") << "\n";
    file << "InvertY=" << (positionInvertY ? "true" : "false") << "\n";
    file << "InvertZ=" << (positionInvertZ ? "true" : "false") << "\n";
    file << "; Enable/disable position tracking (6DOF)\n";
    file << "Enabled=" << (positionEnabled ? "true" : "false") << "\n\n";

    file << "[Hotkeys]\n";
    file << "; Virtual key codes (hex)\n";
    file << "ToggleKey=0x" << std::hex << toggleKey << "    ; End - Enable/disable\n";
    file << "RecenterKey=0x" << std::hex << recenterKey << "  ; Home - Recenter view\n";
    file << "PositionToggleKey=0x" << std::hex << positionToggleKey << " ; Page Up - Toggle position\n";
    file << "ReticleToggleKey=0x" << std::hex << reticleToggleKey << "  ; Insert - Toggle reticle\n\n";

    file << "[Reticle]\n";
    file << "; Show the head tracking reticle overlay\n";
    file << "Enabled=" << (reticleEnabled ? "true" : "false") << "\n\n";

    file << "[General]\n";
    file << "; Auto-enable tracking on game start\n";
    file << "AutoEnable=" << (autoEnable ? "true" : "false") << "\n";
    file << "; Show on-screen notifications\n";
    file << "ShowNotifications=" << (showNotifications ? "true" : "false") << "\n";

    file.close();
    Logger::Instance().Info("Config saved to %s", path);
    return true;
}

} // namespace DL2HT
