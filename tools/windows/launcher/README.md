# Ricasso.exe – starter without a terminal

`Ricasso.exe` sits next to the `app/` folder of the Windows package and makes the client usable without PowerShell, Python or `Start-Swordigo.cmd`:

1. **Setup wizard (English UI).** The user picks or drags in their *own* Swordigo package (`.apkm`, `.apks`, `.xapk`, a single `.apk`, several split APKs, a folder of them, or an earlier installation). The wizard checks the package name, the version (SHA-256 of `lib/arm64-v8a/libswordigo.so`, same table as `import_apkm.py`) and `gamedata.gdata`. It then copies only the library, `assets/` and `res/raw/music_*` into `app/`. Staging goes through `app/.setup-tmp/`, so a failed import never damages a working setup. `game-files.json` records the hashes, and `Start-Swordigo.cmd` is patched to the installed version.
2. **Launcher.** It runs the same `--check-data` preflight and game command line as `Start-Swordigo.cmd`, with `SWORDIGO_DATA_DIR` set and stray `QT_*` variables removed. The game runs **without a console window**, and output goes to `%LOCALAPPDATA%\Ricasso\logs\latest.log` (the previous log is kept as `previous.log`). On a non-zero exit code it shows an explanation and the last 40 log lines.
3. **Convenience.** Desktop and Start-menu shortcuts (using the game's own launcher icon taken from the user's package, if present), buttons for saves and logs, and taking game files over from a previous installation (`%LOCALAPPDATA%\Ricasso\last-install.txt`). It also detects a start straight out of the ZIP, an incomplete extraction, a folder that isn't writable (it offers to copy itself to `%LOCALAPPDATA%\Programs\Ricasso`), paths that are too long, too little disk space and a second instance.

**Legal scope:** the starter contains no game data, downloads nothing and accepts only files the user supplies. The UI asks the user to confirm that it's their own lawfully obtained copy and not to share the game files. The icon is original artwork (a generic sword), not Swordigo branding.

## Build

Requires Go 1.22+. `golang.org/x/sys` is vendored (BSD license in `vendor/golang.org/x/sys/LICENSE`), so the build runs offline.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/windows/launcher/build_launcher.ps1
```

or, cross-compiling on Linux/macOS: `tools/windows/launcher/build_launcher.sh`. Output: `tools/windows/launcher/bin/Ricasso.exe` (ignored by git, like all `*.exe`). `tools/windows/package.py` puts it at the package root.

Icon, manifest (Common Controls v6, PerMonitorV2 DPI, `asInvoker`) and version info live in `rsrc_windows_amd64.syso`. It's generated from `winres/` (icon, manifest; version strings in the script) with `python winres/build_resources.py` – pure Python, no windres needed – and only needs regenerating after changing those files.

## Tests

* `go test ./importer` covers the formats, every error message, `.scene`/`.scene_res` renaming, path checks, re-installation, takeover, the path-length limit and the disk-space limit. It uses synthetic packages only.
* `go build -tags testhash` makes a **test-only** build that also accepts a synthetic library (see `importer/testhash.go`). It was used for GUI tests under Wine and must never be shipped.
* `Ricasso.exe --selftest` checks the package without opening a window (exit 0 = complete, 3 = `app/` missing, 4 = files missing). `verify_package.py` calls it.

## Layout

| File | Purpose |
| --- | --- |
| `main_windows.go` | Start checks (ZIP/temp, `app/`, single instance), self-test |
| `gui_windows.go` | Window, pages (setup, busy, ready, error, help), drag & drop, file dialog |
| `actions_windows.go` | Game start and log, exit-code texts, copy fallback, shortcuts (IShellLinkW) |
| `win32_windows.go` | Minimal Win32 bindings |
| `help_windows.go` | Help text "How do I get the file?" |
| `importer/` | Platform-neutral checking and installation (tested on Linux) |

Every other argument passed to `Ricasso.exe` is forwarded to `swordfare.exe`.

License: GPL-3.0-or-later (like the SwordigoDesktop launcher/frontend).
