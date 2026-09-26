// The config differential test. Every input is read three ways:
//
//   oracle     v1.4.0's reader (oracle/), the newest published build, and its startup code
//   import     the frozen reader in src/legacy_config/, and the startup code it ran under
//   migration  the config owner on a folder holding the input as HeadTracking.ini: the import
//              into a new CameraUnlock.ini, then the canonical reader and table on that file,
//              and the startup code of this build. Every owner reads a scratch Defaults.ini,
//              never the developer's own
//
// Comparison 1, oracle against import, finds what a player updating from v1.4.0 sees change
// that the conversion did not cause. Every difference it may find is listed in
// kComparisonOneDifferences with the commit that made it, and applied to v1.4.0's reading
// before the two are compared; any other difference fails the test.
//
// Comparison 2, import against migration, is the proof for the migration: no difference but
// the approved ones, each of which the import must record. A rotation sensitivity away from its
// shipped 1.0, a position sensitivity away from its shipped 2.0 and a position inversion that is
// on are dropped (pose_shaping); the shipped values are what the mod now applies. [Reticle]
// Enabled=false and the reticle key are dropped (reticle): the aim dot is drawn whenever head
// tracking is on, and neither the key nor Ctrl+Shift+U toggles it. A hotkey code outside
// 0x01-0xFE imports as unbound (N1), and a position limit that is not a number as its default
// (N2). No default moved, so the no-file input has no difference either, and the reader keeps
// every other value inside a range the canonical rows hold, so no input is deferred.
//
// Each input migrates three times: over a Defaults.ini at the built-in values, from a read-only
// HeadTracking.ini, and over a Defaults.ini that differs from the built-in values on every global
// row. All three give the same settings, since the migration writes default only where the
// imported value is what default gives. Every load leaves HeadTracking.ini's bytes, last write
// time and attributes as they were, the folder holds HeadTracking.ini and CameraUnlock.ini and
// nothing else, and the next start reads CameraUnlock.ini, imports nothing and writes nothing.
// The distinct migrated files are written beside the executable under migrated\, for
// lint-migrated.mjs to run core's canonical config lint over.
//
// Inputs: no file, an empty file, every HeadTracking.ini a release shipped (installer ZIP, Nexus
// ZIP and launcher seed carry the same bytes in each release) and every other version of the file
// committed up to v1.4.0, the file each published build wrote at first launch when there was none
// (extracted once into inputs/ from each build's own config code), v1.4.0's file with a value
// continued on an indented line, core's corpus over v1.4.0's file, and v1.4.0's file with all four
// hotkeys on each code from 0x01 to 0xFE. There is no dev pre-release.

#include "pch.h"
#include "core/config.h"
#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/config/testing/ini_mutations.h"
#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

namespace legacy = DL2HT::legacy;
namespace cfg = cameraunlock::config;
using DL2HT::Config;
using cameraunlock::TrackingMode;
using cameraunlock::config::testing::GenerateIniMutations;
using cameraunlock::config::testing::IniMutation;
using cameraunlock::config::testing::MutationKey;
using cameraunlock::input::KeyModifiers;

int g_failures = 0;

void Fail(const std::string& input, const std::string& what) {
    if (g_failures < 50) std::printf("FAIL [%s]: %s\n", input.c_str(), what.c_str());
    ++g_failures;
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

std::wstring Widen(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

std::string Narrow(const std::wstring& path) {
    const int size = WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), 'x');
    WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, out.data(), size, nullptr, nullptr);
    out.resize(static_cast<size_t>(size) - 1);
    return out;
}

std::string ReadBytes(const std::wstring& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + Narrow(path));
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const std::wstring& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + Narrow(path));
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// Every file in the folder, name and bytes, for "the import changed nothing".
std::map<std::wstring, std::string> Snapshot(const std::wstring& dir) {
    std::map<std::wstring, std::string> files;
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot list the test folder");
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        files[data.cFileName] = ReadBytes(dir + L"\\" + data.cFileName);
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return files;
}

void EmptyFolder(const std::wstring& dir) {
    for (const auto& [name, bytes] : Snapshot(dir)) {
        const std::wstring path = dir + L"\\" + name;
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (!DeleteFileW(path.c_str())) throw std::runtime_error("cannot empty the test folder");
    }
}

std::wstring MakeFolder(const std::wstring& parent, const wchar_t* name) {
    const std::wstring dir = parent + L"\\" + name;
    if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        throw std::runtime_error("cannot create the test folder");
    }
    EmptyFolder(dir);
    return dir;
}

// The names of the files in the folder.
std::set<std::wstring> Names(const std::wstring& dir) {
    std::set<std::wstring> names;
    for (const auto& [name, bytes] : Snapshot(dir)) names.insert(name);
    return names;
}

// A file's bytes, last write time and attributes, which no load may change.
struct FileState {
    std::string bytes;
    uint64_t written = 0;
    DWORD attributes = 0;

    bool operator==(const FileState& o) const {
        return bytes == o.bytes && written == o.written && attributes == o.attributes;
    }
    bool operator!=(const FileState& o) const { return !(*this == o); }
};

std::optional<FileState> StateOf(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return std::nullopt;
        throw std::runtime_error("cannot read the attributes of " + Narrow(path));
    }
    FileState state;
    state.bytes = ReadBytes(path);
    state.written = (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                    data.ftLastWriteTime.dwLowDateTime;
    state.attributes = data.dwFileAttributes;
    return state;
}

bool LogSays(const std::vector<std::string>& log, const std::string& text) {
    return std::any_of(log.begin(), log.end(),
                       [&text](const std::string& line) { return line.find(text) != std::string::npos; });
}

const wchar_t kIniName[] = L"HeadTracking.ini";
const wchar_t kCanonicalName[] = L"CameraUnlock.ini";

// ---------------------------------------------------------------------------
// What a reading does
// ---------------------------------------------------------------------------

enum class Action { Toggle, CycleMode, YawMode, Reticle };

const char* ActionName(Action a) {
    switch (a) {
        case Action::Toggle: return "toggle";
        case Action::CycleMode: return "cycle mode";
        case Action::YawMode: return "yaw mode";
        case Action::Reticle: return "reticle";
    }
    throw std::logic_error("action");
}

// One key the poller watches for an action, and the modifiers it fires with: kNav is NavGuarded
// (not while Ctrl and Shift are both held), kChord is ChordGuarded (while both are held).
struct Registration {
    Action action;
    int vk;
    unsigned modifiers;

