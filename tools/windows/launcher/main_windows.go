//go:build windows

// SPDX-License-Identifier: GPL-3.0-or-later

// Ricasso.exe – sets up and starts Swordigo through Ricasso without a terminal.
//
// License: GPL-3.0-or-later (like the SwordigoDesktop launcher/frontend).
// Contains no game files and never downloads any; it only looks up the newest
// Ricasso release on GitHub (at most once a day).
package main

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"runtime/debug"
	"strings"
	"unsafe"

	"golang.org/x/sys/windows"

	"ricasso-launcher/importer"
)

const launcherVersion = "1.3.1"

// releaseVersion is the Ricasso package version (GitHub release); the update
// notice compares it with the newest release. Raise it for every release.
const releaseVersion = "2.0.0"

func init() {
	// All windows belong to the main thread.
	runtime.LockOSThread()
}

func main() {
	// Load DLLs from System32 (or WinSxS) only – protection in case foreign DLLs
	// sit next to Ricasso.exe in the Downloads folder.
	windows.SetDefaultDllDirectories(windows.LOAD_LIBRARY_SEARCH_SYSTEM32)
	defer func() {
		if r := recover(); r != nil {
			messageBox(0, "Ricasso – internal error", fmt.Sprintf("%v\n\n%s", r, debug.Stack()), mbIconError)
		}
	}()

	exe, err := os.Executable()
	if err != nil {
		messageBox(0, "Ricasso", "Unknown program path: "+err.Error(), mbIconError)
		return
	}
	exeDir := filepath.Dir(exe)

	// For automated package checks (verify_package.py): no window.
	if len(os.Args) > 1 && os.Args[1] == "--selftest" {
		os.Exit(selfTest(exeDir))
	}

	if runningFromTemp(exeDir) {
		messageBox(0, "Please extract the ZIP first",
			"Ricasso was started directly from inside the ZIP file. It can't work like that.\n\n"+
				"1. Close this window.\n"+
				"2. Right-click the ZIP file → “Extract All…” → “Extract”.\n"+
				"3. Open the extracted folder and start Ricasso.exe there.", mbIconInfo)
		return
	}
	appDir := findAppDir(exeDir)
	if appDir == "" {
		messageBox(0, "Program files missing",
			"The folder “app” with the program files is missing next to Ricasso.exe.\n\n"+
				"Please extract the complete ZIP file (right-click → “Extract All…”) and start Ricasso.exe in the extracted folder. "+
				"Don't move Ricasso.exe on its own – create a shortcut instead.", mbIconError)
		return
	}

	// Only one starter at a time.
	mutex, err := windows.CreateMutex(nil, false, u16(`Local\RicassoStarter`))
	if err == windows.ERROR_ALREADY_EXISTS {
		if hwnd := call(pFindWindowW, uintptr(unsafe.Pointer(u16(className))), 0); hwnd != 0 && call(pIsWindowVisible, hwnd) != 0 {
			call(pShowWindow, hwnd, swRestore)
			call(pSetForegroundWindow, hwnd)
		} else {
			messageBox(0, "Ricasso", "Ricasso is already running.", mbIconInfo)
		}
		return
	}
	defer func() {
		if mutex != 0 {
			windows.CloseHandle(mutex)
		}
	}()

	windows.CoInitializeEx(0, windows.COINIT_APARTMENTTHREADED)
	icc := initCommonControlsEx{ICC: iccStandardClass | iccProgressClass | iccLinkClass}
	icc.Size = uint32(unsafe.Sizeof(icc))
	call(pInitCommonControlsEx, uintptr(unsafe.Pointer(&icc)))

	importer.FreeSpace = func(dir string) (uint64, error) {
		var free uint64
		err := windows.GetDiskFreeSpaceEx(u16(dir), &free, nil, nil)
		return free, err
	}

	a := &app{exePath: exe, exeDir: exeDir, appDir: appDir, extra: os.Args[1:], mutex: mutex}
	theApp = a
	if err := a.createWindow(); err != nil {
		messageBox(0, "Ricasso", "The window could not be opened: "+err.Error(), mbIconError)
		return
	}
	a.run()
	mutex = a.mutex
}

// selfTest checks without a window whether the package is complete.
// Exit codes: 0 = complete, 3 = folder "app" missing, 4 = program files missing.
func selfTest(exeDir string) int {
	fmt.Println("Ricasso starter", launcherVersion)
	appDir := findAppDir(exeDir)
	if appDir == "" {
		fmt.Println("ERROR: folder app with swordfare.exe is missing")
		return 3
	}
	fmt.Println("Program folder:", appDir)
	if missing := importer.MissingClientFiles(appDir); len(missing) > 0 {
		fmt.Println("ERROR: missing program files:", strings.Join(missing, ", "))
		return 4
	}
	st := importer.DetectInstalled(appDir)
	if st.OK {
		fmt.Println("Game files: Swordigo", st.Version)
	} else {
		fmt.Println("Game files:", st.Problem)
	}
	fmt.Println("OK")
	return 0
}

// findAppDir looks for the folder with swordfare.exe: normally "app" next to
// Ricasso.exe, alternatively the same folder (older package layout).
func findAppDir(exeDir string) string {
	for _, dir := range []string{filepath.Join(exeDir, "app"), exeDir} {
		if fileExists(filepath.Join(dir, "swordfare.exe")) {
			return dir
		}
	}
	return ""
}

// runningFromTemp detects a start straight out of a ZIP file: Windows and
// archivers then extract into a temporary folder.
func runningFromTemp(dir string) bool {
	lower := strings.ToLower(filepath.Clean(dir)) + `\`
	for _, t := range []string{os.Getenv("TEMP"), os.Getenv("TMP"), os.TempDir()} {
		if t == "" {
			continue
		}
		if strings.HasPrefix(lower, strings.ToLower(filepath.Clean(t))+`\`) {
			return true
		}
	}
	for _, marker := range []string{`\temp1_`, `\rar$`, `\7zo`, `\wz`} {
		if strings.Contains(lower, marker) && strings.Contains(lower, `\temp\`) {
			return true
		}
	}
	return false
}
