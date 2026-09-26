#pragma once

#include <cstdint>

namespace DL2HT {

// Version info
inline constexpr const char* DL2HT_VERSION = "0.1.0";

// Target game DLL
inline constexpr const char* DL2_GAME_DLL = "gamedll_ph_x64_rwdi.dll";

// Shared math constant
inline constexpr float DEG_TO_RAD = 0.0174533f;

// Every published build shipped [Position] SensitivityX/Y/Z=2.0: a lean moves the camera twice
// as far as the head moved. That factor is the mod's conversion from the tracker's metres to
// this camera, not a setting, so it stays here and the limits still bound the result.
inline constexpr float kPositionSensitivity = 2.0f;

} // namespace DL2HT
