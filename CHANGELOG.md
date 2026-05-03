# Changelog

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
