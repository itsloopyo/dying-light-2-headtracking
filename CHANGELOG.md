# Changelog

## [Unreleased]

### Changed

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
