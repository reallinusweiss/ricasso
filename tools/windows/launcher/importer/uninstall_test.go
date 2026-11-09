// SPDX-License-Identifier: GPL-3.0-or-later

package importer

import (
	"os"
	"path/filepath"
	"testing"
)

func TestRemoveInstallationKeepsForeignFiles(t *testing.T) {
	root := t.TempDir()
	write := func(rel string) {
		p := filepath.Join(root, rel)
		if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(p, []byte("x"), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	for _, rel := range []string{
		"Ricasso.exe", "README.txt", "my-notes.txt",
		"app/swordfare.exe", "app/SHA256SUMS.json", "app/assets/resources/a.pvr",
		"app/engine/v1.4.13/arm64-v8a/libswordigo.so", "app/" + StagingDir + "/part",
	} {
		write(rel)
	}
	if !LooksLikeInstall(root) {
		t.Fatal("complete package not recognised")
	}
	if err := RemoveInstallation(root); err != nil {
		t.Fatal(err)
	}
	for _, gone := range []string{"app", "README.txt"} {
		if _, err := os.Stat(filepath.Join(root, gone)); !os.IsNotExist(err) {
			t.Errorf("%s still exists", gone)
		}
	}
	// The running starter and files that are not part of the package stay.
	for _, kept := range []string{"Ricasso.exe", "my-notes.txt"} {
		if _, err := os.Stat(filepath.Join(root, kept)); err != nil {
			t.Errorf("%s was removed: %v", kept, err)
		}
	}
}

func TestRemoveInstallationRefusesOtherFolders(t *testing.T) {
	root := t.TempDir()
	// A folder with an app/ subfolder that is not a Ricasso client.
	if err := os.MkdirAll(filepath.Join(root, "app"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "app", "swordfare.exe"), []byte("x"), 0o644); err != nil {
		t.Fatal(err)
	}
	if LooksLikeInstall(root) {
		t.Fatal("folder without SHA256SUMS.json accepted")
	}
	if err := RemoveInstallation(root); err == nil {
		t.Fatal("RemoveInstallation should refuse")
	}
	if _, err := os.Stat(filepath.Join(root, "app", "swordfare.exe")); err != nil {
		t.Fatal("files were removed although the folder was refused")
	}
}
