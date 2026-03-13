#pragma once

namespace DL2HT {

// Install/remove DX12 Present hook for crosshair overlay
bool InstallDXHook();
void RemoveDXHook();

// Update crosshair offset based on head tracking (degrees)
// Positive yaw = looking right, crosshair appears left
// Positive pitch = looking up, crosshair appears down
// Roll is used to rotate the yaw/pitch offsets in screen space
void SetCrosshairOffset(float yaw, float pitch, float roll);

// Set the game camera's base pitch (from gamepad/mouse look, before head tracking)
// This is needed for correct reticle projection when looking up/down
void SetGameCameraPitch(float pitchRadians);

// Set field of view in degrees (detected from engine camera)
void SetCrosshairFOV(float fovDegrees);

// Enable/disable crosshair rendering
void SetCrosshairEnabled(bool enabled);

} // namespace DL2HT
