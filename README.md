# Swordigo for Windows (unofficial port)

Play **your own copy** of the Android game *Swordigo* on Windows 10/11 (x64). This is an unofficial fan
project. It is not made, endorsed or supported by Touch Foo, and it contains **no game files**.

## For players

1. Download `Swordigo-Windows-x64.zip` from the releases page and extract it (right-click → *Extract All…*).
2. Start `Swordigo.exe`.
3. Choose your own Swordigo 1.4.13 or 1.4.12 package (`.apkm`, `.apks`, `.xapk`, `.apk` or several split APKs),
   e.g. exported from your own phone, or drag it onto the window. The starter checks it, copies only what the
   game needs and offers desktop and Start-menu shortcuts.

No Python, no terminal and no installation are needed. Details and troubleshooting: [`WINDOWS_CLIENT.md`](WINDOWS_CLIENT.md)
and `README.txt` in the download.

Please don't download APK files from third-party websites, and don't share game files.

## What this port does and doesn't do

- The client (`swordfare.exe`) loads the original ARM64 game library from the player's own package and runs it
  through the Dynarmic JIT on x64.
- In-app purchases are **not** unlocked: every product counts as not purchased. There is no ad SDK on desktop,
  so ad requests are answered as "no ad available".
- The launcher/UI artwork in `src/assets/` is original (`tools/windows/make_original_assets.ps1`). No game
  artwork is included.

## Scope of this repository

This is a reduced snapshot of [SwordigoDesktop](https://github.com/TheAevoraLabs/SwordigoDesktop) by
AevoraLabs and the Lawncher Team. It contains only what the Windows client and its starter need:

- `src/`, `cmake/`, `include/`, `deps/dynarmic/`, `tests/`: the client, its runtime (SRE) and the client tests
- `tools/windows/`: toolchain bootstrap, build, packaging and verification scripts
- `tools/windows/launcher/`: the starter `Swordigo.exe` (Go)

The editor suite, research tools, documentation and website of the upstream project are not part of this
repository; see the upstream project for those.

## Building

Windows x64, Python 3.10+, 7-Zip and Go 1.22+:

```powershell
python tools/windows/bootstrap.py
powershell -ExecutionPolicy Bypass -File tools/windows/build.ps1
python tools/windows/verify_package.py
```

The package is written to `dist-windows/Swordigo-Windows-x64.zip`. See [`WINDOWS_CLIENT.md`](WINDOWS_CLIENT.md).

## License and credits

GPL-3.0 and MIT components as described in [`LICENSE.md`](LICENSE.md); the starter is GPL-3.0-or-later.
Credits: [`CREDITS.md`](CREDITS.md). Source and dependency notes: [`SOURCE.md`](SOURCE.md).

*Swordigo* is a trademark of Touch Foo. Swordigo © Touch Foo.
