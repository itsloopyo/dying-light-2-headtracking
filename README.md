# Dying Light 2 Head Tracking

![Mod GIF](https://raw.githubusercontent.com/itsloopyo/dying-light-2-headtracking/main/assets/readme-clip.gif)

An **unofficial** 6DOF head tracking mod for Dying Light 2 Stay Human using OpenTrack-compatible trackers.

Lean into a window frame while your crosshair stays on the zombie you were already aiming at. Glance down at a rooftop gap mid-parkour without your jump going with your eyes. Your head drives the camera; the mouse still drives the aim.

## Features

- **Decoupled look + aim**: Look around freely with your head while your aim stays independent
- **6DOF head tracking**: Full rotation (yaw, pitch, roll) and positional tracking (X, Y, Z) via OpenTrack UDP protocol

## Requirements

- Dying Light 2 Stay Human v1.16 or later
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) - bundled with the
  installer download and set up by `install.cmd`. The Nexus download does not
  include it; install it yourself first (see Manual Installation).
- [OpenTrack](https://github.com/opentrack/opentrack) or a compatible head tracking app (smartphone, webcam, or dedicated hardware)

## Installation

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
2. Copy `HeadTracking.ini` to the same directory (optional - defaults will be created)

### Finding Your Game Directory

- **Steam:** Right-click Dying Light 2 > Manage > Browse local files > navigate to `ph/work/bin/x64/`
- **Epic Games:** `C:\Program Files\Epic Games\DyingLight2\ph\work\bin\x64\`
- **GOG:** `C:\GOG Games\Dying Light 2\ph\work\bin\x64\`

## Setting Up OpenTrack

1. Download and install [OpenTrack](https://github.com/opentrack/opentrack/releases)
2. Configure your tracker (Input):
   - For webcam: Select "neuralnet tracker"
   - For phone app: Select "UDP over network"
3. Configure output:
   - Select **UDP over network**
   - Host: `127.0.0.1`
   - Port: `4242`
4. Click **Start** to begin tracking
5. Launch Dying Light 2

### Phone App Setup

This mod smooths remote connections by default (`RemoteSmoothing=0.15`) to handle network jitter, so you can send directly from your phone to the mod on port 4242 without needing OpenTrack on PC. Lower it if your link is clean, or raise it if the view still shakes.

1. Install an OpenTrack-compatible head tracking app from your phone's app store
2. Configure your phone app to send to your PC's IP address on port 4242 (run `ipconfig` to find it, e.g. `192.168.1.100`)
3. Set the protocol to OpenTrack/UDP
4. Start tracking

**With OpenTrack (optional):** If you want curve mapping or visual preview, route through OpenTrack by setting its Input to "UDP over network" and Output to port 4242.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord          |
|---------------------|-------------|----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y` |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G` |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H` |
| Toggle reticle      | `Insert`    | `Ctrl+Shift+U` |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

### Yaw mode

- **Camera-local** (default): yaw rotates around your head's up axis. When
  you pitch the view down and yaw, the horizon tilts and the view sweeps a
  cone. Matches traditional head-tracked FPS feel.
- **World-locked**: yaw rotates around the world's vertical axis. Pitch
  down and yaw - the horizon stays level and the view sweeps horizontally.
  Preferable if you find the default disorienting.

Your choice is remembered across sessions (written back to
`HeadTracking.ini`).

## Configuration

The mod creates `HeadTracking.ini` in the game directory with default settings on first run. Edit it to customize:

```ini
[Network]
UDPPort=4242              ; UDP port for tracker data

[Sensitivity]
YawMultiplier=1.0         ; Horizontal rotation sensitivity (0.1-5.0)
PitchMultiplier=1.0       ; Vertical rotation sensitivity (0.1-5.0)
RollMultiplier=1.0        ; Tilt sensitivity (0.0-2.0)

[Position]
SensitivityX=2.0          ; Position sensitivity per axis (0.1-10.0)
SensitivityY=2.0
SensitivityZ=2.0
LimitX=0.3                ; Max offset in meters per axis
LimitY=0.2
LimitZ=0.4
InvertX=false             ; Invert position axes
InvertY=false
InvertZ=false
Enabled=true              ; Enable/disable positional tracking (set false for 3DOF only)

[Smoothing]
; Chosen per connection from the tracker's source address; covers rotation and position
LocalSmoothing=0.0        ; Tracker on this machine, loopback (0.0-1.0)
RemoteSmoothing=0.15      ; Tracker on a remote network device, e.g. a phone (0.0-1.0)

[Hotkeys]
ToggleKey=0x23            ; End key (VK code in hex)
TrackingModeKey=0x21      ; Page Up key - cycles normal / rotation only / position only
YawModeKey=0x22           ; Page Down key - toggles camera-local / world-locked yaw
ReticleToggleKey=0x2d     ; Insert key (VK code in hex)

[Rotation]
WorldLockedYaw=false      ; false = camera-local (default), true = world-up yaw

[Reticle]
Enabled=true              ; Show/hide the head tracking reticle overlay

[General]
AutoEnable=true           ; Start tracking when game launches
ShowNotifications=true    ; Show on-screen status messages
```

## Troubleshooting

**Mod not loading:**
- Verify `winmm.dll` (ASI Loader) is in `ph/work/bin/x64/`
- Check that `DL2HeadTracking.asi` is in the same directory
- Check `<DL2 Install>/ph/work/bin/x64/HeadTracking.log` for error messages (the previous run is kept as `HeadTracking.prev.log`)

**No tracking response:**
- Ensure your tracker is running and outputting data
- Verify UDP port matches in both tracker and `HeadTracking.ini`
- Press `End` to enable tracking if auto-enable is off
- If the view sits off to one side, centre it in your tracker app (opentrack's
  Center bind, or the CENTER button in a Headcam app)

**Camera jittering:**
- Increase filtering in your tracker software
- Reduce sensitivity multipliers in `HeadTracking.ini`
- Improve lighting for webcam-based tracking

## Uninstalling

**Remove the mod only** - delete from `<DL2 Install>/ph/work/bin/x64/`:
- `DL2HeadTracking.asi`
- `HeadTracking.ini`
- `HeadTracking.log` and `HeadTracking.prev.log` (if present)

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

# Install only config file (for quick settings changes)
pixi run deploy-config
```

The install scripts automatically detect your game installation via Steam/registry.

### Project Structure

```
dying-light-2-headtracking/
├── src/
│   ├── core/           # Core mod logic
│   │   ├── mod.cpp     # Main mod class
│   │   ├── config.cpp  # Configuration handling
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
├── HeadTracking.ini    # Default configuration
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
| `deploy-config` | Deploy only config file |
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
- [EGameTools by EricPlayZ](https://github.com/EricPlayZ/EGameTools) - the reverse-engineering work this mod's camera signature comes from
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
