# Changelog

## [1.4.0] - 2026-08-20

### Added

- drop mod-side recentring, split smoothing into local and remote
- remove the splash skipper and its SkipSplash config option

## [Unreleased]

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from
  `Defaults.ini`, which every head tracking mod that keeps its settings in
  `CameraUnlock.ini` reads. Head tracking mods that keep their settings in
  another file do not read it, and neither do earlier versions of this mod.
  Writing a value in place of `default` changes that setting for this game
  only. When the mod saves a setting that a hotkey changed in game, it writes
  the new value in place of `default`, so that setting no longer follows
  `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows;
  `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or
  `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set,
  under Wine and Proton too; and
  `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's
  log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the
  built-in values, unless Windows runs the game as a packaged app. The mod never
  changes `Defaults.ini` after that.

### Changed

- Settings move to `ph\work\bin\x64\CameraUnlock.ini`. Earlier versions of the
  mod kept these settings in `HeadTracking.ini`, in the same folder. The first
  time this version starts and finds no `CameraUnlock.ini`, it reads your
  settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It
  never changes `HeadTracking.ini`, and does not read it again while
  `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as
  `default` when the value imported for it equals its default at that start,
  which is the value `Defaults.ini` gives it, or the built-in value where
  `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other
  setting is written with the value imported for it.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking
  mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor are these,
  where your old file had them:
  - A sensitivity, scale, deadzone, response curve or axis inversion you
    changed from its default. Set these in your tracker instead.
  - Reticle settings, and a key that toggled the reticle.
- An older version of the mod reads `HeadTracking.ini` and never reads
  `CameraUnlock.ini`, so a setting you change after updating is not in
  `HeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read
  `HeadTracking.ini` again. To go back to the defaults, replace everything in
  `CameraUnlock.ini` with the defaults the README shows. Every setting they set
  to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that
  triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`.
  The chords are ordinary entries now, so they can be rebound or removed. A
  hotkey code outside 0x01-0xFE in `HeadTracking.ini` imports as unbound.
- The tracking mode (Page Up) is saved to `CameraUnlock.ini` when you change
  it, and the game starts in it next time, as the yaw mode (Page Down) already
  was. End still changes the current session only: head tracking starts on or
  off as `EnableOnStartup` says.
- The old names import into the fleet's: `[Network] UDPPort` into `UdpPort`,
  `[General] AutoEnable` into `EnableOnStartup`, `[Rotation] WorldLockedYaw`
  into `WorldSpaceYaw`, `[Position] Enabled` into the tracking mode, and
  `[Position] LimitY` into both `PositionLimitY` and `PositionLimitYDown`,
  which can now be set apart. A position limit that is not a number imports as
  its default.
- `UdpPort` takes any port from 1 to 65535. Earlier versions replaced a port
  below 1024 with 4242.
- Installing, by `install.cmd` or through the launcher, no longer copies a
  default `HeadTracking.ini` over yours, and neither release ZIP carries a
  config: the mod creates `CameraUnlock.ini` at its first start. Uninstalling
  leaves `CameraUnlock.ini` and `HeadTracking.ini` in place, so your settings
  survive a reinstall.
- What v1.4.0 did with the same `HeadTracking.ini` that this version does
  differently, apart from the move above:
  - `[Position] LimitY` bounds lowering your head as well as raising it.
    v1.4.0 kept the downward lean at 0.20 m whatever `LimitY` said (948add0).
  - A start that finds no `HeadTracking.ini`, or cannot open it, runs
    world-locked yaw. v1.4.0 ran camera-local (0307ede). A file without
    `WorldLockedYaw` still imports as camera-local.
  - A value continued on an indented line ends at a `;` comment on that line,
    so a continued `true ; note` reads as true. v1.4.0 read it as false
    (b993f62, the r58 release of inih).

- Removed mod-side recentring. The `Home` key, the `Ctrl+Shift+T` chord and the
  `[Hotkeys] RecenterKey` INI entry are gone, and the mod now applies the
  tracker pose as absolute. Centre the view in your tracker app instead
  (opentrack's Center bind, or the CENTER button in a Headcam app); keeping a
  second centre in the mod drifted against the tracker's own.
- `HeadTracking.log` now keeps one previous generation as
  `HeadTracking.prev.log`, so a crash-then-relaunch no longer truncates away
  the log that recorded the crash.
- The UDP receiver's diagnostics (first packet received and its sender, bind
  retries, parse failures) are forwarded to `HeadTracking.log`, so a
  "no head tracking" report can be answered from the log alone.
