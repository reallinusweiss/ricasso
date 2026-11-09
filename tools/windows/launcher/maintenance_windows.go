//go:build windows

// SPDX-License-Identifier: GPL-3.0-or-later

// Update notice and uninstall.

package main

import (
	"context"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"
	"unsafe"

	"golang.org/x/sys/windows"

	"ricasso-launcher/importer"
	"ricasso-launcher/update"
)

const mbDefButton2 = 0x100

// ---------------------------------------------------------------- Update notice

func updateCacheFile() string {
	base := os.Getenv("LOCALAPPDATA")
	if base == "" {
		base = os.TempDir()
	}
	return filepath.Join(base, "Ricasso", "update-check.json")
}

// checkForUpdate asks GitHub at most once a day for the newest Ricasso release
// and shows a button on the ready page when it is newer than this package.
// It only reads the public release list; offline or on any error it stays
// silent. RICASSO_NO_UPDATE_CHECK=1 turns it off.
func (a *app) checkForUpdate() {
	defer func() { recover() }() // never take the starter down
	if os.Getenv("RICASSO_NO_UPDATE_CHECK") != "" {
		return
	}
	path := updateCacheFile()
	c := update.LoadCache(path)
	if !c.Fresh(time.Now(), 24*time.Hour) {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()
		client := &http.Client{Timeout: 10 * time.Second}
		rel, err := update.Fetch(ctx, client, update.ReleasesAPI, "Ricasso-Starter/"+launcherVersion)
		if err != nil {
			return
		}
		c = update.Cache{Checked: time.Now(), Latest: rel}
		if os.MkdirAll(filepath.Dir(path), 0o755) == nil {
			update.SaveCache(path, c)
		}
	}
	if !update.Newer(c.Latest.Version, releaseVersion) {
		return
	}
	a.mu.Lock()
	a.newRelease = c.Latest
	a.mu.Unlock()
	postMessage(a.hwnd, msgUpdateFound, 0, 0)
}

func (a *app) openRelease() {
	a.mu.Lock()
	u := a.newRelease.URL
	a.mu.Unlock()
	shellOpen(update.SafeURL(u), "")
}

// ---------------------------------------------------------------- Uninstall

func savesDir() string {
	return filepath.Join(os.Getenv("APPDATA"), "ricasso")
}

// uninstall removes the shortcuts that open this copy, the client folder app/
// (with the game files set up from the player's own copy), README.txt and,
// after the starter has exited, Ricasso.exe and the then empty folder.
// Saves in %APPDATA%\ricasso and the player's own Swordigo file stay.
func (a *app) uninstall() {
	if a.busy {
		return
	}
	root := a.exeDir
	if !samePath(a.appDir, filepath.Join(root, "app")) || !importer.LooksLikeInstall(root) {
		messageBox(a.hwnd, "Uninstall", "This folder doesn't look like a complete Ricasso folder, so nothing was removed. "+
			"You can delete the folder yourself:\n\n"+root, mbIconWarning)
		return
	}
	text := "This removes Ricasso from this PC:\n\n" +
		"• the “Swordigo” shortcuts on the desktop and in the Start menu that open this copy\n" +
		"• Ricasso and the game files it set up, in\n   " + root + "\n\n" +
		"Kept: your saves in " + savesDir() + " and your own Swordigo file.\n\nUninstall now?"
	if messageBox(a.hwnd, "Uninstall Ricasso", text, mbYesNo|mbIconWarning|mbDefButton2) != idYes {
		return
	}

	for _, folder := range []*windows.KNOWNFOLDERID{windows.FOLDERID_Desktop, windows.FOLDERID_Programs} {
		dir, err := windows.KnownFolderPath(folder, 0)
		if err != nil {
			continue
		}
		lnk := filepath.Join(dir, "Swordigo.lnk")
		if target, err := shortcutTarget(lnk); err == nil && samePath(target, a.exePath) {
			os.Remove(lnk)
		}
	}
	if data, err := os.ReadFile(lastInstallFile()); err == nil && samePath(strings.TrimSpace(string(data)), a.appDir) {
		os.Remove(lastInstallFile())
	}

	if err := importer.RemoveInstallation(root); err != nil {
		messageBox(a.hwnd, "Uninstall incomplete", "Some files could not be removed:\n\n"+err.Error()+
			"\n\nClose Swordigo if it is still running and try again, or delete this folder yourself:\n\n"+root, mbIconWarning)
		a.refresh()
		return
	}

	done := "Ricasso has been removed."
	removeDir := removableFolder(root)
	canDelete := canScheduleDelete(a.exePath, root)
	if !canDelete {
		done += "\n\nPlease delete the rest of this folder yourself:\n" + root
	} else if removeDir {
		done += "\n\nRicasso.exe and its folder are deleted a few seconds after you click OK."
	} else {
		done += "\n\nRicasso.exe is deleted a few seconds after you click OK. The folder itself stays:\n" + root
	}
	done += "\n\nYour saves are still here:\n" + savesDir()
	messageBox(a.hwnd, "Ricasso removed", done, mbIconInfo)

	if canDelete && !scheduleDelete(a.exePath, root, removeDir) {
		messageBox(a.hwnd, "Ricasso", "Ricasso.exe could not delete itself. Please delete it yourself:\n\n"+a.exePath, mbIconWarning)
	}
	if a.mutex != 0 {
		windows.CloseHandle(a.mutex)
		a.mutex = 0
	}
	call(pDestroyWindow, a.hwnd)
}

