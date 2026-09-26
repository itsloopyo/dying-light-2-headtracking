#include "pch.h"
#include "config.h"
#include "logger.h"

namespace DL2HT {

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
    positionLimitX = cameraunlock::PositionSettings{}.limit_x;
    positionLimitY = cameraunlock::PositionSettings{}.limit_y;
    positionLimitZ = cameraunlock::PositionSettings{}.limit_z;
    positionLimitZBack = cameraunlock::PositionSettings{}.limit_z_back;
    positionInvertX = false;
    positionInvertY = false;
    positionInvertZ = false;
    positionEnabled = true;

    localSmoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    remoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    reticleEnabled = true;

    autoEnable = true;
    showNotifications = true;
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
    file << "; Yaw rotation frame. true = world-up (horizon-locked, default); false = camera-local.\n";
    file << "WorldLockedYaw=" << (worldLockedYaw ? "true" : "false") << "\n\n";

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
