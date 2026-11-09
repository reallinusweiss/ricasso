//go:build windows

// SPDX-License-Identifier: GPL-3.0-or-later

package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"syscall"
	"testing"
	"time"
)

// The cleanup must outlast a starter that is still holding Ricasso.exe open
// and then remove the file and the empty folder.
func TestDeleteScriptWaitsForLockedFile(t *testing.T) {
	dir := filepath.Join(t.TempDir(), "Ricasso Test & Co")
	if err := os.Mkdir(dir, 0o755); err != nil {
		t.Fatal(err)
	}
	exe := filepath.Join(dir, "Ricasso.exe")
	if err := os.WriteFile(exe, []byte("x"), 0o644); err != nil {
		t.Fatal(err)
	}
	lock, err := os.Open(exe) // Go opens without FILE_SHARE_DELETE: del fails meanwhile
	if err != nil {
		t.Fatal(err)
	}
	cmd := exec.Command(filepath.Join(os.Getenv("SystemRoot"), "System32", "cmd.exe"))
	cmd.Dir = filepath.Dir(dir)
	cmd.SysProcAttr = &syscall.SysProcAttr{CmdLine: deleteScript(exe, dir, true), HideWindow: true}
	if err := cmd.Start(); err != nil {
		lock.Close()
		t.Fatal(err)
	}
	time.Sleep(2500 * time.Millisecond)
	if _, err := os.Stat(exe); err != nil {
		lock.Close()
		t.Fatalf("file vanished while it was still locked: %v", err)
	}
	lock.Close()
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	select {
	case <-done:
	case <-time.After(40 * time.Second):
		cmd.Process.Kill()
		t.Fatal("cleanup did not finish")
	}
	if _, err := os.Stat(dir); !os.IsNotExist(err) {
		t.Fatalf("folder still exists (%v)", err)
	}
}

func TestCanScheduleDelete(t *testing.T) {
	if !canScheduleDelete(`C:\Games\Ricasso Windows x64\Ricasso.exe`, `C:\Games\Ricasso Windows x64`) {
		t.Fatal("ordinary path rejected")
	}
	for _, bad := range []string{`C:\100%\Ricasso.exe`, `C:\a"b\Ricasso.exe`, `C:\Hi!\Ricasso.exe`} {
		if canScheduleDelete(bad, filepath.Dir(bad)) {
			t.Errorf("%s accepted", bad)
		}
	}
}