    bool operator<(const Registration& o) const {
        return std::tie(action, vk, modifiers) < std::tie(o.action, o.vk, o.modifiers);
    }
    bool operator==(const Registration& o) const {
        return action == o.action && vk == o.vk && modifiers == o.modifiers;
    }
};

constexpr unsigned kNav = static_cast<unsigned>(KeyModifiers::kNone);
constexpr unsigned kChord = static_cast<unsigned>(KeyModifiers::kCtrl | KeyModifiers::kShift);

// RegisterHotkeys in src/hooks/input_hook.cpp, the same at v1.4.0 and at the commit the reader
// was frozen from: each code NavGuarded, and each action's Ctrl+Shift letter ChordGuarded.
std::vector<Registration> PublishedHotkeys(int toggle, int mode, int yaw, int reticle) {
    return {
        {Action::Toggle, toggle, kNav},   {Action::Toggle, 'Y', kChord},  {Action::CycleMode, mode, kNav},
        {Action::CycleMode, 'G', kChord}, {Action::YawMode, yaw, kNav},   {Action::YawMode, 'H', kChord},
        {Action::Reticle, reticle, kNav}, {Action::Reticle, 'U', kChord},
    };
}

uint32_t Bits(float f) {
    uint32_t b;
    std::memcpy(&b, &f, sizeof(b));
    return b;
}

// Everything the running mod acts on after reading the file: the processor settings, the state
// the session starts in and the bindings that can fire.
struct Reading {
    int port = 0;
    bool enabled = false;
    bool world_yaw = false;
    TrackingMode mode = TrackingMode::RotationAndPosition;
    float yaw_sens = 0, pitch_sens = 0, roll_sens = 0;
    float pos_sens_x = 0, pos_sens_y = 0, pos_sens_z = 0;
    float limit_x = 0, limit_y = 0, limit_y_down = 0, limit_z = 0, limit_z_back = 0;
    bool invert_x = false, invert_y = false, invert_z = false;
    float local_smoothing = 0, remote_smoothing = 0;
    bool reticle_shown = false;
    bool show_notifications = false;
    std::vector<Registration> hotkeys;
};

using Record = std::map<std::string, std::string>;

std::string Hex(uint32_t bits) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", static_cast<unsigned>(bits));
    return text;
}

std::string Flag(bool value) { return value ? "1" : "0"; }

// The bindings that can fire. The poller skips code 0, and GetAsyncKeyState reports no code
// above 0xFF or below 0 down; 0xFF it can.
void AddHotkeys(Record& r, const std::vector<Registration>& registrations) {
    std::map<Action, std::vector<std::pair<unsigned, int>>> byAction;
    for (Action a : {Action::Toggle, Action::CycleMode, Action::YawMode, Action::Reticle}) byAction[a];
    for (const Registration& reg : registrations) {
        if (reg.vk < 0x01 || reg.vk > 0xFF) continue;
        byAction[reg.action].push_back({reg.modifiers, reg.vk});
    }
    for (auto& [action, items] : byAction) {
        std::sort(items.begin(), items.end());
        items.erase(std::unique(items.begin(), items.end()), items.end());
        std::string text;
        for (const auto& [modifiers, vk] : items) {
            char item[32];
            std::snprintf(item, sizeof(item), "%s%u:0x%02X", text.empty() ? "" : " ", modifiers,
                          static_cast<unsigned>(vk));
            text += item;
        }
        r[std::string("hotkey.") + ActionName(action)] = text;
    }
}

Record Observe(const Reading& g) {
    Record r;
    r["port"] = std::to_string(g.port);
    r["start.enabled"] = Flag(g.enabled);
    r["start.world_yaw"] = Flag(g.world_yaw);
    r["start.mode"] = g.mode == TrackingMode::RotationAndPosition ? "RotationAndPosition"
                      : g.mode == TrackingMode::RotationOnly          ? "RotationOnly"
                                                                      : "PositionOnly";
    r["rot.yaw_sensitivity"] = Hex(Bits(g.yaw_sens));
    r["rot.pitch_sensitivity"] = Hex(Bits(g.pitch_sens));
    r["rot.roll_sensitivity"] = Hex(Bits(g.roll_sens));
    r["pos.sensitivity_x"] = Hex(Bits(g.pos_sens_x));
    r["pos.sensitivity_y"] = Hex(Bits(g.pos_sens_y));
    r["pos.sensitivity_z"] = Hex(Bits(g.pos_sens_z));
    r["pos.limit_x"] = Hex(Bits(g.limit_x));
    r["pos.limit_y"] = Hex(Bits(g.limit_y));
    r["pos.limit_y_down"] = Hex(Bits(g.limit_y_down));
    r["pos.limit_z"] = Hex(Bits(g.limit_z));
    r["pos.limit_z_back"] = Hex(Bits(g.limit_z_back));
    r["pos.invert_x"] = Flag(g.invert_x);
    r["pos.invert_y"] = Flag(g.invert_y);
    r["pos.invert_z"] = Flag(g.invert_z);
    r["smoothing.local"] = Hex(Bits(g.local_smoothing));
    r["smoothing.remote"] = Hex(Bits(g.remote_smoothing));
    r["start.reticle_shown"] = Flag(g.reticle_shown);
    r["notifications"] = Flag(g.show_notifications);
    AddHotkeys(r, g.hotkeys);
    return r;
}

std::vector<std::string> Differences(const Record& a, const Record& b) {
    std::vector<std::string> out;
    for (const auto& [name, value] : a) {
        const auto it = b.find(name);
        if (it == b.end()) {
            out.push_back(name + " only on the left");
        } else if (it->second != value) {
            out.push_back(name + ": " + value + " / " + it->second);
        }
    }
    for (const auto& [name, value] : b) {
        if (a.find(name) == a.end()) out.push_back(name + " only on the right");
    }
    return out;
}