- Smoothing is now two user-configurable INI keys in a new `[Smoothing]`
  section: `LocalSmoothing` (default `0.0`) for a tracker running on this
  machine (loopback) and `RemoteSmoothing` (default `0.15`) for a tracker on a
  remote network device. The value is selected per connection from the packet
  source address and re-evaluated every frame, so switching trackers needs no
  restart.
- Removed the `[Position] Smoothing` key and the hidden 0.15 baseline floor.
  The two new keys cover rotation and position, so local users get
  zero-latency tracking by default.
- World-locked yaw is now the default for new installs (`[Rotation]
  WorldLockedYaw=true`), matching the other head-tracking mods. An existing
  `HeadTracking.ini` keeps the mode it already has; Page Down switches it in
  game.

### Removed

- The key that toggled the reticle (`Insert`, `Ctrl+Shift+U`), and the reticle
  setting `[Reticle] Enabled`. The mod's aim dot is drawn whenever head
  tracking is on.
- The sensitivity and axis inversion settings: `YawMultiplier`,
  `PitchMultiplier` and `RollMultiplier` under `[Sensitivity]`, and
  `SensitivityX`, `SensitivityY`, `SensitivityZ`, `InvertX`, `InvertY` and
  `InvertZ` under `[Position]`. Set these in your tracker app instead.
- With these settings at their shipped defaults the camera moves as it did
  before: every release shipped the rotation multipliers at 1.0, the position
  sensitivities at 2.0 and the inversions off, and the mod applies a lean at
  twice the head's movement itself now.

### Fixed

- `HeadTracking.log` is opened and rotated before the mod waits for the engine
  DLL, not after. A renamed engine DLL, or the ASI loading into a launcher or
  wrapper process, previously produced no log at all and left the previous
  run's log in place, so the user sent an earlier launch's file believing it
  was the current one. The wait now records its outcome, including the
  10 second timeout.

## [1.3.0] - 2026-08-03

### Fixed

- show full control set in pixi install via shared -Controls

## [1.2.4] - 2026-06-07

### Other

- powershell: stop redirecting git stderr in Invoke-VersionCommit

## [1.2.2] - 2026-06-07

### Added

- add HeadTrackingSession and expand C++ core with RE Engine, Unreal, and tracking-session modules
- aim projection, reframework/unreal hooks, input/logging hardening, games
- add Mass Effect Legendary Edition to games catalog
- expand games catalog, fix unicode games.json read, stage launcher manifest
- add Pacific Drive to games catalog
- add Homeworld: Remastered Collection to games catalog
- add manifest-mode installer validator and ASI loader subdir support
- authenticate GitHub API requests via env token when present
- add R.E.P.O. detection data

### Fixed

- fail fast in ASI dev-deploy when the game is running
- restore il2cpp camera position by undoing applied local delta
- set SO_REUSEADDR so the receiver reclaims its port on relaunch

### Other

- Add Ubisoft Connect detection and VendorZip BepInEx install
- Add PluginSubfolder param to Invoke-DevDeployBepInEx
- Add Xbox install path for Easy Delivery Co
- Add GOG IDs for Cyberpunk 2077
- Add PLUGIN_SUBFOLDER support to BepInEx install/uninstall bodies
- scripts: drop the two-phase loader-init prompt from install bodies
- data: add Black & White (Lionhead) to games registry
- scripts: detect BepInEx 6 IL2CPP via BepInEx.Core.dll marker
- powershell: skip cameraunlock-core remote refresh in CI
- scripts: add UE4SS install template, fix delayed expansion in ASI body, expand games registry
- protocol: reject finite-but-out-of-float-range packet values
- data: add Subnautica 2 to games registry
- detection: add installer-registry game path lookup (Black & White GameDir)
- protocol: reorder tracking data member in udp_receiver
- data: fix Subnautica 2 Steam app id (3367150 -> 1962700)
- data: add Ni no Kuni Remastered and Yakuza 0; switch find-game output to UTF-8
- detection: add Xbox/GDK build support for Subnautica 2 (and any future GDK title)
- find-game: escape `&` in GAME_DISPLAY_NAME so echo doesn't split
- templates: add uninstall.ps1; data: add Deus Ex Mankind Divided
- powershell: add NightlyRelease module for Patreon-gated nightly builds
- protocol: disable SIO_UDP_CONNRESET and add one-shot receiver diagnostics; powershell: write nightly manifest.json without UTF-8 BOM; data: add Mixtape
- powershell: stop redirecting git stderr in Update-CameraUnlockCoreToRemoteTip
- powershell: publish dev builds as GitHub pre-releases
- protocol: disable SIO_UDP_CONNRESET and add one-shot receiver diagnostics
- data: add Mixtape
- powershell: stop redirecting git stderr in Update-CameraUnlockCoreToRemoteTip
- powershell: run gh under Continue so its stderr doesn't abort the dev-release publish
- reframework: strip VR runtime DLLs on install for flatscreen mode
- reframework: cache GetValue method and avoid per-call heap in ArrayGetValue; data: add BioShock Infinite
- uninstall: remove reframework_revision.txt marker dropped at game root
- install: render MOD_CONTROLS multi-line via percent expansion
- Add YAPYAP to games.json
- powershell: write state file BOM-less so Lopari JSON parser accepts it

