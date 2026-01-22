#pragma once

namespace DL2HT {

// Engine camera hook - intercepts MoveCameraFromForwardUpPos in engine_x64_rwdi.dll
// This is the main hook for applying head tracking rotation
bool InstallEngineCameraHook();
void RemoveEngineCameraHook();

// Enable/disable head tracking in the camera hook
void SetCameraHookEnabled(bool enabled);

// Refresh cached gameplay state (loading transitions, timer frozen)
// Call once per frame from DX hook to avoid redundant syscalls
void RefreshGameplayStateCache();

// Check if player is in gameplay (not menu, cutscene, or loading)
// Uses cached values from RefreshGameplayStateCache
bool IsInGameplay();

} // namespace DL2HT