// v1.4.0's Mod::Initialize (src/core/mod.cpp at 3421cad) on its Config: the processor's
// sensitivities, the position settings with limit_y_down left at the 0.20 core's
// PositionSettings held at v1.4.0's pin (3465659), the mode from [Position] Enabled, and the
// hotkeys.
Reading FromOracle(const oracle_api::Config& c) {
    Reading g;
    g.port = c.udpPort;
    g.enabled = c.autoEnable;
    g.world_yaw = c.worldLockedYaw;
    g.mode = c.positionEnabled ? TrackingMode::RotationAndPosition : TrackingMode::RotationOnly;
    g.yaw_sens = c.yawMultiplier;
    g.pitch_sens = c.pitchMultiplier;
    g.roll_sens = c.rollMultiplier;
    g.pos_sens_x = c.positionSensitivityX;
    g.pos_sens_y = c.positionSensitivityY;
    g.pos_sens_z = c.positionSensitivityZ;
    g.limit_x = c.positionLimitX;
    g.limit_y = c.positionLimitY;
    g.limit_y_down = 0.20f;
    g.limit_z = c.positionLimitZ;
    g.limit_z_back = c.positionLimitZBack;
    g.invert_x = c.positionInvertX;
    g.invert_y = c.positionInvertY;
    g.invert_z = c.positionInvertZ;
    g.local_smoothing = c.localSmoothing;
    g.remote_smoothing = c.remoteSmoothing;
    g.reticle_shown = c.reticleEnabled;
    g.show_notifications = c.showNotifications;
    g.hotkeys = PublishedHotkeys(c.toggleKey, c.trackingModeKey, c.yawModeKey, c.reticleToggleKey);
    return g;
}

// The frozen reader through the startup code of the commit that froze it: Mod::LoadConfig runs
// on the frozen defaults with WorldLockedYaw on when the file was not read, and Mod::Initialize
// puts LimitY on both vertical bounds.
Reading FromImport(legacy::ReadStatus status, legacy::Config c) {
    if (status != legacy::ReadStatus::Read) {
        c = legacy::Config{};
        c.worldLockedYaw = true;
    }
    Reading g;
    g.port = c.udpPort;
    g.enabled = c.autoEnable;
    g.world_yaw = c.worldLockedYaw;
    g.mode = c.positionEnabled ? TrackingMode::RotationAndPosition : TrackingMode::RotationOnly;
    g.yaw_sens = c.yawMultiplier;
    g.pitch_sens = c.pitchMultiplier;
    g.roll_sens = c.rollMultiplier;
    g.pos_sens_x = c.positionSensitivityX;
    g.pos_sens_y = c.positionSensitivityY;
    g.pos_sens_z = c.positionSensitivityZ;
    g.limit_x = c.positionLimitX;
    g.limit_y = c.positionLimitY;
    g.limit_y_down = c.positionLimitY;
    g.limit_z = c.positionLimitZ;
    g.limit_z_back = c.positionLimitZBack;
    g.invert_x = c.positionInvertX;
    g.invert_y = c.positionInvertY;
    g.invert_z = c.positionInvertZ;
    g.local_smoothing = c.localSmoothing;
    g.remote_smoothing = c.remoteSmoothing;
    g.reticle_shown = c.reticleEnabled;
    g.show_notifications = c.showNotifications;
    g.hotkeys = PublishedHotkeys(c.toggleKey, c.trackingModeKey, c.yawModeKey, c.reticleToggleKey);
    return g;
}

// ---------------------------------------------------------------------------
// Comparison 1: v1.4.0 against the frozen reader
// ---------------------------------------------------------------------------

// What a player updating from v1.4.0 sees change, and the commit that made each change. The
// changelog carries the same list.
struct ListedDifference {
    const char* id;
    const char* commit;
    const char* what;
    int seen = 0;
};

ListedDifference kComparisonOneDifferences[] = {
    {"downward-limit", "948add0",
     "[Position] LimitY bounds leaning down as well as up. v1.4.0 kept the downward lean at 0.20 m "
     "whatever LimitY said"},
    {"first-launch-yaw", "0307ede",
     "a start that finds no HeadTracking.ini, or cannot open it, runs world-locked yaw and writes "
     "WorldLockedYaw=true. v1.4.0 ran camera-local and wrote false. A file without the key still "
     "reads as camera-local (6155436)"},
    {"continued-value", "b993f62",
     "inih, which reads HeadTracking.ini, was replaced by the r58 release its notices name. A value "
     "continued on an indented line ends at an inline ';' comment there; v1.4.0's copy kept the "
     "comment in the value, so a continued true followed by a comment read as false"},
};

ListedDifference& Listed(const char* id) {
    for (ListedDifference& d : kComparisonOneDifferences) {
        if (std::strcmp(d.id, id) == 0) return d;
    }
    throw std::logic_error(id);
}

// One input and what comparison 1 needs to know about it.
struct Input {
    std::string name;
    std::optional<std::string> bytes;
    // The one input whose AutoEnable is continued on an indented line with a comment after it.
    bool continued_auto_enable = false;
};

// v1.4.0's reading with the listed changes applied, each counted where it changes something.
Reading SincePublished(Reading g, oracle_api::LoadStatus status, const Input& input) {
    if (Bits(g.limit_y_down) != Bits(g.limit_y)) {
        ++Listed("downward-limit").seen;
        g.limit_y_down = g.limit_y;
    }
    if (status != oracle_api::LoadStatus::Read && !g.world_yaw) {
        ++Listed("first-launch-yaw").seen;
        g.world_yaw = true;
    }
    if (input.continued_auto_enable && !g.enabled) {
        ++Listed("continued-value").seen;
        g.enabled = true;
    }
    return g;
}

bool SameStatus(oracle_api::LoadStatus o, legacy::ReadStatus i) {
    switch (o) {
        case oracle_api::LoadStatus::Read: return i == legacy::ReadStatus::Read;
        case oracle_api::LoadStatus::Created: return i == legacy::ReadStatus::Absent;
        case oracle_api::LoadStatus::OpenFailed: return i == legacy::ReadStatus::OpenFailed;
    }
    throw std::logic_error("status");
}

struct OracleRun {
    oracle_api::LoadStatus status = oracle_api::LoadStatus::Read;
    oracle_api::Config cfg;
};

struct ImportRun {
    legacy::ReadStatus status = legacy::ReadStatus::Read;
    legacy::Config cfg;
};

void CompareOracleWithImport(const Input& input, const OracleRun& o, const ImportRun& i) {
    if (!SameStatus(o.status, i.status)) {
        Fail(input.name, "comparison 1: v1.4.0 and the import do not agree on whether the file was read");
        return;
    }
    const Record want = Observe(SincePublished(FromOracle(o.cfg), o.status, input));
    for (const std::string& d : Differences(want, Observe(FromImport(i.status, i.cfg)))) {
        Fail(input.name, "comparison 1: " + d + " (v1.4.0 with the listed changes / the import)");
    }
}

// ---------------------------------------------------------------------------
// The keys the frozen reader reads, and the corpus descriptors
// ---------------------------------------------------------------------------

