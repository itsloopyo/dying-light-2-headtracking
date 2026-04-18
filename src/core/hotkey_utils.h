#pragma once

#include <atomic>
#include <string>

namespace DL2HT {

// Convert virtual key code to human-readable string
const char* VirtualKeyToString(int vkCode);

// Check if a key code is valid for use as a hotkey
bool IsValidHotkeyCode(int vkCode);

// Format hotkey configuration as a string for display
std::string FormatHotkeyConfig(int toggleKey, int recenterKey);

// Rising-edge detector for a virtual key. Returns true exactly once per
// press (when the key transitions from up to down), and false otherwise.
// `downState` is updated in place to track the current key state.
bool DetectEdge(int vkCode, std::atomic<bool>& downState);

// Chord variant: only fires when `chordActive` is true (caller computes it
// from the Ctrl and Shift modifier state). Tracks its own down-state so the
// chord debounces independently of the primary key.
bool DetectChordEdge(int letterVk, std::atomic<bool>& downState, bool chordActive);

} // namespace DL2HT
