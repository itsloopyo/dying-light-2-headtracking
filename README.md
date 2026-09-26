# Dying Light 2 Head Tracking

![Dying Light 2 Stay Human running with this mod](https://raw.githubusercontent.com/itsloopyo/dying-light-2-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Dying Light 2 Stay Human that moves the view with your head while your mouse or controller keeps aiming, driven by OpenTrack over UDP, with no VR headset required.

Lean into a window frame while your crosshair stays on the zombie you were already aiming at. Glance down at a rooftop gap mid-parkour without your jump going with your eyes. Your head drives the camera; the mouse still drives the aim.

**Updating from an earlier version?** Settings now live in `CameraUnlock.ini`
in `ph/work/bin/x64`, next to the mod. The first start of this version reads
your settings from `HeadTracking.ini` into it and leaves `HeadTracking.ini` as
it was. See [Configuration](#configuration).

## Features

- **Decoupled look + aim**: Look around freely with your head while your aim stays independent
- **6DOF head tracking**: Full rotation (yaw, pitch, roll) and positional tracking (X, Y, Z) via OpenTrack UDP protocol
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- Dying Light 2 Stay Human v1.16 or later
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) - bundled with the
  installer download and set up by `install.cmd`. The Nexus download does not
  include it; install it yourself first (see Manual Installation).
- [OpenTrack](https://github.com/opentrack/opentrack) or a compatible head tracking app (smartphone, webcam, or dedicated hardware)

## Installation

### Lopari

Download [Lopari](https://lopari.app), choose **Dying Light 2 Stay Human**, and click
**Play with head tracking**.

### Standalone Installer

1. Download the latest release from the [Releases page](https://github.com/itsloopyo/dying-light-2-headtracking/releases)
2. Extract the ZIP anywhere
3. Double-click `install.cmd`

### Manual Installation

**Step 1: Install ASI Loader**

1. Download [Ultimate-ASI-Loader_x64.zip](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases)
2. Extract `winmm.dll` to your game directory:
   ```
   <DL2 Install>/ph/work/bin/x64/winmm.dll
   ```

**Step 2: Install the Mod**

1. Copy `DL2HeadTracking.asi` to:
   ```
   <DL2 Install>/ph/work/bin/x64/DL2HeadTracking.asi
   ```
2. Start the game. The mod creates `CameraUnlock.ini` in the same directory.

### Finding Your Game Directory

- **Steam:** Right-click Dying Light 2 > Manage > Browse local files > navigate to `ph/work/bin/x64/`
- **Epic Games:** `C:\Program Files\Epic Games\DyingLight2\ph\work\bin\x64\`
- **GOG:** `C:\GOG Games\Dying Light 2\ph\work\bin\x64\`

## Setting Up OpenTrack

The mod listens for OpenTrack pose data on UDP port `4242`, on every network
interface. One datagram is six little-endian 64-bit floats in the order
`x, y, z, yaw, pitch, roll`: position in centimetres, rotation in degrees, 48
bytes in total. Anything that sends that to that port drives the view.
OpenTrack's **UDP over network** output sends exactly this, and the steps below
set it up.

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Pick a tracker under **Input**, using the notes below.
3. Set **Output** to **UDP over network**, host `127.0.0.1`, port `4242`.
4. Press **Start**. Tracking and the game can start in either order.

### Webcam

OpenTrack ships a `neuralnet tracker` input that reads a plain webcam. Select it
under **Input**, pick your camera in its settings, and use the output settings
above. How well it tracks depends on your camera and your lighting, so try it
before buying anything.

### Phone

A phone app can reach the mod directly, with no OpenTrack on the PC, if it sends
the datagram described above. Point it at this PC's IP address (run `ipconfig`
to find it) on port `4242`. Not every phone tracker speaks this protocol, so
check yours for an OpenTrack or UDP output option first. [Headcam](https://headcam.app)
sends it, and I wrote it so decent tracking is free for anyone who already owns
a phone.

Sending direct works when the app filters its own signal on the device. The
mod's smoothing is sized to take the edge off a clean signal rather than to
rescue a noisy one, so a raw feed sent direct will jitter. If it does, point the
app at OpenTrack's **UDP over network** *input* on some other port, say 5252,
and let OpenTrack's filters and curves clean it up before its output forwards to
`127.0.0.1:4242`.

Anything arriving from outside `127.0.0.0/8` counts as a remote connection and
is smoothed with `RemoteSmoothing` rather than `LocalSmoothing`. That includes a
tracker on this very PC that sends to the machine's own LAN address, because the
mod reads the source address and not the machine.

### Headset or other hardware

If your device has an OpenTrack input driver, select it under **Input** and use
the same output settings. OpenTrack's own **Input** list is the authority on
what it can read; the mod only ever sees what OpenTrack sends.

### Centring

Centring belongs to your tracker. The mod subtracts no centre of its own: it
applies the pose it receives exactly as it arrives, so a stream of zeros holds
the view where the game itself puts it. Press the centre control in your tracker
(OpenTrack's **Center** bind, or the CENTER button in Headcam) and the tracker
zeroes its own output, which leaves the view centred with the mod doing nothing.

That is why there is no centre hotkey here and nothing to re-centre in game. Two
centres in series would drift apart, because each side re-centres at moments the
other cannot see, and you would end up pressing twice to centre once. If the
view sits off to one side, centre it in the tracker.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord          |
|---------------------|-------------|----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y` |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G` |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H` |

Both keys of each action are one list in `CameraUnlock.ini` (`ToggleKey`,
`CycleTrackingModeKey`, `YawModeKey`), so either can be rebound or removed.

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

### Yaw mode

- **World-locked** (default): yaw rotates around the world's vertical axis.
  Pitch down and yaw - the horizon stays level and the view sweeps
  horizontally.
- **Camera-local**: yaw rotates around your head's up axis. When you pitch
  the view down and yaw, the horizon tilts and the view sweeps a cone.

The tracking mode and the yaw mode are saved to `CameraUnlock.ini` when you
change them, and the game starts in them next time. `End` changes the current
session only: head tracking starts on or off as `EnableOnStartup` says.

While head tracking is on, the mod draws a dot where your aim points. There is
no longer a key or setting that hides it.

## Configuration

<!-- cameraunlock:config -->
The mod reads its settings from `ph\work\bin\x64\CameraUnlock.ini` in the game folder, and creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.

A setting that the defaults below set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it. `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.

Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:

- Reticle settings, and a key that toggled the reticle.
- A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.

An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.

Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults below. Every setting they set to `default` then follows `Defaults.ini`.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `WorldSpaceYaw=true`
- `RotationEnabled=true`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`
- `YawModeKey=PageDown, Ctrl+Shift+H`

With every setting at its default, the file reads:

```ini
; Dying Light 2 Stay Human head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default
; true: write the mod's notices (tracking on or off, a mode change) to HeadTracking.log.
ShowNotifications=true

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=default
```
<!-- /cameraunlock:config -->

Earlier versions also read these settings, which this version no longer
reads: `YawMultiplier`, `PitchMultiplier` and `RollMultiplier` under
`[Sensitivity]`, `SensitivityX`, `SensitivityY`, `SensitivityZ`, `InvertX`,
`InvertY` and `InvertZ` under `[Position]`, `[Reticle] Enabled` and
`[Hotkeys] ReticleToggleKey`. A lean still moves the view twice as far as your
head moves, as the shipped `SensitivityX/Y/Z=2.0` did, and the aim dot is drawn
whenever head tracking is on.

The mod applies the head pose as your tracker sends it. Set sensitivity,
response curves and axis inversion in your tracker, so the same profile works
across games.

## Troubleshooting

**Mod not loading:**
- Verify `winmm.dll` (ASI Loader) is in `ph/work/bin/x64/`
- Check that `DL2HeadTracking.asi` is in the same directory
- Check `<DL2 Install>/ph/work/bin/x64/HeadTracking.log` for error messages (the previous run is kept as `HeadTracking.prev.log`)

**No tracking response:**
- Ensure your tracker is running and outputting data
- Verify the UDP port matches in both the tracker and `CameraUnlock.ini` (`UdpPort`)
- Press `End` to enable tracking if auto-enable is off
- If the view sits off to one side, centre it in your tracker app (opentrack's
  Center bind, or the CENTER button in a Headcam app)

**Camera jittering:**
- Increase filtering in your tracker software
- Raise `LocalSmoothing` or `RemoteSmoothing` in `CameraUnlock.ini`
- Improve lighting for webcam-based tracking

## Updating

Update through Lopari, or download the new release and run `install.cmd` again.
Neither copies a config file, so your `CameraUnlock.ini` stays as it is.

## Uninstalling

**Remove the mod only** - delete from `<DL2 Install>/ph/work/bin/x64/`:
- `DL2HeadTracking.asi`
- `HeadTracking.log` and `HeadTracking.prev.log` (if present)

`uninstall.cmd` removes the same files and leaves `CameraUnlock.ini` and
`HeadTracking.ini` in place, so your settings survive a reinstall.

**Remove all mods** - delete `winmm.dll` from the game directory to disable ASI loading entirely.

## Building from Source

### Prerequisites

- **Windows 10/11**
- **Visual Studio 2022** with C++ desktop development workload
- **CMake 3.20+**
- **Git**
- **Pixi** (optional, for task automation) - [Install pixi](https://pixi.sh)

### Clone the Repository

```bash
git clone --recurse-submodules https://github.com/itsloopyo/dying-light-2-headtracking.git
cd dying-light-2-headtracking
```

### Build

**With pixi:**
```bash
# Debug build
pixi run build

# Release build
pixi run build-release
```

**Manual CMake:**
```bash
# Configure
cmake -B build -A x64

# Build Debug
cmake --build build --config Debug

# Build Release
cmake --build build --config Release
```

Output: `bin/Debug/DL2HeadTracking.asi` or `bin/Release/DL2HeadTracking.asi`

### Install to Game

**With pixi:**
```bash
# Build release and install to game directory
pixi run install
```

The install scripts automatically detect your game installation via Steam/registry.

### Project Structure

```
dying-light-2-headtracking/
├── src/
│   ├── core/           # Core mod logic
│   │   ├── mod.cpp     # Main mod class
│   │   ├── config.cpp  # CameraUnlock.ini's table and HeadTracking.ini's import
│   │   └── logger.cpp  # Logging
│   ├── hooks/          # Game hooks
│   │   ├── engine_camera_hook.cpp  # Camera manipulation
│   │   ├── input_hook.cpp          # Hotkey handling
│   │   ├── dx_hook.cpp             # DirectX overlay
│   │   └── crosshair_hook.cpp      # Crosshair management
│   └── ui/             # User interface
│       └── notification.cpp        # On-screen notifications
├── extern/             # Vendored third-party sources (MinHook, ImGui, Kiero, inih)
├── vendor/             # Vendored Ultimate ASI Loader binary + its licence
├── scripts/            # Build and deployment scripts
├── HeadTracking.ini    # The CameraUnlock.ini a first launch creates (pixi run render-config)
├── CMakeLists.txt      # Build configuration
└── pixi.toml           # Task runner configuration
```

### Available Pixi Tasks

| Task | Description |
|------|-------------|
| `build` | Build debug configuration |
| `build-release` | Build release configuration |
| `install` | Build release and install to game directory |
| `uninstall` | Remove HeadTracking mod only |
| `test` | Run the config tests |
| `render-config` | Rewrite the committed config after a change to the config table |
| `detect-game` | Show detected game path |
| `clean` | Clean build artifacts |
| `clean-all` | Clean all artifacts including release |
| `release` | Automated release workflow |
| `show-changelog` | Preview changelog for next release |
| `test-udp` | Test UDP receiver with simulated data |
| `test-udp-continuous` | Send continuous test tracking data |

### Debugging

1. Build debug configuration: `pixi run build`
2. Install: `pixi run deploy`
3. Check logs at `<game dir>/ph/work/bin/x64/HeadTracking.log`
4. Attach Visual Studio debugger to `DyingLightGame_x64_rwdi.exe` if needed

## Community & Support

- Discord: [Loop's Head Tracking Hangout](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch for the released head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your iPhone or Android phone into the head tracker

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) for details.

Third-party components bundled or linked into the mod are listed with their full
licence texts in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Credits

- [Techland](https://techland.net/) - Dying Light 2
- [EGameTools by EricPlayZ](https://github.com/EricPlayZ/EGameTools) - the source of this mod's camera signature
- [OpenTrack](https://github.com/opentrack/opentrack) - Head tracking protocol and software
- [MinHook](https://github.com/TsudaKageyu/minhook) - API hooking library
- [Dear ImGui](https://github.com/ocornut/imgui) - Overlay UI
- [Kiero](https://github.com/Rebzzel/kiero) - DirectX hooking
- [inih](https://github.com/benhoyt/inih) - INI file parser
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) - ASI plugin loading

## Disclaimer

This is an unofficial, fan-made mod. It is not affiliated with, endorsed by, or
supported by Techland. Dying Light 2 Stay Human and all related trademarks are
the property of Techland, used here only to describe what this mod works with.

It requires a legitimately purchased copy of the game. It contains no game code,
no extracted assets and no data files, and it touches no DRM or licence check.
It is single-player only and confers no multiplayer advantage.

The clip at the top is a short capture of ordinary gameplay, recorded to show
what the mod does. It remains Techland's copyright, ships in neither release
ZIP, and will be removed on request.
