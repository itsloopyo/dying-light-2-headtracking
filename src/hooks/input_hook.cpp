#include "pch.h"
#include "input_hook.h"
#include "core/mod.h"
#include "core/logger.h"
#include "core/hotkey_utils.h"

namespace DL2HT {

static std::thread g_inputThread;
static std::atomic<bool> g_stopFlag{false};
static std::atomic<bool> g_running{false};

static std::atomic<int> g_toggleKey{DEFAULT_TOGGLE_KEY};
static std::atomic<int> g_recenterKey{DEFAULT_RECENTER_KEY};
static std::atomic<int> g_positionToggleKey{DEFAULT_POSITION_TOGGLE_KEY};

// Key state tracking for debouncing
static std::atomic<bool> g_toggleKeyDown{false};
static std::atomic<bool> g_recenterKeyDown{false};
static std::atomic<bool> g_positionToggleKeyDown{false};

static void InputPollingThread() {
    Logger::Instance().Debug("Input polling thread started");

    while (!g_stopFlag.load()) {
        int toggleKey = g_toggleKey.load();
        int recenterKey = g_recenterKey.load();

        bool togglePressed = (GetAsyncKeyState(toggleKey) & 0x8000) != 0;
        if (togglePressed && !g_toggleKeyDown.load()) {
            g_toggleKeyDown.store(true);
            Logger::Instance().Debug("Toggle key (%s) pressed", VirtualKeyToString(toggleKey));
            Mod::Instance().Toggle();
        } else if (!togglePressed && g_toggleKeyDown.load()) {
            g_toggleKeyDown.store(false);
        }

        bool recenterPressed = (GetAsyncKeyState(recenterKey) & 0x8000) != 0;
        if (recenterPressed && !g_recenterKeyDown.load()) {
            g_recenterKeyDown.store(true);
            Logger::Instance().Debug("Recenter key (%s) pressed", VirtualKeyToString(recenterKey));
            Mod::Instance().Recenter();
        } else if (!recenterPressed && g_recenterKeyDown.load()) {
            g_recenterKeyDown.store(false);
        }

        int positionToggleKey = g_positionToggleKey.load();
        bool positionTogglePressed = (GetAsyncKeyState(positionToggleKey) & 0x8000) != 0;
        if (positionTogglePressed && !g_positionToggleKeyDown.load()) {
            g_positionToggleKeyDown.store(true);
            Logger::Instance().Debug("Position toggle key (%s) pressed", VirtualKeyToString(positionToggleKey));
            Mod::Instance().TogglePosition();
        } else if (!positionTogglePressed && g_positionToggleKeyDown.load()) {
            g_positionToggleKeyDown.store(false);
        }

        Sleep(16);  // ~60Hz polling rate
    }

    Logger::Instance().Debug("Input polling thread stopped");
}

bool InstallInputHook() {
    if (g_running.load()) {
        return true;
    }

    // Get hotkeys from config
    const Config& config = Mod::Instance().GetConfig();
    g_toggleKey.store(config.toggleKey);
    g_recenterKey.store(config.recenterKey);
    g_positionToggleKey.store(config.positionToggleKey);

    // Reset key states
    g_toggleKeyDown.store(false);
    g_recenterKeyDown.store(false);
    g_positionToggleKeyDown.store(false);

    // Start polling thread
    g_stopFlag.store(false);
    g_running.store(true);
    g_inputThread = std::thread(InputPollingThread);

    Logger::Instance().Info("Input hook installed - Toggle: %s, Recenter: %s",
        VirtualKeyToString(g_toggleKey.load()), VirtualKeyToString(g_recenterKey.load()));

    return true;
}

void RemoveInputHook() {
    if (!g_running.load()) {
        return;
    }

    g_stopFlag.store(true);

    if (g_inputThread.joinable()) {
        g_inputThread.join();
    }

    g_running.store(false);
    Logger::Instance().Info("Input hook removed");
}

void SetHotkeys(int toggleKey, int recenterKey) {
    if (IsValidHotkeyCode(toggleKey)) {
        g_toggleKey.store(toggleKey);
    }
    if (IsValidHotkeyCode(recenterKey)) {
        g_recenterKey.store(recenterKey);
    }

    Logger::Instance().Info("Hotkeys updated - Toggle: %s, Recenter: %s",
        VirtualKeyToString(g_toggleKey.load()), VirtualKeyToString(g_recenterKey.load()));
}

} // namespace DL2HT
