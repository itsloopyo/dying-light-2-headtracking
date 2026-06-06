#include "pch.h"
#include "input_hook.h"
#include "core/mod.h"
#include "core/logger.h"
#include "core/hotkey_utils.h"
#include <cameraunlock/input/hotkey_poller.h>
#include <cameraunlock/input/chord_hotkeys.h>

namespace DL2HT {

// Ctrl+Shift+<letter> chord fallbacks (CLAUDE.md cluster: T/Y/U/G/H/J).
// Mapping kept in sync with README and with the CLAUDE.md hotkey spec.
static constexpr int CHORD_RECENTER_VK = 0x54;         // T
static constexpr int CHORD_TOGGLE_VK = 0x59;           // Y
static constexpr int CHORD_TRACKING_MODE_VK = 0x47;    // G - cycle tracking mode
static constexpr int CHORD_YAW_MODE_VK = 0x48;         // H
static constexpr int CHORD_RETICLE_TOGGLE_VK = 0x55;   // U

static cameraunlock::input::HotkeyPoller g_poller;
static bool g_hotkeysRegistered = false;

static void RegisterHotkeys(const Config& config) {
    using cameraunlock::input::NavGuarded;
    using cameraunlock::input::ChordGuarded;

    // Primary nav-cluster keys fire only when the Ctrl+Shift chord is NOT
    // held, so Ctrl+Shift+End doesn't also trigger the End-only binding.
    g_poller.AddHotkey(config.toggleKey, NavGuarded([] {
        Logger::Instance().Debug("Toggle key pressed");
        Mod::Instance().Toggle();
    }));
    g_poller.AddHotkey(CHORD_TOGGLE_VK, ChordGuarded([] {
        Logger::Instance().Debug("Toggle chord (Ctrl+Shift+Y) pressed");
        Mod::Instance().Toggle();
    }));

    g_poller.AddHotkey(config.recenterKey, NavGuarded([] {
        Logger::Instance().Debug("Recenter key pressed");
        Mod::Instance().Recenter();
    }));
    g_poller.AddHotkey(CHORD_RECENTER_VK, ChordGuarded([] {
        Logger::Instance().Debug("Recenter chord (Ctrl+Shift+T) pressed");
        Mod::Instance().Recenter();
    }));

    g_poller.AddHotkey(config.trackingModeKey, NavGuarded([] {
        Logger::Instance().Debug("Tracking mode key pressed");
        Mod::Instance().CycleTrackingMode();
    }));
    g_poller.AddHotkey(CHORD_TRACKING_MODE_VK, ChordGuarded([] {
        Logger::Instance().Debug("Tracking mode chord (Ctrl+Shift+G) pressed");
        Mod::Instance().CycleTrackingMode();
    }));

    g_poller.AddHotkey(config.yawModeKey, NavGuarded([] {
        Logger::Instance().Debug("Yaw mode key pressed");
        Mod::Instance().ToggleYawMode();
    }));
    g_poller.AddHotkey(CHORD_YAW_MODE_VK, ChordGuarded([] {
        Logger::Instance().Debug("Yaw mode chord (Ctrl+Shift+H) pressed");
        Mod::Instance().ToggleYawMode();
    }));

    g_poller.AddHotkey(config.reticleToggleKey, NavGuarded([] {
        Logger::Instance().Debug("Reticle toggle key pressed");
        Mod::Instance().ToggleReticle();
    }));
    g_poller.AddHotkey(CHORD_RETICLE_TOGGLE_VK, ChordGuarded([] {
        Logger::Instance().Debug("Reticle toggle chord (Ctrl+Shift+U) pressed");
        Mod::Instance().ToggleReticle();
    }));
}

bool InstallInputHook() {
    if (g_poller.IsRunning()) {
        return true;
    }

    const Config& config = Mod::Instance().GetConfig();

    if (!g_hotkeysRegistered) {
        RegisterHotkeys(config);
        g_hotkeysRegistered = true;
    }

    if (!g_poller.Start(16)) {
        return false;
    }

    Logger::Instance().Info("Input hook installed - Toggle: %s, Recenter: %s",
        VirtualKeyToString(config.toggleKey), VirtualKeyToString(config.recenterKey));

    return true;
}

void RemoveInputHook() {
    if (!g_poller.IsRunning()) {
        return;
    }

    g_poller.Stop();
    Logger::Instance().Info("Input hook removed");
}

} // namespace DL2HT