// shortcutTarget reads the program a .lnk file starts (IShellLinkW::GetPath).
func shortcutTarget(lnk string) (string, error) {
	if !fileExists(lnk) {
		return "", os.ErrNotExist
	}
	var link *comObject
	hr := call(pCoCreateInstance, uintptr(unsafe.Pointer(&clsidShellLink)), 0, 1, uintptr(unsafe.Pointer(&iidShellLinkW)), uintptr(unsafe.Pointer(&link)))
	if int32(hr) < 0 || link == nil {
		return "", syscall.Errno(hr)
	}
	defer link.call(2) // Release
	var file *comObject
	hr = link.call(0, uintptr(unsafe.Pointer(&iidPersistFile)), uintptr(unsafe.Pointer(&file))) // QueryInterface
	if int32(hr) < 0 || file == nil {
		return "", syscall.Errno(hr)
	}
	defer file.call(2)
	if hr = file.call(5, uintptr(unsafe.Pointer(u16(lnk))), 0); int32(hr) < 0 { // Load, STGM_READ
		return "", syscall.Errno(hr)
	}
	buf := make([]uint16, windows.MAX_LONG_PATH)
	if hr = link.call(3, uintptr(unsafe.Pointer(&buf[0])), uintptr(len(buf)), 0, 0); int32(hr) < 0 { // GetPath
		return "", syscall.Errno(hr)
	}
	return windows.UTF16ToString(buf), nil
}

// removableFolder reports whether the (then empty) package folder may be
// removed: never a drive root or one of the user's standard folders, in case
// Ricasso was unpacked straight into the desktop or downloads.
func removableFolder(dir string) bool {
	dir = filepath.Clean(dir)
	if filepath.Dir(dir) == dir || strings.EqualFold(dir, filepath.VolumeName(dir)+`\`) {
		return false
	}
	for _, id := range []*windows.KNOWNFOLDERID{
		windows.FOLDERID_Desktop, windows.FOLDERID_Documents, windows.FOLDERID_Downloads,
		windows.FOLDERID_Profile, windows.FOLDERID_LocalAppData, windows.FOLDERID_RoamingAppData,
		windows.FOLDERID_ProgramFiles, windows.FOLDERID_UserProgramFiles, windows.FOLDERID_Programs,
	} {
		if p, err := windows.KnownFolderPath(id, 0); err == nil && samePath(p, dir) {
			return false
		}
	}
	for _, env := range []string{"OneDrive", "USERPROFILE", "PUBLIC", "SystemRoot"} {
		if p := os.Getenv(env); p != "" && samePath(p, dir) {
			return false
		}
	}
	return true
}

// canScheduleDelete rejects paths that cmd.exe would expand or misquote.
func canScheduleDelete(exe, dir string) bool {
	return !strings.ContainsAny(exe+dir, `%"!`)
}

// deleteScript is the cmd.exe line that removes Ricasso.exe once the starter
// has exited: it retries every second for up to 30 seconds while the file is
// still locked (slow exit, virus scanner), then removes the folder if it is
// empty ("rd" without /s never deletes anything else).
func deleteScript(exe, dir string, removeDir bool) string {
	q := `"` + exe + `"`
	script := `(for /l %i in (1,1,30) do @(if exist ` + q + ` (del /f /q ` + q + ` >nul 2>&1 & if exist ` + q + ` ping -n 2 127.0.0.1 >nul)))`
	if removeDir {
		script += ` & rd "` + dir + `" 2>nul`
	}
	return `cmd.exe /d /c "` + script + `"`
}

// scheduleDelete starts the hidden cleanup, because a running program can't
// delete itself. It first tries to leave a job object the starter may run in
// (some launchers end all child processes together with the parent).
func scheduleDelete(exe, dir string, removeDir bool) bool {
	cmdExe := filepath.Join(os.Getenv("SystemRoot"), "System32", "cmd.exe")
	for _, flags := range []uint32{windows.CREATE_NO_WINDOW | windows.CREATE_BREAKAWAY_FROM_JOB, windows.CREATE_NO_WINDOW} {
		cmd := exec.Command(cmdExe)
		cmd.Dir = filepath.Dir(dir) // not inside the folder that is being removed
		cmd.SysProcAttr = &syscall.SysProcAttr{
			CmdLine:       deleteScript(exe, dir, removeDir),
			HideWindow:    true,
			CreationFlags: flags,
		}
		if cmd.Start() == nil {
			return true
		}
	}
	return false
}