// The reader clamps the sensitivities and limits, sanitises the smoothing pair and refuses a
// port below 1024; each out-of-range value is one beyond such a bound. A hotkey takes any
// number strtol reads.
std::vector<MutationKey> CorpusKeys() {
    const auto boolean = [](const char* s, const char* k, const char* alternate) {
        return MutationKey{s, k, alternate, {}, false, {}};
    };
    const auto multiplier = [](const char* k, std::vector<std::string> out) {
        return MutationKey{"Sensitivity", k, "0.5", std::move(out), false, {}};
    };
    const auto sens = [](const char* k) { return MutationKey{"Position", k, "1.5", {"0.05", "12"}, false, {}}; };
    const auto limit = [](const char* k) { return MutationKey{"Position", k, "0.25", {"0.001", "2.5"}, false, {}}; };
    const auto smooth = [](const char* k) { return MutationKey{"Smoothing", k, "0.3", {"-0.5", "1.5"}, false, {}}; };
    const auto hotkey = [](const char* k, const char* alt) {
        return MutationKey{"Hotkeys", k, alt, {"0xFF", "0x100", "-1"}, true, {}};
    };
    return {
        MutationKey{"Network", "UDPPort", "5000", {"80", "70000"}, false, {}},
        multiplier("YawMultiplier", {"0.05", "6"}),
        multiplier("PitchMultiplier", {"0.05", "6"}),
        multiplier("RollMultiplier", {"-0.5", "3"}),
        hotkey("ToggleKey", "0x70"),
        hotkey("TrackingModeKey", "0x71"),
        hotkey("PositionToggleKey", "0x73"),
        hotkey("YawModeKey", "0x72"),
        hotkey("ReticleToggleKey", "0x74"),
        boolean("Rotation", "WorldLockedYaw", "true"),
        sens("SensitivityX"),
        sens("SensitivityY"),
        sens("SensitivityZ"),
        limit("LimitX"),
        limit("LimitY"),
        limit("LimitZ"),
        limit("LimitZBack"),
        boolean("Position", "InvertX", "true"),
        boolean("Position", "InvertY", "true"),
        boolean("Position", "InvertZ", "true"),
        boolean("Position", "Enabled", "false"),
        smooth("LocalSmoothing"),
        smooth("RemoteSmoothing"),
        boolean("Reticle", "Enabled", "false"),
        boolean("General", "AutoEnable", "false"),
        boolean("General", "ShowNotifications", "false"),
    };
}

// ---------------------------------------------------------------------------
// Comparison 2: the frozen reader against the migration
// ---------------------------------------------------------------------------

// This build: Mod::Initialize, which runs the processor with no rotation sensitivity, the
// position sensitivity kPositionSensitivity on every axis and no inversion, the limits and
// smoothing from the file, the mode from the pair and the aim dot on whenever tracking is; and
// the lists RegisterHotkeys registers.
Reading FromMigration(const Config& c) {
    Reading g;
    g.port = c.udp_port;
    g.enabled = c.enable_on_startup;
    g.world_yaw = c.world_space_yaw;
    g.mode = DL2HT::StartupTrackingMode(c);
    g.yaw_sens = g.pitch_sens = g.roll_sens = 1.0f;
    g.pos_sens_x = g.pos_sens_y = g.pos_sens_z = DL2HT::kPositionSensitivity;
    g.limit_x = c.position.limit_x;
    g.limit_y = c.position.limit_y;
    g.limit_y_down = c.position.limit_y_down;
    g.limit_z = c.position.limit_z;
    g.limit_z_back = c.position.limit_z_back;
    g.local_smoothing = c.local_smoothing;
    g.remote_smoothing = c.remote_smoothing;
    g.reticle_shown = true;
    g.show_notifications = c.show_notifications;
    const std::pair<Action, const std::string*> lists[] = {
        {Action::Toggle, &c.toggle_key_name},
        {Action::CycleMode, &c.cycle_tracking_mode_key_name},
        {Action::YawMode, &c.yaw_mode_key_name},
    };
    for (const auto& [action, list] : lists) {
        const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(*list);
        if (!parsed.ok()) throw std::logic_error("a migrated hotkey list does not parse: " + *list);
        for (const cameraunlock::input::KeyBinding& b : parsed.bindings) {
            g.hotkeys.push_back({action, b.vk, static_cast<unsigned>(b.modifiers)});
        }
    }
    return g;
}

// The reticle key and Ctrl+Shift+U are gone for every player, a file or none (reticle): the aim
// dot is drawn whenever head tracking is on.
Reading WithoutReticleToggle(Reading g) {
    g.reticle_shown = true;
    g.hotkeys.erase(std::remove_if(g.hotkeys.begin(), g.hotkeys.end(),
                                   [](const Registration& r) { return r.action == Action::Reticle; }),
                    g.hotkeys.end());
    return g;
}

const cfg::DroppedValue* FindDrop(const std::vector<cfg::DroppedValue>& dropped, cfg::DropRule rule,
                                  const char* section, const char* key) {
    for (const cfg::DroppedValue& d : dropped) {
        if (d.rule == rule && d.section == section && d.key == key) return &d;
    }
    return nullptr;
}

struct DropCounts {
    int pose_shaping = 0;
    int reticle = 0;
    int n1 = 0;
    int n2 = 0;
};

