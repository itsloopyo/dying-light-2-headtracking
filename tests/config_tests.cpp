// The settings file: the committed HeadTracking.ini is the table's fresh render, which a first
// launch creates as CameraUnlock.ini, the defaults the builds before it ran on without a file map
// to the table's defaults, the file v1.4.0 shipped imports into the committed file but for the
// yaw it named, the toggles save only their own lines, End's row cannot be saved, and a row
// holding default takes Defaults.ini's value. Every owner reads a scratch Defaults.ini.
//
// `--render-config <path>` writes the committed file instead (pixi run render-config).

#include "pch.h"
#include "core/config.h"

#include "legacy_config/legacy_config.h"

#include <cameraunlock/tracking/tracking_mode.h>

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace DL2HT;

namespace {

namespace cfg = cameraunlock::config;

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

std::string ReadBytes(const std::wstring& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read a test file");
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const std::wstring& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write a test file");
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::wstring Widen(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

std::string Narrow(const std::wstring& path) {
    const int size = WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, out.data(), size, nullptr, nullptr);
    out.resize(static_cast<size_t>(size) - 1);
    return out;
}

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    const size_t at = text.find(from);
    if (at == std::string::npos) throw std::logic_error("'" + from + "' is not in the text");
    return text.replace(at, from.size(), to);
}

std::string Rendered() {
    return cfg::RenderCanonicalFresh(MakeConfigTable(), {kConfigDisplayName});
}

const wchar_t* const kScratchFiles[] = {kConfigFileName, kLegacyFileName, L"Defaults.ini"};

// A scratch folder under the temp folder, ending in its separator, emptied of what an earlier run
// left there.
std::wstring ScratchFolder(const wchar_t* name) {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    const std::wstring dir = std::wstring(temp) + L"dl2-config-" + name + L"-" +
                             std::to_wstring(GetCurrentProcessId()) + L"\\";
    CreateDirectoryW(dir.c_str(), nullptr);
    for (const wchar_t* file : kScratchFiles) DeleteFileW((dir + file).c_str());
    return dir;
}

void RemoveScratchFolder(const std::wstring& dir) {
    for (const wchar_t* file : kScratchFiles) DeleteFileW((dir + file).c_str());
    RemoveDirectoryW(dir.c_str());
}

cfg::ConfigOwnerOptions<Config> Options(const std::wstring& folder, const std::wstring& defaults) {
    return MakeConfigOwnerOptions(folder, cfg::DefaultsFile::At(defaults));
}

std::vector<std::string> Lines(const std::string& bytes) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < bytes.size()) {
        const size_t end = bytes.find("\r\n", start);
        lines.push_back(bytes.substr(start, end - start));
        start = end + 2;
    }
    return lines;
}

// The lines of `after` that differ from `before`, which must have as many lines.
std::vector<std::string> ChangedLines(const std::string& before, const std::string& after) {
    const std::vector<std::string> a = Lines(before);
    const std::vector<std::string> b = Lines(after);
    if (a.size() != b.size()) return {"a line was added or removed"};
    std::vector<std::string> changed;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) changed.push_back(b[i]);
    }
    return changed;
}

void TestCommittedConfigIsRendered() {
    const std::string committed = ReadBytes(Widen(DL2_COMMITTED_CONFIG));
    Check(committed == Rendered(), "HeadTracking.ini is the table's fresh render (pixi run render-config)");
    for (const char* line :
         {"UdpPort=default", "EnableOnStartup=default", "WorldSpaceYaw=default", "RotationEnabled=default",
          "LocalSmoothing=default", "RemoteSmoothing=default", "PositionEnabled=default", "PositionLimitX=default",
          "PositionLimitY=default", "PositionLimitYDown=default", "PositionLimitZ=default",
          "PositionLimitZBack=default", "ToggleKey=default", "CycleTrackingModeKey=default", "YawModeKey=default",
          "ShowNotifications=true"}) {
        Check(committed.find(std::string("\r\n") + line + "\r\n") != std::string::npos,
              std::string("the committed file holds ") + line);
    }
}

// The built-in defaults are the fleet's, and the ones the builds before the conversion ran on.
void TestDefaults() {
    const Config d = MakeConfigTable().defaults();
    Check(d.toggle_key_name == "End, Ctrl+Shift+Y", "ToggleKey defaults to End, Ctrl+Shift+Y");
    Check(d.cycle_tracking_mode_key_name == "PageUp, Ctrl+Shift+G",
          "CycleTrackingModeKey defaults to PageUp, Ctrl+Shift+G");
    Check(d.yaw_mode_key_name == "PageDown, Ctrl+Shift+H", "YawModeKey defaults to PageDown, Ctrl+Shift+H");
    Check(d.udp_port == 4242 && d.enable_on_startup && d.world_space_yaw && d.show_notifications,
          "port 4242, on at startup, world-locked yaw and notices logged");
    Check(StartupTrackingMode(d) == cameraunlock::TrackingMode::RotationAndPosition,
          "the default tracking mode is rotation and position");
    Check(d.position.limit_x == 0.30f && d.position.limit_y == 0.20f && d.position.limit_y_down == 0.20f &&
              d.position.limit_z == 0.40f && d.position.limit_z_back == 0.10f,
          "the lean limits are 0.30 m sideways, 0.20 m up and down, 0.40 m forward and 0.10 m back");
    Check(d.local_smoothing == 0.0f && d.remote_smoothing == 0.15f, "smoothing is 0 locally and 0.15 remotely");
}

