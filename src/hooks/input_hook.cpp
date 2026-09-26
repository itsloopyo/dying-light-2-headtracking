#include "pch.h"
#include "input_hook.h"
#include "core/mod.h"
#include "core/logger.h"
#include <cameraunlock/input/hotkey_poller.h>
#include <cameraunlock/input/key_binding_registration.h>
#include <cameraunlock/input/key_bindings.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace DL2HT {

static cameraunlock::input::HotkeyPoller g_poller;
static bool g_hotkeysRegistered = false;

// The config table already refused a list that does not parse, so one here is a bug rather than
// a player's typo.
static std::vector<cameraunlock::input::KeyBinding> Parse(const std::string& list) {
    cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(list);
    if (!parsed.ok()) throw std::logic_error("hotkey list '" + list + "' does not parse: " + parsed.error);
    return parsed.bindings;
}

static void RegisterHotkeys(const Config& config) {
    // Each list from CameraUnlock.ini, chords included. A plain key does not fire while Ctrl and
    // Shift are both held, so Ctrl+Shift with a key reaches only a binding that names the chord.
    using cameraunlock::input::RegisterKeyBindings;
    RegisterKeyBindings(g_poller, Parse(config.toggle_key_name), [] {
        Logger::Instance().Debug("Toggle key pressed");
        Mod::Instance().Toggle();
    });
    RegisterKeyBindings(g_poller, Parse(config.cycle_tracking_mode_key_name), [] {
        Logger::Instance().Debug("Tracking mode key pressed");
        Mod::Instance().CycleTrackingMode();
    });
    RegisterKeyBindings(g_poller, Parse(config.yaw_mode_key_name), [] {
        Logger::Instance().Debug("Yaw mode key pressed");
        Mod::Instance().ToggleYawMode();
    });
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

    Logger::Instance().Info("Input hook installed - Toggle: %s", config.toggle_key_name.c_str());

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
