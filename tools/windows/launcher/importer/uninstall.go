// SPDX-License-Identifier: GPL-3.0-or-later

package importer

import (
	"fmt"
	"os"
	"path/filepath"
)

// installItems are the entries a Ricasso package puts next to Ricasso.exe.
// Uninstalling removes exactly these, so anything else the player keeps in the
// folder stays. The running Ricasso.exe itself is removed by the caller after
// it has exited.
var installItems = []string{"app", "README.txt"}

// LooksLikeInstall reports whether root holds a Ricasso package in the current
// layout: the client folder app/ with swordfare.exe and its checksum list.
func LooksLikeInstall(root string) bool {
	for _, name := range []string{"swordfare.exe", "SHA256SUMS.json"} {
		info, err := os.Stat(filepath.Join(root, "app", name))
		if err != nil || !info.Mode().IsRegular() {
			return false
		}
	}
	return true
}

// RemoveInstallation deletes the client folder app/ (including the game files
// that were set up from the player's own copy) and the README next to the
// starter. Saves live elsewhere (%APPDATA%\ricasso) and are not touched.
func RemoveInstallation(root string) error {
	if !LooksLikeInstall(root) {
		return fmt.Errorf("%s does not look like a Ricasso folder (app\\swordfare.exe or app\\SHA256SUMS.json is missing)", root)
	}
	for _, name := range installItems {
		if err := os.RemoveAll(filepath.Join(root, name)); err != nil {
			return err
		}
	}
	return nil
}
