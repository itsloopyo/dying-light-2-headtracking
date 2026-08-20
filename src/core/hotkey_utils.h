#pragma once

namespace DL2HT {

// Convert virtual key code to human-readable string
const char* VirtualKeyToString(int vkCode);

// Check if a key code is valid for use as a hotkey
bool IsValidHotkeyCode(int vkCode);

} // namespace DL2HT