// A start the frozen reader finds no file for maps to the table's defaults: the build ran on its
// frozen defaults with world-locked yaw, every sensitivity and inversion they hold is the shipped
// one, and only the reticle key the build bound is dropped.
void TestLegacyDefaultsMapToTheDefaults() {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    const std::wstring missing = std::wstring(temp) + L"dl2-no-such-folder\\" + kLegacyFileName;
    const auto table = MakeConfigTable();
    Config mapped = table.defaults();
    const cfg::ImportResult result =
        MakeLegacyImport().run(cfg::LegacyInput{missing, Narrow(missing), false}, mapped);
    Check(result.status == cfg::ImportStatus::Absent, "no file imports as Absent");
    Check(result.dropped.size() == 1 && result.dropped[0].rule == cfg::DropRule::Reticle &&
              result.dropped[0].key == "ReticleToggleKey" && result.dropped[0].value == "0x2D",
          "the old defaults drop only the reticle key, Insert");
    Check(result.pose_shaping.size() == 9, "every sensitivity and position inversion is recorded");
    for (const cfg::PoseShapingValue& value : result.pose_shaping) {
        Check(value.folded, "[" + value.section + "] " + value.key + " holds its shipped value and is folded");
    }
    Check(cfg::RenderCanonical(table, mapped, {kConfigDisplayName}) ==
              cfg::RenderCanonical(table, table.defaults(), {kConfigDisplayName}),
          "the old defaults map to the defaults");
}

// Fresh equals upgrade: the file v1.4.0 shipped, and the one it wrote at first launch, import into
// the committed file but for WorldSpaceYaw. Both name WorldLockedYaw=false, the camera-local yaw
// v1.4.0 ran, which 0307ede moved the no-file default away from.
void TestShippedFilesImportAsTheCommittedFile() {
    const std::string committed = ReadBytes(Widen(DL2_COMMITTED_CONFIG));
    for (const char* file : {"shipped-v1.4.0.ini", "first-run-v1.4.0.ini"}) {
        const std::wstring dir = ScratchFolder(L"upgrade");
        const std::wstring global = ScratchFolder(L"upgrade-global");
        const std::string legacy = ReadBytes(Widen(std::string(DL2_DIFFERENTIAL_INPUTS) + "/" + file));
        WriteBytes(dir + kLegacyFileName, legacy);
        const auto loaded = cfg::ConfigOwner<Config>(Options(dir, global + L"Defaults.ini")).Load();
        Check(loaded.status == cfg::ConfigLoadStatus::Migrated, std::string(file) + " imports");
        Check(ChangedLines(committed, ReadBytes(dir + kConfigFileName)) ==
                  std::vector<std::string>{"WorldSpaceYaw=false"},
              std::string(file) + " imports into the committed file with WorldSpaceYaw=false");
        Check(ReadBytes(dir + kLegacyFileName) == legacy, std::string(file) + " keeps its bytes");
        RemoveScratchFolder(dir);
        RemoveScratchFolder(global);
    }
}

