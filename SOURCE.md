# Source and dependency notices

Ricasso is a reduced snapshot of SwordigoDesktop with Windows compatibility changes: the host client, ARM64 SRE sources, focused tests, vendored source dependencies and Windows build/package scripts. The original history and the complete project are in the upstream repository: https://github.com/TheAevoraLabs/SwordigoDesktop

Baseline: https://github.com/TheAevoraLabs/SwordigoDesktop/commit/072db6cc335c1c0bad41b965986738eb3840ab6d

Build instructions: WINDOWS_CLIENT.md and tools/windows/build.ps1. Exact Windows toolchain/package versions and SHA-256 hashes are in tools/windows/packages.lock.json. Game APK/APKM packages, original game libraries and extracted game data are not supplied by this contribution.

Every release is built from a tagged commit of this repository, and the release text names that commit.

Bundled runtime dependencies retain their own license terms. Their packaged notices are under licenses/dependencies and licenses/vendored. THIRD_PARTY_SOURCES.md links the exact source package of every bundled DLL; recipes are at https://github.com/msys2/MINGW-packages .

No game data is included.

Starter `Ricasso.exe` (no-terminal setup wizard and launcher): source in `tools/windows/launcher/` (Go, GPL-3.0-or-later). Its only dependency, `golang.org/x/sys` v0.30.0 (BSD-3-Clause), is vendored there, so the corresponding source is complete. The icon is original artwork. The starter contains no game data and downloads nothing.

Modifications in this tree (2026-10-31): images taken from the game were removed from `src/assets/`, `res/` and `web/assets/` and replaced by original artwork generated with `tools/windows/make_original_assets.ps1`; `src/main.cpp` no longer reports in-app purchases as owned (every product, including "Remove Ads", now counts as not purchased; ads are treated as unavailable); the archived `old/` source snapshots were removed.

This repository is a reduced snapshot (2026-11-01): it keeps only the files the Windows client build, its tests, packaging and the starter use (determined from the build graph), plus license notices. The editor suite (`src/ruby/`), research tools, documentation, website and Linux/Android packaging of upstream SwordigoDesktop are not included; they remain available upstream. `src/imgui/LICENSE.txt` is the Dear ImGui MIT license, added next to the vendored sources.

Ricasso (2026-11-06) is the project name of this reduced Windows distribution. User data lives in `%APPDATA%\ricasso` and logs in `%LOCALAPPDATA%\Ricasso`. The client binary (`swordfare.exe`) and the upstream notices keep their names.

Client reduction (2026-11-01): the in-game Memory Research subsystem (`src/game/research/`, including its embedded recovery database and memory watchpoints), the scene tooling in `src/tools/` (scene loader, creator, program, schemas, physics, collision, entity, game, terrain, workspace, template sources, v3 biome database, file-format library and terrain generator), the unused SCL parser and the OpenSwordigo host mode were removed from the source tree and the build. The `filerift` library now only carries the host Lua. Hero/scene pointer discovery for the mod tools is kept in `src/main.cpp`.