// The import's reading with the approved changes applied, each checked against what the import
// recorded: every sensitivity and inversion listed in its place, and dropped exactly where it is
// not the shipped value; the reticle switch and key dropped exactly where the build had them; N1
// on a code outside 0x01-0xFE and N2 on a limit that is not a number, and nothing else dropped.
Reading Expected(const std::string& name, Reading g, const legacy::Config& c, const cfg::ImportResult& result,
                 DropCounts& counts) {
    struct Shaped {
        const char* section;
        const char* key;
        bool shipped;
    };
    const Shaped shaped[] = {
        {"Sensitivity", "YawMultiplier", Bits(c.yawMultiplier) == Bits(legacy::kDefaultMultiplier)},
        {"Sensitivity", "PitchMultiplier", Bits(c.pitchMultiplier) == Bits(legacy::kDefaultMultiplier)},
        {"Sensitivity", "RollMultiplier", Bits(c.rollMultiplier) == Bits(legacy::kDefaultMultiplier)},
        {"Position", "SensitivityX", Bits(c.positionSensitivityX) == Bits(legacy::kDefaultPositionSensitivity)},
        {"Position", "SensitivityY", Bits(c.positionSensitivityY) == Bits(legacy::kDefaultPositionSensitivity)},
        {"Position", "SensitivityZ", Bits(c.positionSensitivityZ) == Bits(legacy::kDefaultPositionSensitivity)},
        {"Position", "InvertX", c.positionInvertX == legacy::kDefaultPositionInvert},
        {"Position", "InvertY", c.positionInvertY == legacy::kDefaultPositionInvert},
        {"Position", "InvertZ", c.positionInvertZ == legacy::kDefaultPositionInvert},
    };
    if (result.pose_shaping.size() != std::size(shaped)) {
        Fail(name, "the import lists " + std::to_string(result.pose_shaping.size()) + " pose-shaping values, not 9");
    } else {
        bool any = false;
        for (size_t k = 0; k < std::size(shaped); ++k) {
            const cfg::PoseShapingValue& v = result.pose_shaping[k];
            const std::string label = std::string("[") + shaped[k].section + "] " + shaped[k].key;
            if (v.section != shaped[k].section || v.key != shaped[k].key) Fail(name, label + " is not listed in its place");
            if (v.folded != shaped[k].shipped) Fail(name, label + " is " + (v.folded ? "folded" : "dropped") + " wrongly");
            const bool listed =
                FindDrop(result.dropped, cfg::DropRule::PoseShaping, shaped[k].section, shaped[k].key) != nullptr;
            if (listed == shaped[k].shipped) {
                Fail(name, label + (listed ? " is dropped at its shipped value" : " is changed and not dropped"));
            }
            any = any || !shaped[k].shipped;
        }
        if (any) ++counts.pose_shaping;
    }
    g.yaw_sens = g.pitch_sens = g.roll_sens = 1.0f;
    g.pos_sens_x = g.pos_sens_y = g.pos_sens_z = legacy::kDefaultPositionSensitivity;
    g.invert_x = g.invert_y = g.invert_z = false;

    const bool reticleOff = !c.reticleEnabled;
    const bool reticleKeyBound = c.reticleToggleKey >= 0x01 && c.reticleToggleKey <= 0xFF;
    if (reticleOff != (FindDrop(result.dropped, cfg::DropRule::Reticle, "Reticle", "Enabled") != nullptr)) {
        Fail(name, "[Reticle] Enabled is dropped where it did not switch the dot off, or kept where it did");
    }
    if (reticleKeyBound != (FindDrop(result.dropped, cfg::DropRule::Reticle, "Hotkeys", "ReticleToggleKey") != nullptr)) {
        Fail(name, "[Hotkeys] ReticleToggleKey is dropped where no key was bound, or kept where one was");
    }
    if (reticleOff || reticleKeyBound) ++counts.reticle;
    g = WithoutReticleToggle(g);

    struct Hotkey {
        Action action;
        int vk;
        const char* key;
    };
    bool anyN1 = false;
    for (const Hotkey& h : {Hotkey{Action::Toggle, c.toggleKey, "ToggleKey"},
                            Hotkey{Action::CycleMode, c.trackingModeKey, "TrackingModeKey"},
                            Hotkey{Action::YawMode, c.yawModeKey, "YawModeKey"}}) {
        const bool outOfRange = h.vk != 0 && (h.vk < 0x01 || h.vk > 0xFE);
        if (outOfRange != (FindDrop(result.dropped, cfg::DropRule::KeyCodeOutOfRange, "Hotkeys", h.key) != nullptr)) {
            Fail(name, std::string("[Hotkeys] ") + h.key + " dropped as out of range does not match its code");
        }
        if (!outOfRange) continue;
        anyN1 = true;
        g.hotkeys.erase(std::remove_if(g.hotkeys.begin(), g.hotkeys.end(),
                                       [&h](const Registration& r) { return r.action == h.action && r.modifiers == kNav; }),
                        g.hotkeys.end());
    }
    if (anyN1) ++counts.n1;

    const Config defaults;
    struct Limit {
        float* value;
        float fallback;
        const char* key;
    };
    bool anyN2 = false;
    for (const Limit& l : {Limit{&g.limit_x, defaults.position.limit_x, "LimitX"},
                           Limit{&g.limit_y, defaults.position.limit_y, "LimitY"},
                           Limit{&g.limit_z, defaults.position.limit_z, "LimitZ"},
                           Limit{&g.limit_z_back, defaults.position.limit_z_back, "LimitZBack"}}) {
        const bool finite = std::isfinite(*l.value);
        if (finite == (FindDrop(result.dropped, cfg::DropRule::NonFiniteNumber, "Position", l.key) != nullptr)) {
            Fail(name, std::string("[Position] ") + l.key + " dropped as not a number does not match its value");
        }
        if (finite) continue;
        anyN2 = true;
        *l.value = l.fallback;
    }
    if (anyN2) ++counts.n2;
    g.limit_y_down = g.limit_y;

    for (const cfg::DroppedValue& d : result.dropped) {
        const bool approved = d.rule == cfg::DropRule::PoseShaping || d.rule == cfg::DropRule::Reticle ||
                              d.rule == cfg::DropRule::KeyCodeOutOfRange || d.rule == cfg::DropRule::NonFiniteNumber;
        if (!approved) Fail(name, "the import drops [" + d.section + "] " + d.key + " by a rule this map never applies");
    }
    return g;
}

struct MigrationTally {
    std::string committed;
    std::set<std::string> migrated;
    int created = 0;
    int converted = 0;
    DropCounts drops;
};

// Every row's value, whatever Defaults.ini holds, so two Configs compare in full.
std::string Values(const Config& c) {
    return cfg::RenderCanonical(DL2HT::MakeConfigTable(), c, {DL2HT::kConfigDisplayName});
}

const char kSkewedDefaults[] =
    "[CameraUnlock]\r\nConfigFormat=1\r\n\r\n"
    "[Network]\r\nUdpPort=5252\r\n\r\n"
    "[General]\r\nEnableOnStartup=false\r\nWorldSpaceYaw=false\r\nRotationEnabled=false\r\n\r\n"
    "[Smoothing]\r\nLocalSmoothing=0.5\r\nRemoteSmoothing=0.5\r\n\r\n"
    "[Position]\r\nPositionEnabled=true\r\nPositionLimitX=0.5\r\nPositionLimitY=0.45\r\n"
    "PositionLimitYDown=0.35\r\nPositionLimitZ=0.6\r\nPositionLimitZBack=0.25\r\n\r\n"
    "[Hotkeys]\r\nToggleKey=F8\r\nCycleTrackingModeKey=F9\r\nYawModeKey=F10\r\n";