// A first launch creates the committed file's bytes as CameraUnlock.ini, a save writes the value
// of its rows over default and changes no other byte, the yaw mode and the tracking mode persist,
// End's row cannot be saved at all, and Defaults.ini is never written.
void TestTogglesSave() {
    const std::wstring dir = ScratchFolder(L"save");
    const std::wstring global = ScratchFolder(L"save-global");
    const std::wstring defaults = global + L"Defaults.ini";
    const std::wstring path = dir + kConfigFileName;
    const std::string committed = ReadBytes(Widen(DL2_COMMITTED_CONFIG));

    {
        cfg::ConfigOwner<Config> owner(Options(dir, defaults));
        const auto created = owner.Load();
        Check(created.status == cfg::ConfigLoadStatus::Created,
              std::string("a first launch creates the file, not ") + cfg::ConfigLoadStatusName(created.status));
        Check(ReadBytes(path) == committed, "a first launch writes the committed file's bytes");
        Check(GetFileAttributesW((dir + kLegacyFileName).c_str()) == INVALID_FILE_ATTRIBUTES,
              "a first launch writes no HeadTracking.ini");
        const std::string defaultsBytes = ReadBytes(defaults);

        const cfg::ConfigSaveResult yaw = owner.Save([](Config& c) { c.world_space_yaw = false; });
        Check(yaw.status == cfg::ConfigSaveStatus::Saved, "the yaw mode saves");
        Check(yaw.log.size() == 1 && yaw.log[0].find("WorldSpaceYaw=false") != std::string::npos,
              "the yaw save logs that WorldSpaceYaw no longer follows Defaults.ini");
        const std::string afterYaw = ReadBytes(path);
        Check(ChangedLines(committed, afterYaw) == std::vector<std::string>{"WorldSpaceYaw=false"},
              "saving the yaw mode writes its value over default and changes nothing else");

        const auto rotationOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::RotationOnly);
        Check(owner.Save([rotationOnly](Config& c) {
                  c.rotation_enabled = rotationOnly.rotation_enabled;
                  c.position_enabled = rotationOnly.position_enabled;
              }).status == cfg::ConfigSaveStatus::Saved,
              "the tracking mode saves");
        const std::string afterRotationOnly = ReadBytes(path);
        Check(ChangedLines(afterYaw, afterRotationOnly) ==
                  std::vector<std::string>{"RotationEnabled=true", "PositionEnabled=false"},
              "saving rotation only writes both tracking mode rows over default and changes nothing else");

        const auto positionOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::PositionOnly);
        Check(owner.Save([positionOnly](Config& c) {
                  c.rotation_enabled = positionOnly.rotation_enabled;
                  c.position_enabled = positionOnly.position_enabled;
              }).status == cfg::ConfigSaveStatus::Saved,
              "the third tracking mode saves");
        Check(ChangedLines(afterRotationOnly, ReadBytes(path)) ==
                  std::vector<std::string>{"RotationEnabled=false", "PositionEnabled=true"},
              "saving position only changes the mode pair and nothing else");

        bool refused = false;
        try {
            owner.Save([](Config& c) { c.enable_on_startup = false; });
        } catch (const std::logic_error&) {
            refused = true;
        }
        Check(refused, "EnableOnStartup is not Writable, so the End toggle cannot persist");
        Check(ReadBytes(defaults) == defaultsBytes, "no save changes Defaults.ini");
    }

    cfg::ConfigOwner<Config> reopened(Options(dir, defaults));
    const auto again = reopened.Load();
    Check(again.status == cfg::ConfigLoadStatus::Canonical && again.diagnostics.empty() &&
              !again.config.world_space_yaw &&
              StartupTrackingMode(again.config) == cameraunlock::TrackingMode::PositionOnly &&
              again.config.enable_on_startup,
          "the saved yaw and tracking mode come back at the next start");

    RemoveScratchFolder(dir);
    RemoveScratchFolder(global);
}

// A fresh file holds default on every global row, so a Defaults.ini the player edited reaches
// the game, a row Defaults.ini leaves out takes the built-in value, and a value the game's own
// file holds wins over Defaults.ini.
void TestDefaultRowsFollowDefaultsIni() {
    const std::wstring dir = ScratchFolder(L"follows");
    const std::wstring global = ScratchFolder(L"follows-global");
    const std::wstring defaults = global + L"Defaults.ini";
    WriteBytes(dir + kConfigFileName, Rendered());
    WriteBytes(defaults,
               "[CameraUnlock]\r\nConfigFormat=1\r\n\r\n[Network]\r\nUdpPort=5252\r\n\r\n[General]\r\n"
               "WorldSpaceYaw=false\r\n\r\n[Position]\r\nPositionLimitZ=0.25\r\n\r\n[Hotkeys]\r\nToggleKey=F8\r\n");
    const auto loaded = cfg::ConfigOwner<Config>(Options(dir, defaults)).Load();
    Check(loaded.status == cfg::ConfigLoadStatus::Canonical, "the committed file loads as canonical");
    Check(loaded.config.udp_port == 5252 && !loaded.config.world_space_yaw && loaded.config.toggle_key_name == "F8" &&
              loaded.config.position.limit_z == 0.25f,
          "rows holding default take Defaults.ini's values");
    Check(loaded.config.cycle_tracking_mode_key_name == "PageUp, Ctrl+Shift+G" &&
              loaded.config.position.limit_z_back == 0.10f,
          "a row Defaults.ini leaves out takes the built-in value");

    WriteBytes(dir + kConfigFileName, Replace(Rendered(), "WorldSpaceYaw=default\r\n", "WorldSpaceYaw=true\r\n"));
    Check(cfg::ConfigOwner<Config>(Options(dir, defaults)).Load().config.world_space_yaw,
          "a value written in CameraUnlock.ini wins over Defaults.ini");

    RemoveScratchFolder(dir);
    RemoveScratchFolder(global);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::strcmp(argv[1], "--render-config") == 0) {
            const std::string rendered = Rendered();
            std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
            out.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
            if (!out) {
                std::printf("could not write %s\n", argv[2]);
                return 1;
            }
            return 0;
        }

        TestCommittedConfigIsRendered();
        TestDefaults();
        TestLegacyDefaultsMapToTheDefaults();
        TestShippedFilesImportAsTheCommittedFile();
        TestTogglesSave();
        TestDefaultRowsFollowDefaultsIni();
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }

    if (g_failures == 0) {
        std::printf("config tests: all passed\n");
        return 0;
    }
    std::printf("config tests: %d failure(s)\n", g_failures);
    return 1;
}
