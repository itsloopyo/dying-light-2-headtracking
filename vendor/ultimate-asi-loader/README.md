# Ultimate ASI Loader (bundled)

This directory ships a pre-built copy of Ultimate ASI Loader so the DL2
Head Tracking installer does not depend on a GitHub download at install
time. Earlier versions curl'd the loader from upstream's release assets,
which broke when upstream retired the `x64-latest` rolling tag.

- **Upstream**: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- **Version**: v9.7.1 (released 2026-04-03)
- **Asset**: `Ultimate-ASI-Loader_x64.zip`
- **License**: MIT (see `LICENSE` next to this file)

`dinput8.dll` is the DL2-compatible hook DLL extracted from that asset
untouched. `install.cmd` copies it into `ph/work/bin/x64/` and renames it
to `winmm.dll` (the hook name DL2 will actually load).

## Updating

When a new upstream version is needed:

1. Download `Ultimate-ASI-Loader_x64.zip` from the desired upstream tag.
2. Extract `dinput8.dll`, overwrite the one here.
3. Re-fetch `license` from that same tag (upstream sometimes updates
   copyright year or wording) and overwrite `LICENSE` here.
4. Update the version line above.
5. Update the version line in `THIRD-PARTY-NOTICES.md` at the repo root.

Do not modify the DLL. If a patched build is ever needed, fork upstream
and bundle the fork's build here, with the patched source commit SHA
recorded in place of "v9.7.1".
