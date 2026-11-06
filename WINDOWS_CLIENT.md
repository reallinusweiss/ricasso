# Ricasso: Windows x64 client for Swordigo

Setup, controls and build notes for the Windows client. Read README.md and CREDITS.md for origin and license information. This is an experimental client, primarily tested with **Swordigo 1.4.13 ARM64 on Windows 11 x64**.

## Quick start (no terminal)

The package root contains only `Ricasso.exe`, `README.txt` (a short quick start) and the folder `app/` with the client.

1. Extract the ZIP (right-click → *Extract All…*).
2. Double-click `Ricasso.exe`.
3. Pick your own Swordigo 1.4.13/1.4.12 package (`.apkm`, `.apks`, `.xapk`, `.apk`, several split APKs, a folder, or a previous installation) or drag it onto the window.

The starter checks package name, version (ARM64 library SHA-256) and `gamedata.gdata`, copies only the required files into `app/`, offers Desktop/Start-menu shortcuts and then launches the game without a console window. On a crash it shows an explanation plus the log tail. It contains no game data and downloads nothing. Details: `tools/windows/launcher/README.md` in the source archive.

The manual and Python routes below still work. Run them inside `app/`.

## Install from the Windows package

1. Extract the complete Windows client ZIP into a writable folder. Keep all DLLs, `platforms/`, `libs/`, `qt.conf` and notices together (they live in `app/`; the paths below are relative to `app/`).
2. Supply a matching Swordigo game package. Original files are not included and are never downloaded by the importer.
3. For a manual 1.4.13 setup, copy `lib/arm64-v8a/libswordigo.so` into `engine/v1.4.13/arm64-v8a/libswordigo.so`, the complete APK `assets/` tree into `assets/` and game music from `res/raw/music_*` into `res/raw/`.
4. Run `Start-Swordigo.cmd`. Missing data produces a diagnostic instead of starting an incomplete game.

The full game asset tree is required, including `assets/resources/gamedata.gdata` (or `assets/gamedata.gdata`). Do not mix assets and libraries from different versions. If an extractor cannot represent both `X.scene` and the directory `X.scene/`, use `X.scenebin` for the file and `X.scene_res/` for the directory. The client supports these names.

Expected 1.4.13 ARM64 library SHA-256:

```text
c2f567a34ec7bed9cf6b0af0ce88e9c925c11e5c2a2f2fbd8604461cf93aae7d
```

## Import your own APKM

The Windows ZIP includes a local importer. With Python 3.10+ installed, run from the extracted client folder:

```powershell
python tools/windows/import_apkm.py 'C:/path/to/Swordigo.apkm'
```

The importer checks the package identity, supported version and ARM64 library hash, then creates a separate `play-windows/Swordigo <version>/` folder. It preserves the clean client template, refuses to overwrite an existing installation and records original-file hashes in `game-files.json`. It supports the known 1.4.12 and 1.4.13 APKM layouts; 1.4.13 is the primary tested game.

From a source checkout, first build/package the client, then run the same command. Its `.tools/latest-package.txt` selects the built clean template. A plain APK can be installed manually as described above; this importer expects APKM split packages.

## Controls and branch scope

Default movement is A/D or arrow keys; jump is Space/W; attack and magic bindings are shown in the F2 editor. Existing personal bindings override defaults.

- **F1:** existing upstream client menu.
- **F2:** control editor; F2 saves/closes, R toggles key rebinding. Bindings persist after restarting.
- **F3:** existing upstream performance/debug display. The validated live XYZ reader is an extras feature.

Game Over accepts the original on-screen action and the supported keyboard inputs. Original game saves are separate from practice save/load-state features: **Save State and Load State are not implemented**.

**In-app purchases and ads.** The desktop client has no Google Play Billing and no ad SDK. Every in-app product (including "Remove Ads", the compass and the coin doubler) counts as not purchased; nothing paid is unlocked. Ad requests are answered as "no ad available": no ad is shown, and the death screen continues through the game's own continue path.

The client keeps the existing upstream menus. The in-game Memory Research console and the scene tooling of upstream SwordigoDesktop are not part of it. Existing upstream practice controls do not establish speedrun eligibility.

## Saves and logs

Game data is loaded from the package folder. User profiles and controls use `%APPDATA%/ricasso/`; game saves are under its `save/Documents/` tree. The starter writes `%LOCALAPPDATA%/Ricasso/logs/latest.log`. Personal saves, controls, traces and logs are not part of the release.

## Build from source

Requirements: Windows x64, Python 3.10+, Git, Windows `curl.exe`, 7-Zip and Go 1.22+ for the starter `Ricasso.exe` (`tools/windows/launcher/build_launcher.ps1`; `build.ps1` calls it). The tested toolchain is MinGW-w64 UCRT64 / GCC 16.2; dependencies are downloaded into `.tools/`, not installed globally. Original game data is unnecessary for building and the focused tests.

```powershell
python tools/windows/bootstrap.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/windows/build.ps1 -Python python
python tools/windows/verify_package.py
```

The build compiles the host, both ARM64 SREs and all six core client tests, runs CTest and produces `dist-windows/Ricasso-Windows-x64.zip`. `packages.lock.json` fixes package versions and SHA-256 hashes. If an old package is no longer available from the primary MSYS2 mirror, obtain that exact package from a trusted archive/cache; do not silently replace locked versions.

An existing compatible toolchain can be reused with `-ToolchainPrefix C:/path/to/ucrt64` on `build.ps1`. It changes only where build dependencies are read.

Editor tools and other platforms are not part of this repository; see upstream SwordigoDesktop for those.
