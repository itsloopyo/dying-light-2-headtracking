#pragma once

namespace DL2HT {

// Install the input polling thread for hotkey detection
bool InstallInputHook();

// Remove the input polling thread
void RemoveInputHook();

// Update hotkey bindings at runtime
// Only valid key codes (checked via IsValidHotkeyCode) will be applied
void SetHotkeys(int toggleKey, int recenterKey);

} // namespace DL2HT
