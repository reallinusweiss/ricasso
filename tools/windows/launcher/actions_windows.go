//go:build windows

// SPDX-License-Identifier: GPL-3.0-or-later

package main

import (
	"context"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"
	"unicode/utf8"
	"unsafe"

	"golang.org/x/sys/windows"

	"ricasso-launcher/importer"
)

func logFilePath() string {
	base := os.Getenv("LOCALAPPDATA")
	if base == "" {
		base = os.TempDir()
	}
	return filepath.Join(base, "SwordigoDesktop", "logs", "latest.log")
}

func lastInstallFile() string {
	base := os.Getenv("LOCALAPPDATA")
	if base == "" {
		base = os.TempDir()
	}
	return filepath.Join(base, "Ricasso", "last-install.txt")
}

func fileExists(p string) bool {
	info, err := os.Stat(p)
	return err == nil && info.Mode().IsRegular()
}

func dirExists(p string) bool {
	info, err := os.Stat(p)
	return err == nil && info.IsDir()
}

func samePath(a, b string) bool {
	return strings.EqualFold(filepath.Clean(a), filepath.Clean(b))
}

func writable(dir string) bool {
	f, err := os.CreateTemp(dir, ".writetest-*")
	if err != nil {
		return false
	}
	name := f.Name()
	f.Close()
	os.Remove(name)
	return true
}

func shellOpen(file, params string) {
	var p *uint16
	if params != "" {
		p = u16(params)
	}
	windows.ShellExecute(0, u16("open"), u16(file), p, nil, windows.SW_SHOWNORMAL)
}

