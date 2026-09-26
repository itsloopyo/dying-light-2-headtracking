#include "pch.h"
#include "config.h"

#include "legacy_config/legacy_config.h"

#include <cameraunlock/config/head_tracking_config_table.h>
#include <cameraunlock/config/value_codecs.h>
#include <cameraunlock/input/key_bindings.h>

#include <cstdio>
#include <stdexcept>
#include <utility>
#include <vector>

namespace DL2HT {

namespace {

namespace cfg = cameraunlock::config;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// A legacy hotkey code and the Ctrl+Shift chord the builds always registered beside it, as one
// key list.
std::string KeyList(int vk, char letter, const char* key, std::vector<cfg::DroppedValue>& dropped) {
    const std::string code = cfg::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    const std::string chord =
        cameraunlock::input::FormatKeyBindings({KeyBinding{KeyModifiers::kCtrl | KeyModifiers::kShift, letter}});
    return code.empty() ? chord : code + ", " + chord;
}

cfg::ImportResult Import(const cfg::LegacyInput& input, Config& out) {
    legacy::Config c;
    const legacy::ReadStatus status = legacy::Read(input.ansi_path.c_str(), c);
    if (status == legacy::ReadStatus::OpenFailed) {
        return cfg::ImportResult::Refused(
            "the file could not be opened, so the mod runs on its default settings this session, "
            "as the last version did");
    }
    // Without a file the build ran on its defaults with world-locked yaw, the file it then wrote.
    if (status == legacy::ReadStatus::Absent) c.worldLockedYaw = true;

    const Config defaults;
    std::vector<cfg::DroppedValue> dropped;
    std::vector<cfg::PoseShapingValue> shaping;

    // The reader keeps the port at 1024 or above and both smoothing values finite and inside
    // [0, 1], so these carry over as they are.
    out.udp_port = c.udpPort;
    out.enable_on_startup = c.autoEnable;
    out.world_space_yaw = c.worldLockedYaw;
    out.local_smoothing = c.localSmoothing;
    out.position.local_smoothing = c.localSmoothing;
    out.remote_smoothing = c.remoteSmoothing;
    out.position.remote_smoothing = c.remoteSmoothing;

    // [Position] Enabled chose only the mode the session started in: the cycle key reached every
    // mode either way.
    const cameraunlock::TrackingModeChannels mode = cameraunlock::EncodeTrackingMode(
        c.positionEnabled ? cameraunlock::TrackingMode::RotationAndPosition
                          : cameraunlock::TrackingMode::RotationOnly);
    out.rotation_enabled = mode.rotation_enabled;
    out.position_enabled = mode.position_enabled;

    // The reader clamps each limit to [0.01, 2.0], which lets a NaN through; N2 takes that to the
    // default. LimitY bounded both directions, so it becomes both explicit values.
    const auto finite = [&dropped](float value, float row_default, const char* key) {
        return cfg::LegacyFiniteOrDefault(value, row_default, "Position", key, dropped);
    };
    out.position.limit_x = finite(c.positionLimitX, defaults.position.limit_x, "LimitX");
    out.position.limit_y = finite(c.positionLimitY, defaults.position.limit_y, "LimitY");
    out.position.limit_y_down = out.position.limit_y;
    out.position.limit_z = finite(c.positionLimitZ, defaults.position.limit_z, "LimitZ");
    out.position.limit_z_back = finite(c.positionLimitZBack, defaults.position.limit_z_back, "LimitZBack");

    // Every rotation sensitivity shipped at 1.0 and every inversion off, which is identity. The
    // position sensitivity shipped at 2.0 on each axis, which Mod::Initialize now applies as a
    // constant. A value the player changed from those is dropped.
    const auto shape = [&](auto value, auto shipped, const char* section, const char* key) {
        cfg::LegacyPoseShaping(value, shipped, section, key, shaping, dropped);
    };
    shape(c.yawMultiplier, legacy::kDefaultMultiplier, "Sensitivity", "YawMultiplier");
    shape(c.pitchMultiplier, legacy::kDefaultMultiplier, "Sensitivity", "PitchMultiplier");
    shape(c.rollMultiplier, legacy::kDefaultMultiplier, "Sensitivity", "RollMultiplier");
    shape(c.positionSensitivityX, legacy::kDefaultPositionSensitivity, "Position", "SensitivityX");
    shape(c.positionSensitivityY, legacy::kDefaultPositionSensitivity, "Position", "SensitivityY");
    shape(c.positionSensitivityZ, legacy::kDefaultPositionSensitivity, "Position", "SensitivityZ");
    shape(c.positionInvertX, legacy::kDefaultPositionInvert, "Position", "InvertX");
    shape(c.positionInvertY, legacy::kDefaultPositionInvert, "Position", "InvertY");
    shape(c.positionInvertZ, legacy::kDefaultPositionInvert, "Position", "InvertZ");

    // The mod's aim dot is drawn whenever head tracking is on, with no switch and no key. A file
    // that switched it off loses that switch, and the reticle key the build registered, a code
    // GetAsyncKeyState can report, is gone with its Ctrl+Shift+U.
    if (!c.reticleEnabled) dropped.push_back({cfg::DropRule::Reticle, "Reticle", "Enabled", "false"});
    if (c.reticleToggleKey >= 0x01 && c.reticleToggleKey <= 0xFF) {
        char code[8];
        std::snprintf(code, sizeof(code), "0x%02X", static_cast<unsigned>(c.reticleToggleKey));
        dropped.push_back({cfg::DropRule::Reticle, "Hotkeys", "ReticleToggleKey", code});
    }

    out.toggle_key_name = KeyList(c.toggleKey, 'Y', "ToggleKey", dropped);
    out.cycle_tracking_mode_key_name = KeyList(c.trackingModeKey, 'G', "TrackingModeKey", dropped);
    out.yaw_mode_key_name = KeyList(c.yawModeKey, 'H', "YawModeKey", dropped);

    out.show_notifications = c.showNotifications;

    return status == legacy::ReadStatus::Absent ? cfg::ImportResult::Absent(std::move(dropped), std::move(shaping))
                                                : cfg::ImportResult::Imported(std::move(dropped), std::move(shaping));
}

} // namespace

cfg::ConfigTable<Config> MakeConfigTable() {
    using C = cfg::schema::Concept;
    cfg::ConfigTable<Config> table = cfg::HeadTrackingConfigTable<Config>(
        {C::UdpPort, C::EnableOnStartup, C::WorldSpaceYaw, C::RotationEnabled, C::LocalSmoothing,
         C::RemoteSmoothing, C::PositionEnabled, C::PositionLimitX, C::PositionLimitY, C::PositionLimitYDown,
         C::PositionLimitZ, C::PositionLimitZBack, C::ToggleKey, C::CycleTrackingModeKey, C::YawModeKey});
    table.Select(C::WorldSpaceYaw).Writable()
        .Select(C::RotationEnabled).Writable()
        .Select(C::PositionEnabled).Writable();
    table.Local("General", "ShowNotifications", &Config::show_notifications, cfg::BoolCodec(),
                "true: write the mod's notices (tracking on or off, a mode change) to HeadTracking.log.");
    return table;
}

cfg::LegacyImport<Config> MakeLegacyImport() {
    return {&Import, legacy::ReadKeys()};
}

cfg::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder, cfg::DefaultsFile defaults) {
    cfg::ConfigOwnerOptions<Config> options;
    options.path = folder + kConfigFileName;
    options.table = MakeConfigTable();
    options.import = MakeLegacyImport();
    options.legacy_path = folder + kLegacyFileName;
    options.header.display_name = kConfigDisplayName;
    options.defaults = std::move(defaults);
    return options;
}

cameraunlock::TrackingMode StartupTrackingMode(const Config& config) {
    const auto mode = cameraunlock::DecodeTrackingMode(config.rotation_enabled, config.position_enabled);
    if (!mode) throw std::logic_error("RotationEnabled and PositionEnabled are both false, which the table never gives");
    return *mode;
}

} // namespace DL2HT
