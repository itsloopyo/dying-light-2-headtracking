#pragma once

namespace DL2HT {

// Install/remove DX12 Present hook for crosshair overlay
bool InstallDXHook();
void RemoveDXHook();

// Set the crosshair's aim-point projection in tangent space.
// tanRight/tanUp are the tangent of the angle between the head-tracked
// camera center and the aim point, computed from actual camera vectors
// in MoveCameraHook (matching the Subnautica VP-projection approach).
//   tanRight > 0 = aim is to the right of camera center
//   tanUp    > 0 = aim is above camera center
void SetCrosshairProjection(float tanRight, float tanUp);

// Set field of view in degrees (detected from engine camera)
void SetCrosshairFOV(float fovDegrees);

// Enable/disable crosshair rendering
void SetCrosshairEnabled(bool enabled);

} // namespace DL2HT
