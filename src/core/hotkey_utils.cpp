#include "pch.h"
#include "hotkey_utils.h"
#include <cameraunlock/input/hotkey_poller.h>

namespace DL2HT {

// Delegate to core library, but extend with more key names for display
const char* VirtualKeyToString(int vkCode) {
    // First try core library
    const char* result = cameraunlock::input::VirtualKeyToString(vkCode);
    if (result && std::strcmp(result, "Unknown") != 0) {
        return result;
    }

    // Extended key names not in core library
    switch (vkCode) {
        // Navigation keys
        case 0x21: return "PageUp";
        case 0x22: return "PageDown";
        case 0x25: return "Left";
        case 0x26: return "Up";
        case 0x27: return "Right";
        case 0x28: return "Down";

        // Special keys
        case 0x08: return "Backspace";
        case 0x09: return "Tab";
        case 0x0D: return "Enter";
        case 0x14: return "CapsLock";

        // Modifier keys
        case 0x10: return "Shift";
        case 0x11: return "Ctrl";
        case 0x12: return "Alt";

        // OEM keys
        case 0xBA: return ";";
        case 0xBB: return "=";
        case 0xBC: return ",";
        case 0xBD: return "-";
        case 0xBE: return ".";
        case 0xBF: return "/";
        case 0xC0: return "`";
        case 0xDB: return "[";
        case 0xDC: return "\\";
        case 0xDD: return "]";
        case 0xDE: return "'";

        default: return result;  // Return core library result (may be "Unknown")
    }
}

bool IsValidHotkeyCode(int vkCode) {
    // Use core library validation as base
    if (cameraunlock::input::IsValidHotkeyCode(vkCode)) {
        return true;
    }

    // Letter keys A-Z
    if (vkCode >= 0x41 && vkCode <= 0x5A) return true;

    // Number keys 0-9
    if (vkCode >= 0x30 && vkCode <= 0x39) return true;

    // Navigation keys: PageUp, PageDown, Arrow keys
    if (vkCode >= 0x21 && vkCode <= 0x28) return true;

    return false;
}

std::string FormatHotkeyConfig(int toggleKey, int recenterKey) {
    std::ostringstream ss;
    ss << VirtualKeyToString(toggleKey) << "=Toggle, ";
    ss << VirtualKeyToString(recenterKey) << "=Recenter";
    return ss.str();
}

} // namespace DL2HT
