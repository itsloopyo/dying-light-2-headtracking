#pragma once

#include <cstdint>

namespace DL2HT {

// Version info
inline constexpr const char* DL2HT_VERSION = "0.1.0";

// Target game DLL
inline constexpr const char* DL2_GAME_DLL = "gamedll_ph_x64_rwdi.dll";

// Default UDP port for OpenTrack
inline constexpr uint16_t DL2HT_DEFAULT_UDP_PORT = 4242;

// Shared math constant
inline constexpr float DEG_TO_RAD = 0.0174533f;

// Default hotkey virtual key codes
inline constexpr int DEFAULT_TOGGLE_KEY = 0x23;    // VK_END - Enable/disable tracking
inline constexpr int DEFAULT_RECENTER_KEY = 0x24;   // VK_HOME - Recenter view
inline constexpr int DEFAULT_POSITION_TOGGLE_KEY = 0x21; // VK_PRIOR (Page Up) - Toggle position
inline constexpr int DEFAULT_RETICLE_TOGGLE_KEY = 0x2D;  // VK_INSERT - Toggle reticle

} // namespace DL2HT