// gameEnv matches Start-Swordigo.cmd and removes Qt variables that other
// programs set system-wide and that could disturb the client.
func gameEnv(appDir string) []string {
	skip := map[string]bool{"SWORDIGO_DATA_DIR": true, "QT_PLUGIN_PATH": true, "QT_QPA_PLATFORM_PLUGIN_PATH": true, "QT_QPA_PLATFORM": true}
	var env []string
	for _, kv := range os.Environ() {
		k, _, _ := strings.Cut(kv, "=")
		if k != "" && skip[strings.ToUpper(k)] {
			continue
		}
		env = append(env, kv)
	}
	return append(env, "SWORDIGO_DATA_DIR="+strings.TrimRight(appDir, `\/`)+`\`)
}

func (a *app) runGame(st importer.Installed) {
	exe := filepath.Join(a.appDir, "swordfare.exe")
	logPath := logFilePath()
	os.MkdirAll(filepath.Dir(logPath), 0o755)
	if fileExists(logPath) {
		os.Rename(logPath, filepath.Join(filepath.Dir(logPath), "previous.log"))
	}
	logFile, err := os.Create(logPath)
	if err != nil {
		logPath = filepath.Join(filepath.Dir(logPath), fmt.Sprintf("latest-%d.log", os.Getpid()))
		logFile, err = os.Create(logPath)
	}
	var out io.Writer = io.Discard
	if err == nil {
		defer logFile.Close()
		out = logFile
	} else {
		logPath = ""
	}
	a.mu.Lock()
	a.logPath = logPath
	a.mu.Unlock()
	fmt.Fprintf(out, "[Starter] Ricasso starter %s · %s\r\n[Starter] Program folder: %s\r\n[Starter] Game version: %s\r\n",
		launcherVersion, time.Now().Format("2006-01-02 15:04:05"), a.appDir, st.Version)

	fail := func(f gameFailure) {
		fmt.Fprintf(out, "[Starter] %s\r\n", f.title)
		a.mu.Lock()
		a.failure = f
		a.mu.Unlock()
		postMessage(a.hwnd, msgGameFailed, 0, 0)
	}
	newCmd := func(ctx context.Context, args []string) *exec.Cmd {
		var c *exec.Cmd
		if ctx != nil {
			c = exec.CommandContext(ctx, exe, args...)
		} else {
			c = exec.Command(exe, args...)
		}
		c.Dir = a.appDir
		c.Env = gameEnv(a.appDir)
		if logFile != nil && logPath != "" {
			c.Stdout, c.Stderr = logFile, logFile
		}
		c.SysProcAttr = &syscall.SysProcAttr{HideWindow: true, CreationFlags: createNoWindow}
		return c
	}

	// 1. Preflight check as in Start-Swordigo.cmd
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
	defer cancel()
	if err := newCmd(ctx, importer.CheckArgs(st)).Run(); err != nil {
		var ee *exec.ExitError
		if errors.As(err, &ee) {
			code := uint32(ee.ExitCode())
			if code == 2 {
				fail(gameFailure{title: "The game files are incomplete or don't match.",
					hint: "The client found a problem while checking (details below). Set up the game files again: " +
						"“Back” → “Set up again…” and choose your Swordigo file once more.", showLog: true})
				return
			}
			title, hint := describeExit(code)
			fail(gameFailure{title: title, hint: hint, showLog: true, retry: true})
			return
		}
		title, hint := describeStartError(err)
		fail(gameFailure{title: title, hint: hint, showLog: true})
		return
	}

	// 2. Start the game
	args := append(importer.GameArgs(st), a.extra...)
	fmt.Fprintf(out, "[Starter] Starting: swordfare.exe %s\r\n", strings.Join(args, " "))
	game := newCmd(nil, args)
	if err := game.Start(); err != nil {
		title, hint := describeStartError(err)
		fail(gameFailure{title: title, hint: hint, showLog: true})
		return
	}
	postMessage(a.hwnd, msgGameStarted, 0, 0)
	game.Wait()
	code := uint32(0)
	if game.ProcessState != nil {
		code = uint32(game.ProcessState.ExitCode())
	}
	fmt.Fprintf(out, "[Starter] Swordigo exited, code %d (0x%08X)\r\n", code, code)
	postMessage(a.hwnd, msgGameExited, uintptr(code), 0)
}

const commonCauses = "Common causes: an outdated graphics driver (install the current driver from AMD, NVIDIA or Intel), " +
	"an antivirus program blocking swordfare.exe, or damaged game files (“Back” → “Set up again…”). " +
	"If you ask for help, include the log."

func describeExit(code uint32) (string, string) {
	switch code {
	case 0xC0000135:
		return "A program file (DLL) is missing.", "Extract the ZIP file again completely and check whether your antivirus program removed any files."
	case 0xC000007B:
		return "A program file is damaged or doesn't match.", "Extract the ZIP file again completely into a new folder."
	case 0xC0000017, 0xC000012D:
		return "Not enough memory.", "Close other programs and try again."
	case 0xC0000005:
		return "Swordigo crashed (access violation).", commonCauses
	case 0xC0000409:
		return "Swordigo was stopped because of an internal error.", commonCauses
	}
	if code > 0xFFFF {
		return fmt.Sprintf("Swordigo quit unexpectedly (error code 0x%08X).", code), commonCauses
	}
	return fmt.Sprintf("Swordigo quit with error code %d.", code), commonCauses
}

func describeStartError(err error) (string, string) {
	var errno syscall.Errno
	if errors.As(err, &errno) {
		switch errno {
		case 2, 3:
			return "swordfare.exe was not found.", "Extract the ZIP file again completely. Also check your antivirus quarantine."
		case 5:
			return "Windows refused to start the program (access denied).", "Often an antivirus program blocks it. Add the Ricasso folder as an exclusion there."
		case 225, 226:
			return "Your antivirus program blocked swordfare.exe.",
				"The client translates ARM code at runtime; some antivirus programs wrongly consider that suspicious. " +
					"Restore the file from quarantine and add the Ricasso folder as an exclusion – only if you trust the source of Ricasso."
		case 4551, 1260:
			return "Windows is blocking the program (app control).",
				"Windows 11 “Smart App Control” or a company policy doesn't allow unsigned programs. " +
					"You can change it under Windows Security → App & browser control."
		case 193, 216:
			return "swordfare.exe is damaged.", "Extract the ZIP file again completely into a new folder."
		}
	}
	return "Swordigo could not be started.", err.Error()
}

// tailFile returns the last lines of a (UTF-8) text file for display.
func tailFile(path string, lines int) string {
	if path == "" {
		return "(No log available.)"
	}
	f, err := os.Open(path)
	if err != nil {
		return "(The log could not be read: " + err.Error() + ")"
	}
	defer f.Close()
	const max = 64 << 10
	info, _ := f.Stat()
	if info != nil && info.Size() > max {
		f.Seek(-max, io.SeekEnd)
	}
	data, _ := io.ReadAll(f)
	text := strings.ToValidUTF8(string(data), "?")
	if !utf8.ValidString(text) {
		text = "(Log not readable)"
	}
	text = strings.ReplaceAll(text, "\r\n", "\n")
	all := strings.Split(strings.TrimRight(text, "\n"), "\n")
	if len(all) > lines {
		all = all[len(all)-lines:]
	}
	return strings.Join(all, "\r\n")
}

// ---------------------------------------------------------------- Startup check

func (a *app) startupChecks() {
	if writable(a.appDir) {
		return
	}
	target := filepath.Join(os.Getenv("LOCALAPPDATA"), "Programs", "Ricasso")
	text := "Ricasso is not allowed to write to this folder:\n\n" + a.exeDir +
		"\n\nThis happens, for example, in the “Program Files” folder. Ricasso needs a folder it can copy your game files into." +
		"\n\nCopy Ricasso here now and start it from there?\n\n" + target
	if messageBox(a.hwnd, "Different location needed", text, mbYesNo|mbIconQuestion) != idYes {
		return
	}
	a.copyTarget = target
	a.setBusy("Copying Ricasso…", target)
	go func() {
		err := copyTree(a.exeDir, target, a.reportProgress)
		a.mu.Lock()
		a.copyErr = err
		a.mu.Unlock()
		postMessage(a.hwnd, msgCopyDone, 0, 0)
	}()
}

func (a *app) onCopyDone() {
	a.busy = false
	a.mu.Lock()
	err := a.copyErr
	a.mu.Unlock()
	if err != nil {
		a.refresh()
		messageBox(a.hwnd, "Copying failed", err.Error(), mbIconError)
		return
	}
	if a.mutex != 0 {
		windows.CloseHandle(a.mutex)
		a.mutex = 0
	}
	exe := filepath.Join(a.copyTarget, filepath.Base(a.exePath))
	cmd := exec.Command(exe)
	cmd.Dir = a.copyTarget
	if err := cmd.Start(); err != nil {
		messageBox(a.hwnd, "Ricasso", "Copied to:\n\n"+a.copyTarget+"\n\nPlease start Ricasso.exe there.", mbIconInfo)
	}
	call(pDestroyWindow, a.hwnd)
}

func copyTree(src, dst string, progress importer.Progress) error {
	var total, done int64
	filepath.WalkDir(src, func(p string, d fs.DirEntry, err error) error {
		if err == nil && !d.IsDir() {
			if info, err := d.Info(); err == nil {
				total += info.Size()
			}
		}
		return nil
	})
	return filepath.WalkDir(src, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		rel, err := filepath.Rel(src, p)
		if err != nil {
			return err
		}
		if d.IsDir() && strings.HasPrefix(d.Name(), importer.StagingDir) {
			return filepath.SkipDir
		}
		target := filepath.Join(dst, rel)
		if d.IsDir() {
			return os.MkdirAll(target, 0o755)
		}
		in, err := os.Open(p)
		if err != nil {
			return err
		}
		defer in.Close()
		out, err := os.Create(target)
		if err != nil {
			return err
		}
		n, err := io.Copy(out, in)
		if cerr := out.Close(); err == nil {
			err = cerr
		}
		done += n
		progress("Copying “"+rel+"”…", done, total)
		return err
	})
}

// ---------------------------------------------------------------- Shortcuts

var (
	clsidShellLink = windows.GUID{Data1: 0x00021401, Data4: [8]byte{0xC0, 0, 0, 0, 0, 0, 0, 0x46}}
	iidShellLinkW  = windows.GUID{Data1: 0x000214F9, Data4: [8]byte{0xC0, 0, 0, 0, 0, 0, 0, 0x46}}
	iidPersistFile = windows.GUID{Data1: 0x0000010B, Data4: [8]byte{0xC0, 0, 0, 0, 0, 0, 0, 0x46}}
)

type comObject struct{ vtbl *[32]uintptr }

//go:uintptrescapes
func (o *comObject) call(index int, args ...uintptr) uintptr {
	r, _, _ := syscall.SyscallN(o.vtbl[index], append([]uintptr{uintptr(unsafe.Pointer(o))}, args...)...)
	return r
}

// createShortcut creates a .lnk file via IShellLinkW/IPersistFile.
func createShortcut(lnk, target, workDir, desc, icon string, iconIndex int) error {
	var link *comObject
	hr := call(pCoCreateInstance, uintptr(unsafe.Pointer(&clsidShellLink)), 0, 1, uintptr(unsafe.Pointer(&iidShellLinkW)), uintptr(unsafe.Pointer(&link)))
	if int32(hr) < 0 || link == nil {
		return fmt.Errorf("the shortcut could not be created (0x%08X)", uint32(hr))
	}
	defer link.call(2)                                                    // Release
	link.call(20, uintptr(unsafe.Pointer(u16(target))))                   // SetPath
	link.call(9, uintptr(unsafe.Pointer(u16(workDir))))                   // SetWorkingDirectory
	link.call(7, uintptr(unsafe.Pointer(u16(desc))))                      // SetDescription
	link.call(17, uintptr(unsafe.Pointer(u16(icon))), uintptr(iconIndex)) // SetIconLocation
	var file *comObject
	hr = link.call(0, uintptr(unsafe.Pointer(&iidPersistFile)), uintptr(unsafe.Pointer(&file))) // QueryInterface
	if int32(hr) < 0 || file == nil {
		return fmt.Errorf("the shortcut could not be saved (0x%08X)", uint32(hr))
	}
	defer file.call(2)
	if hr = file.call(6, uintptr(unsafe.Pointer(u16(lnk))), 1); int32(hr) < 0 { // Save
		return fmt.Errorf("saving failed (0x%08X)", uint32(hr))
	}
	return nil
}