cfg::ConfigOwnerOptions<Config> Options(const std::wstring& folder, const std::wstring& defaults) {
    return DL2HT::MakeConfigOwnerOptions(folder + L"\\", cfg::DefaultsFile::At(defaults));
}

// The owner on `folder`, which holds the input as HeadTracking.ini (or nothing), over the
// Defaults.ini at `defaults`: what a load must do beyond comparison 2, and the settings the
// session runs on. A file it migrates goes into the tally.
std::optional<Config> Migrate(const std::wstring& folder, const std::wstring& defaults, const std::string& label,
                              MigrationTally& tally) {
    using cfg::ConfigLoadStatus;
    const std::wstring legacyPath = folder + L"\\" + kIniName;
    const std::wstring path = folder + L"\\" + kCanonicalName;
    const std::optional<FileState> legacyBefore = StateOf(legacyPath);

    const cfg::ConfigLoadResult<Config> loaded = cfg::ConfigOwner<Config>(Options(folder, defaults)).Load();
    if (StateOf(legacyPath) != legacyBefore) Fail(label, "the load changed HeadTracking.ini's bytes, write time or attributes");

    if (!legacyBefore) {
        if (loaded.status != ConfigLoadStatus::Created) Fail(label, "no file is not Created");
        if (Names(folder) != std::set<std::wstring>{kCanonicalName}) Fail(label, "a first launch left more than CameraUnlock.ini");
    } else if (loaded.status != ConfigLoadStatus::Migrated) {
        Fail(label, std::string("the migration is ") + cfg::ConfigLoadStatusName(loaded.status) + ": " + loaded.reason);
        return std::nullopt;
    } else {
        if (Names(folder) != std::set<std::wstring>{kIniName, kCanonicalName}) {
            Fail(label, "the folder holds more than HeadTracking.ini and CameraUnlock.ini");
        }
        const std::string migrated = ReadBytes(path);
        const cfg::CanonicalIni doc = cfg::ParseCanonicalIni(migrated);
        Config reread = DL2HT::MakeConfigTable().defaults();
        if (!cfg::HasCanonicalStamp(migrated) || !doc.IsReadable() || !doc.diagnostics.empty() ||
            !cfg::ApplyCanonical(doc, DL2HT::MakeConfigTable(), reread).diagnostics.empty()) {
            Fail(label, "the migrated file is not a stamped canonical file that reads without a diagnostic");
        }
        tally.migrated.insert(migrated);
    }

    // The next start reads CameraUnlock.ini, imports nothing and writes nothing.
    const std::optional<FileState> created = StateOf(path);
    const cfg::ConfigLoadResult<Config> again = cfg::ConfigOwner<Config>(Options(folder, defaults)).Load();
    if (again.status != ConfigLoadStatus::Canonical || !again.diagnostics.empty()) {
        Fail(label, std::string("the next start is ") + cfg::ConfigLoadStatusName(again.status));
    }
    if (Values(again.config) != Values(loaded.config)) Fail(label, "the next start gives other settings");
    if (StateOf(path) != created || StateOf(legacyPath) != legacyBefore) Fail(label, "the next start changed a file");
    if (legacyBefore && !LogSays(again.log, "is left as it was and is not read")) {
        Fail(label, "the next start does not log that HeadTracking.ini is not read");
    }
    return loaded.config;
}

// ---------------------------------------------------------------------------
// The run
// ---------------------------------------------------------------------------

struct Folders {
    std::wstring oracle;
    std::wstring import;
    std::wstring migration;
    std::wstring read_only;
    std::wstring skewed;
    // Defaults.ini at the built-in values, which the first owner creates, and one that differs
    // from them on every global row the table binds.
    std::wstring defaults;
    std::wstring skewed_defaults;
};

void MigrateInput(const Folders& f, const Input& input, const ImportRun& i, const cfg::ImportResult* result,
                  MigrationTally& tally) {
    EmptyFolder(f.migration);
    if (input.bytes) WriteBytes(f.migration + L"\\" + kIniName, *input.bytes);
    const std::optional<Config> migrated = Migrate(f.migration, f.defaults, input.name, tally);
    if (!migrated) return;

    if (!input.bytes) {
        ++tally.created;
        if (ReadBytes(f.migration + L"\\" + kCanonicalName) != tally.committed) {
            Fail(input.name, "the created CameraUnlock.ini is not HeadTracking.ini as committed");
        }
        for (const std::string& d :
             Differences(Observe(WithoutReticleToggle(FromImport(i.status, i.cfg))), Observe(FromMigration(*migrated)))) {
            Fail(input.name, "comparison 2: " + d + " (the import without the reticle key / the migration)");
        }
        return;
    }

    ++tally.converted;
    const Record want = Observe(Expected(input.name, FromImport(i.status, i.cfg), i.cfg, *result, tally.drops));
    for (const std::string& d : Differences(want, Observe(FromMigration(*migrated)))) {
        Fail(input.name, "comparison 2: " + d + " (the import with the approved changes / the migration)");
    }

    EmptyFolder(f.read_only);
    const std::wstring readOnly = f.read_only + L"\\" + kIniName;
    WriteBytes(readOnly, *input.bytes);
    SetFileAttributesW(readOnly.c_str(), FILE_ATTRIBUTE_READONLY);
    const std::optional<Config> fromReadOnly = Migrate(f.read_only, f.defaults, input.name + " (read-only)", tally);
    if (fromReadOnly && Values(*fromReadOnly) != Values(*migrated)) {
        Fail(input.name, "a read-only HeadTracking.ini migrates to other settings than a writable one");
    }
    if ((GetFileAttributesW(readOnly.c_str()) & FILE_ATTRIBUTE_READONLY) == 0) {
        Fail(input.name, "HeadTracking.ini lost its read-only attribute");
    }

    EmptyFolder(f.skewed);
    WriteBytes(f.skewed + L"\\" + kIniName, *input.bytes);
    const std::optional<Config> overSkewed = Migrate(f.skewed, f.skewed_defaults, input.name + " (skewed Defaults.ini)", tally);
    if (overSkewed && Values(*overSkewed) != Values(*migrated)) {
        Fail(input.name, "the migration gives other settings over a Defaults.ini that differs on every global row");
    }
}

