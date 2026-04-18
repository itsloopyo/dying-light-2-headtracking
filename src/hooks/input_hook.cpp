#include "pch.h"
#include "input_hook.h"
#include "core/mod.h"
#include "core/logger.h"
#include "core/hotkey_utils.h"

namespace DL2HT {

// Ctrl+Shift+<letter> chord fallbacks (CLAUDE.md cluster: T/Y/U/G/H/J).
// Mapping kept in sync with README and with the CLAUDE.md hotkey spec.
static constexpr int CHORD_RECENTER_VK = 0x54;         // T
static constexpr int CHORD_TOGGLE_VK = 0x59;           // Y
static constexpr int CHORD_POSITION_TOGGLE_VK = 0x47;  // G
static constexpr int CHORD_YAW_MODE_VK = 0x48;         // H
static constexpr int CHORD_RETICLE_TOGGLE_VK = 0x55;   // U

static std::thread g_inputThread;
static std::atomic<bool> g_stopFlag{false};
static std::atomic<bool> g_running{false};

static std::atomic<int> g_toggleKey{DEFAULT_TOGGLE_KEY};
static std::atomic<int> g_recenterKey{DEFAULT_RECENTER_KEY};
static std::atomic<int> g_positionToggleKey{DEFAULT_POSITION_TOGGLE_KEY};
static std::atomic<int> g_yawModeKey{DEFAULT_YAW_MODE_KEY};
static std::atomic<int> g_reticleToggleKey{DEFAULT_RETICLE_TOGGLE_KEY};

// Primary-key rising-edge state
static std::atomic<bool> g_toggleKeyDown{false};
static std::atomic<bool> g_recenterKeyDown{false};
static std::atomic<bool> g_positionToggleKeyDown{false};
static std::atomic<bool> g_yawModeKeyDown{false};
static std::atomic<bool> g_reticleToggleKeyDown{false};

// Chord rising-edge state (tracked separately so chord and primary don't
// interfere with each other's debouncing)
static std::atomic<bool> g_toggleChordDown{false};
static std::atomic<bool> g_recenterChordDown{false};
static std::atomic<bool> g_positionToggleChordDown{false};
static std::atomic<bool> g_yawModeChordDown{false};
static std::atomic<bool> g_reticleToggleChordDown{false};

static void InputPollingThread() {
    Logger::Instance().Debug("Input polling thread started");

    while (!g_stopFlag.load()) {
        // Modifier snapshot for chord detection. Primary-key fires are gated
        // by !chordActive so Ctrl+Shift+End doesn't also fire the End-only
        // toggle path.
        bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool chordActive = ctrl && shift;

        int toggleKey = g_toggleKey.load();
        if (DetectEdge(toggleKey, g_toggleKeyDown) && !chordActive) {
            Logger::Instance().Debug("Toggle key (%s) pressed", VirtualKeyToString(toggleKey));
            Mod::Instance().Toggle();
        }
        if (DetectChordEdge(CHORD_TOGGLE_VK, g_toggleChordDown, chordActive)) {
            Logger::Instance().Debug("Toggle chord (Ctrl+Shift+Y) pressed");
            Mod::Instance().Toggle();
        }

        int recenterKey = g_recenterKey.load();
        if (DetectEdge(recenterKey, g_recenterKeyDown) && !chordActive) {
            Logger::Instance().Debug("Recenter key (%s) pressed", VirtualKeyToString(recenterKey));
            Mod::Instance().Recenter();
        }
        if (DetectChordEdge(CHORD_RECENTER_VK, g_recenterChordDown, chordActive)) {
            Logger::Instance().Debug("Recenter chord (Ctrl+Shift+T) pressed");
            Mod::Instance().Recenter();
        }

        int positionToggleKey = g_positionToggleKey.load();
        if (DetectEdge(positionToggleKey, g_positionToggleKeyDown) && !chordActive) {
            Logger::Instance().Debug("Position toggle key (%s) pressed", VirtualKeyToString(positionToggleKey));
            Mod::Instance().TogglePosition();
        }
        if (DetectChordEdge(CHORD_POSITION_TOGGLE_VK, g_positionToggleChordDown, chordActive)) {
            Logger::Instance().Debug("Position toggle chord (Ctrl+Shift+G) pressed");
            Mod::Instance().TogglePosition();
        }

        int yawModeKey = g_yawModeKey.load();
        if (DetectEdge(yawModeKey, g_yawModeKeyDown) && !chordActive) {
            Logger::Instance().Debug("Yaw mode key (%s) pressed", VirtualKeyToString(yawModeKey));
            Mod::Instance().ToggleYawMode();
        }
        if (DetectChordEdge(CHORD_YAW_MODE_VK, g_yawModeChordDown, chordActive)) {
            Logger::Instance().Debug("Yaw mode chord (Ctrl+Shift+H) pressed");
            Mod::Instance().ToggleYawMode();
        }

        int reticleToggleKey = g_reticleToggleKey.load();
        if (DetectEdge(reticleToggleKey, g_reticleToggleKeyDown) && !chordActive) {
            Logger::Instance().Debug("Reticle toggle key (%s) pressed", VirtualKeyToString(reticleToggleKey));
            Mod::Instance().ToggleReticle();
        }
        if (DetectChordEdge(CHORD_RETICLE_TOGGLE_VK, g_reticleToggleChordDown, chordActive)) {
            Logger::Instance().Debug("Reticle toggle chord (Ctrl+Shift+U) pressed");
            Mod::Instance().ToggleReticle();
        }

        Sleep(16);  // ~60Hz polling rate
    }

    Logger::Instance().Debug("Input polling thread stopped");
}

bool InstallInputHook() {
    if (g_running.load()) {
        return true;
    }

    const Config& config = Mod::Instance().GetConfig();
    g_toggleKey.store(config.toggleKey);
    g_recenterKey.store(config.recenterKey);
    g_positionToggleKey.store(config.positionToggleKey);
    g_yawModeKey.store(config.yawModeKey);
    g_reticleToggleKey.store(config.reticleToggleKey);

    g_toggleKeyDown.store(false);
    g_recenterKeyDown.store(false);
    g_positionToggleKeyDown.store(false);
    g_yawModeKeyDown.store(false);
    g_reticleToggleKeyDown.store(false);

    g_toggleChordDown.store(false);
    g_recenterChordDown.store(false);
    g_positionToggleChordDown.store(false);
    g_yawModeChordDown.store(false);
    g_reticleToggleChordDown.store(false);

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