## [1.2.1] - 2026-05-03

### Other

- Verify existing BepInEx loader arch and replace on mismatch
- Fall back to dev-tree vendor path in BepInEx install body

## [1.2.0] - 2026-05-03

### Added

- splash skipper, DX12 per-buffer allocators, ultrawide FOV fix

### Other

- Add DX11 overlay header for crosshair rendering
- Update PositionInterpolator tests for bounded extrapolation
- Skip vendor refresh when SHA-256 matches existing copy
- Fix degenerate-input bugs in scanners, projection, and color parser
- Add yaw-mode key and WorldSpaceYaw config options
- Quote /y flag detection and add shared install/uninstall bodies
- Add DevDeploy module with Cecil dev-install orchestrator
- Auto-refresh cameraunlock-core submodule in Copy-SharedBundle
- Add install bodies and dev-deploy orchestrators for non-Cecil frameworks
- Resolve exe relpath from games.json in ASI/shim dev-deploy
- Add automatic port retry to C++ UdpReceiver
- Take BuildOutputPath in dev-deploy and add loader/config auto-install

## [1.1.0] - 2026-04-30

### Added

- add Invoke-FetchLatestLoader and Refresh-VendoredLoader helpers
- cycle tracking mode (normal / rotation only / position only) on Page Up

### Other

- Wire update-deps task and refresh vendored ASI loader
- Add prediction-error correction to interpolators for smooth high-FPS output
- Port linear interpolation and quaternion SLERP smoothing from C# core
- Add gui_marker_compensation.h for RE Engine GUI world-anchor tracking
- Add REFramework utilities module (cameraunlock_reframework)
- Add velocity extrapolation to interpolators for smooth high-refresh output
- Gate UnityEngine.InputLegacyModule reference on file existence
- Fix batch paren-poisoning in install.cmd template
- Move game detection to data-driven games.json
- Fix install.cmd/uninstall.cmd templates for dev-tree use
- Unify installer CLI across BepInEx/MelonLoader/Cecil/ASI/REFramework/shim
- Make vendored loaders the install-time source of truth
- Add Step-SemanticVersion and Resolve-ReleaseVersion helpers
- Add camera discovery module (RTTI vtable + float classifier)
- Add AGENTS.md with shared code-quality and library API rules
- Expand submodule pointer commits in generated changelogs
- Fix /y flag detection and bundle vendored BepInEx in installers
- Use WriteAllBytes for .cmd output to avoid Defender race

## [1.0.5] - 2026-04-18

### Added

- add Page Down yaw-mode toggle and Ctrl+Shift chord hotkeys

### Fixed

- install.cmd works on Program Files (x86) paths
- bundle Ultimate ASI Loader and ship launcher-manifest.json in installer

### Other

- Rework crosshair projection to inline tangent-space math from head-tracked camera vectors

## [1.0.4] - 2026-03-13

### Other

- Use live engine FOV for crosshair projection instead of hardcoded default

## [1.0.3] - 2026-03-13

### Other

- Move vendored libs to extern/, add neck model, update metadata

## [1.0.2] - 2026-03-12

### Other

- Switch rotation model from spherical to horizon-locked sequential yaw/pitch/roll

## [1.0.1] - 2026-03-10

### Other

- Add position tracking toggle hotkey (Page Up)
- Add Nexus Mods release packaging
- Add reticle toggle (Insert key) and apply smoothing baseline to all connections

## [1.0.0] - 2026-03-08

First release.