void RunInput(const Folders& f, const Input& input, MigrationTally& tally) {
    OracleRun o;
    {
        EmptyFolder(f.oracle);
        const std::wstring path = f.oracle + L"\\" + kIniName;
        if (input.bytes) WriteBytes(path, *input.bytes);
        o.status = oracle_api::LoadOrCreate(Narrow(path).c_str(), o.cfg);
    }

    ImportRun i;
    std::optional<cfg::ImportResult> result;
    {
        EmptyFolder(f.import);
        const std::wstring path = f.import + L"\\" + kIniName;
        if (input.bytes) {
            WriteBytes(path, *input.bytes);
            SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY);
        }
        const auto before = Snapshot(f.import);
        i.status = legacy::Read(Narrow(path).c_str(), i.cfg);
        if (input.bytes) {
            Config mapped = DL2HT::MakeConfigTable().defaults();
            result = DL2HT::MakeLegacyImport().run(cfg::LegacyInput{path, Narrow(path), false}, mapped);
            if (result->status != cfg::ImportStatus::Imported) Fail(input.name, "the import does not read the file");
        }
        if (Snapshot(f.import) != before) Fail(input.name, "the import changed the folder it read from");
        if (!input.bytes && i.status != legacy::ReadStatus::Absent) Fail(input.name, "the import read a file that is not there");
    }

    CompareOracleWithImport(input, o, i);
    MigrateInput(f, input, i, result ? &*result : nullptr, tally);
}

std::string ReadInput(const std::string& file) {
    return ReadBytes(Widen(std::string(DL2_DIFFERENTIAL_INPUTS) + "/" + file));
}

// `text` with the value of its one `key=` line replaced, the rest of that line included.
std::string WithValue(const std::string& text, const std::string& key, const std::string& value) {
    const size_t at = text.find("\n" + key + "=");
    if (at == std::string::npos || text.find("\n" + key + "=", at + 1) != std::string::npos) {
        throw std::logic_error("the file does not hold exactly one " + key + " line");
    }
    const size_t start = at + 1 + key.size() + 1;
    const size_t end = text.find_first_of("\r\n", start);
    return text.substr(0, start) + value + text.substr(end);
}

// `text` with `line` inserted after its one `after` line.
std::string WithLineAfter(const std::string& text, const std::string& after, const std::string& line) {
    const size_t at = text.find("\n" + after + "\n");
    if (at == std::string::npos || text.find("\n" + after + "\n", at + 1) != std::string::npos) {
        throw std::logic_error("the file does not hold exactly one " + after + " line");
    }
    const size_t end = at + 1 + after.size() + 1;
    return text.substr(0, end) + line + "\n" + text.substr(end);
}

// Every hotkey code the reader takes as a key, 0x01 to 0xFE, on all four hotkeys at once. The
// corpus tries one alternate code per hotkey; this is where every code has to come through.
std::vector<Input> EveryHotkeyCode(const std::string& shipped) {
    std::vector<Input> inputs;
    for (int vk = 0x01; vk <= 0xFE; ++vk) {
        char code[8];
        std::snprintf(code, sizeof(code), "0x%02X", static_cast<unsigned>(vk));
        std::string bytes = shipped;
        for (const char* key : {"ToggleKey", "TrackingModeKey", "YawModeKey", "ReticleToggleKey"}) {
            bytes = WithValue(bytes, key, code);
        }
        inputs.push_back({std::string("every hotkey ") + code, bytes});
    }
    return inputs;
}

// A file that exists and cannot be opened: v1.4.0 ran on its defaults and could not write them
// over it, the import reports it as such, and the owner defers it on the defaults, creates no
// CameraUnlock.ini and saves nothing that session.
void TestUnopenableFile(const Folders& f, const std::string& shipped) {
    const Input input{"a file another program holds open with no sharing", shipped};
    OracleRun o;
    ImportRun i;
    std::optional<cfg::ConfigLoadResult<Config>> loaded;
    for (const std::wstring& dir : {f.oracle, f.import, f.migration}) {
        EmptyFolder(dir);
        const std::wstring path = dir + L"\\" + kIniName;
        WriteBytes(path, shipped);
        HANDLE held = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (held == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot hold the test file open");
        if (dir == f.oracle) {
            o.status = oracle_api::LoadOrCreate(Narrow(path).c_str(), o.cfg);
        } else if (dir == f.import) {
            i.status = legacy::Read(Narrow(path).c_str(), i.cfg);
        } else {
            cfg::ConfigOwner<Config> owner(Options(dir, f.defaults));
            loaded.emplace(owner.Load());
            if (owner.Save([](Config& c) { c.world_space_yaw = false; }).status != cfg::ConfigSaveStatus::NotSaved) {
                Fail(input.name, "a deferred session saved");
            }
        }
        CloseHandle(held);
        if (ReadBytes(path) != shipped) Fail(input.name, "a build rewrote a file it could not open");
    }
    if (o.status != oracle_api::LoadStatus::OpenFailed) Fail(input.name, "v1.4.0 did not report it unopenable");
    if (i.status != legacy::ReadStatus::OpenFailed) Fail(input.name, "the import did not report it unopenable");
    CompareOracleWithImport(input, o, i);
    if (loaded->status != cfg::ConfigLoadStatus::Deferred) {
        Fail(input.name, std::string("the owner's load is ") + cfg::ConfigLoadStatusName(loaded->status) + ", not Deferred");
    }
    for (const std::string& d :
         Differences(Observe(WithoutReticleToggle(FromImport(i.status, i.cfg))), Observe(FromMigration(loaded->config)))) {
        Fail(input.name, "comparison 2: " + d + " (the import without the reticle key / the deferred session)");
    }
    if (Snapshot(f.migration) != std::map<std::wstring, std::string>{{kIniName, shipped}}) {
        Fail(input.name, "a deferred import left more than HeadTracking.ini as it was");
    }
    if (!LogSays(loaded->log, "in use by another program") && loaded->reason.find("in use by another program") == std::string::npos) {
        Fail(input.name, "the deferred import does not say the file is in use");
    }
}

// Registration compares the two builds by key and modifiers, which holds only while a binding
// with no modifiers fires as the old build's NavGuarded did (not while Ctrl and Shift are both
// held) and a Ctrl+Shift binding as its ChordGuarded did (while both are held). Alt changes
// neither.
void TestRegistrationModel() {
    using cameraunlock::input::detail::BindingFires;
    for (unsigned held = 0; held < 8; ++held) {
        const auto mods = static_cast<KeyModifiers>(held);
        const bool chordHeld = cameraunlock::input::HasModifiers(mods, KeyModifiers::kCtrl | KeyModifiers::kShift);
        if (BindingFires(KeyModifiers::kNone, mods) != !chordHeld) {
            Fail("registration", "a key with no modifiers does not fire as NavGuarded did, held " + std::to_string(held));
        }
        if (BindingFires(KeyModifiers::kCtrl | KeyModifiers::kShift, mods) != chordHeld) {
            Fail("registration", "a Ctrl+Shift key does not fire as ChordGuarded did, held " + std::to_string(held));
        }
    }
}

// The frozen defaults are v1.4.0's, field for field. The first-launch yaw is applied by the
// startup code, not held in the frozen struct, so it compares here too.
void TestFrozenDefaults() {
    const oracle_api::Config o = oracle_api::Defaults();
    const legacy::Config l;
    const Record published = Observe(FromOracle(o));
    Reading frozen = FromImport(legacy::ReadStatus::Read, l);
    frozen.limit_y_down = 0.20f;
    for (const std::string& d : Differences(published, Observe(frozen))) {
        Fail("defaults", d + ": the frozen default differs from v1.4.0's");
    }
}

}  // namespace

