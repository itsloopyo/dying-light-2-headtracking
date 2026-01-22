#pragma once

#include <string>

namespace DL2HT {

// Convert virtual key code to human-readable string
const char* VirtualKeyToString(int vkCode);

// Check if a key code is valid for use as a hotkey
bool IsValidHotkeyCode(int vkCode);

// Format hotkey configuration as a string for display
std::string FormatHotkeyConfig(int toggleKey, int recenterKey);

} // namespace DL2HT