int main() {
    try {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        const std::wstring root = std::wstring(temp) + L"dl2-config-differential-" + std::to_wstring(GetCurrentProcessId());
        CreateDirectoryW(root.c_str(), nullptr);
        const std::wstring global = MakeFolder(root, L"global");
        const std::wstring skewedGlobal = MakeFolder(root, L"skewed-global");
        const Folders folders{MakeFolder(root, L"oracle"),    MakeFolder(root, L"import"),
                              MakeFolder(root, L"migration"), MakeFolder(root, L"read-only"),
                              MakeFolder(root, L"skewed"),    global + L"\\Defaults.ini",
                              skewedGlobal + L"\\Defaults.ini"};
        WriteBytes(folders.skewed_defaults, kSkewedDefaults);
        MigrationTally tally;
        tally.committed = ReadBytes(Widen(DL2_COMMITTED_CONFIG));

        TestFrozenDefaults();
        TestRegistrationModel();

        const std::string shipped = ReadInput("shipped-v1.4.0.ini");
        {
            EmptyFolder(folders.oracle);
            const std::wstring path = folders.oracle + L"\\" + kIniName;
            oracle_api::Config created;
            if (oracle_api::LoadOrCreate(Narrow(path).c_str(), created) != oracle_api::LoadStatus::Created ||
                ReadBytes(path) != ReadInput("first-run-v1.4.0.ini")) {
                Fail("first run", "v1.4.0's first-run output is not inputs/first-run-v1.4.0.ini");
            }
        }

        std::vector<Input> inputs = {
            {"no file", std::nullopt},
            {"empty file", std::string()},
        };
        for (const char* file :
             {"shipped-v1.0.0.ini", "committed-7c996cf.ini", "shipped-v1.0.1.ini", "shipped-v1.0.2.ini",
              "shipped-v1.0.5.ini", "shipped-v1.1.0.ini", "shipped-v1.4.0.ini", "first-run-v1.0.0.ini",
              "first-run-v1.0.1.ini", "first-run-v1.0.2.ini", "first-run-v1.0.5.ini", "first-run-v1.1.0.ini",
              "first-run-v1.2.0.ini", "first-run-v1.4.0.ini"}) {
            inputs.push_back({file, ReadInput(file)});
        }
        inputs.push_back({"AutoEnable continued on an indented line with a comment",
                          WithLineAfter(WithValue(shipped, "AutoEnable", "false"), "AutoEnable=false",
                                        "    true ; turned back on"),
                          true});
        for (const Input& input : inputs) RunInput(folders, input, tally);
        TestUnopenableFile(folders, shipped);

        const std::vector<IniMutation> corpus = GenerateIniMutations(shipped, legacy::ReadKeys(), CorpusKeys());
        for (const IniMutation& m : corpus) RunInput(folders, {"corpus: " + m.name, m.bytes}, tally);

        const std::vector<Input> codes = EveryHotkeyCode(shipped);
        for (const Input& input : codes) RunInput(folders, input, tally);

        std::printf("%zu inputs, %zu of them from the corpus and %zu with every hotkey on one code\n",
                    inputs.size() + 1 + corpus.size() + codes.size(), corpus.size(), codes.size());
        std::printf("comparison 1, v1.4.0 against the frozen reader:\n");
        for (const ListedDifference& d : kComparisonOneDifferences) {
            std::printf("  %s (%s): %d inputs\n    %s\n", d.id, d.commit, d.seen, d.what);
            if (d.seen == 0) Fail(d.id, "a listed difference no input shows");
        }
        std::printf("comparison 2, the frozen reader against the migration: %d created, %d converted, "
                    "%zu distinct files\n",
                    tally.created, tally.converted, tally.migrated.size());
        std::printf("  %d with a changed sensitivity or an inversion dropped (pose_shaping)\n", tally.drops.pose_shaping);
        std::printf("  %d with the reticle switch or key dropped (reticle)\n", tally.drops.reticle);
        std::printf("  %d with a hotkey code outside 0x01-0xFE unbound (N1)\n", tally.drops.n1);
        std::printf("  %d with a position limit that is not a number at its default (N2)\n", tally.drops.n2);
        if (tally.drops.pose_shaping == 0) Fail("pose shaping", "no input drops a changed value");
        if (tally.drops.reticle == 0) Fail("reticle", "no input drops a reticle setting");
        if (tally.drops.n1 == 0) Fail("N1", "no input unbinds a hotkey code");
        if (tally.drops.n2 == 0) Fail("N2", "no input takes a limit that is not a number to its default");
        if (tally.migrated.count(tally.committed) == 0) Fail("first run", "no input migrated to the committed file");

        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring lintDir(exe);
        lintDir = lintDir.substr(0, lintDir.find_last_of(L'\\'));
        lintDir = MakeFolder(lintDir, L"migrated");
        int n = 0;
        for (const std::string& file : tally.migrated) {
            WriteBytes(lintDir + L"\\" + std::to_wstring(n++) + L".ini", file);
        }

        for (const std::wstring& dir : {folders.oracle, folders.import, folders.migration, folders.read_only,
                                        folders.skewed, global, skewedGlobal}) {
            EmptyFolder(dir);
            RemoveDirectoryW(dir.c_str());
        }
        RemoveDirectoryW(root.c_str());
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }

    if (g_failures == 0) {
        std::printf("config differential: all passed\n");
        return 0;
    }
    std::printf("config differential: %d failure(s)\n", g_failures);
    return 1;
}
