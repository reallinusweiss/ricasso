// SPDX-License-Identifier: GPL-3.0-or-later

package importer

import (
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

// StagingDir is the temporary folder inside the client's program folder that an
// import writes to before it replaces the previous setup.
const StagingDir = ".setup-tmp"

// Names in the client's program folder that the import manages.
const (
	recordFile    = "game-files.json"
	iconFile      = "game-icon.ico"
	iconPNGFile   = "game-icon.png"
	maxWindowsLen = 259 // MAX_PATH - 1; the client uses plain Win32 paths
)

// managed are the entries that are replaced when setting up again.
var managed = []string{"assets", "res", "engine", recordFile, iconFile, iconPNGFile}

// FreeSpace is set on Windows (free disk space in bytes).
var FreeSpace func(dir string) (uint64, error)

// Result describes a successful setup.
type Result struct {
	Version    string
	AssetCount int
	MusicCount int
	HasIcon    bool
	Warnings   []string
}

// Install copies the checked files into appDir (the folder with swordfare.exe).
// It writes to a staging folder first; the old setup is only replaced once
// everything has worked.
func Install(s *Scan, appDir string, progress Progress) (*Result, error) {
	if progress == nil {
		progress = noProgress
	}
	if !fileExists(filepath.Join(appDir, "swordfare.exe")) {
		return nil, userErr("Program files missing", "swordfare.exe is missing in the folder “"+appDir+"”.",
			"Extract the complete ZIP file again.")
	}
	targets, err := s.targets()
	if err != nil {
		return nil, err
	}
	libRel := "engine/v" + s.Version + "/arm64-v8a/libswordigo.so"

	// Path length: the client uses plain Windows paths (max. 259 characters).
	longest := libRel
	for t := range targets {
		if len(t) > len(longest) {
			longest = t
		}
	}
	absApp, _ := filepath.Abs(appDir)
	if n := len([]rune(absApp)) + 1 + len([]rune(longest)); n > maxWindowsLen {
		return nil, userErr("Folder path too long", fmt.Sprintf("The location is nested too deeply (%d of at most %d characters).", n, maxWindowsLen),
			"Move the Swordigo folder to a shorter location, e.g. “C:\\Games\\Swordigo”, and start Swordigo.exe there again.")
	}
	if FreeSpace != nil {
		if free, err := FreeSpace(appDir); err == nil && free < uint64(s.TotalBytes)+(64<<20) {
			return nil, userErr("Not enough disk space", fmt.Sprintf("The game files need about %s, but only %s is free.",
				humanBytes(uint64(s.TotalBytes)), humanBytes(free)), "Free up disk space or move the Swordigo folder to another drive.")
		}
	}

	stage := filepath.Join(appDir, StagingDir)
	if err := os.RemoveAll(stage); err != nil {
		return nil, userErr("Folder locked", "An old staging folder could not be removed.", "Restart your PC and try again.")
	}
	if err := os.MkdirAll(stage, 0o755); err != nil {
		return nil, writeErr(appDir, err)
	}
	cleanup := true
	defer func() {
		if cleanup {
			os.RemoveAll(stage)
		}
	}()

	hashes := map[string]string{}
	total := s.TotalBytes
	var done int64
	count, all := 0, len(targets)+1
	report := func() {
		progress(fmt.Sprintf("Copying game files… (%d of %d)", count, all), done, total)
	}

	// Game library (the checksum is verified again while writing).
	sum, n, err := writeEntry(s.lib, filepath.Join(stage, filepath.FromSlash(libRel)))
	if err != nil {
		return nil, writeErr(appDir, err)
	}
	if KnownLibraries[sum] != s.Version {
		return nil, userErr("File damaged", "The game library changed while copying.", "Copy the file to your PC again.")
	}
	hashes[libRel] = sum
	done += n
	count++
	report()

	names := make([]string, 0, len(targets))
	for t := range targets {
		names = append(names, t)
	}
	sort.Strings(names)
	last := time.Now()
	for _, t := range names {
		sum, n, err := writeEntry(targets[t], filepath.Join(stage, filepath.FromSlash(t)))
		if err != nil {
			return nil, writeErr(appDir, err)
		}
		hashes[t] = sum
		done += n
		count++
		if time.Since(last) > 50*time.Millisecond || count == all {
			report()
			last = time.Now()
		}
	}

	hasIcon := false
	if s.icon != nil {
		if png, err := readAll(s.icon, 4<<20); err == nil && isPNG(png) {
			if os.WriteFile(filepath.Join(stage, iconFile), pngToICO(png), 0o644) == nil &&
				os.WriteFile(filepath.Join(stage, iconPNGFile), png, 0o644) == nil {
				hasIcon = true
			}
		}
	}

	record := map[string]any{
		"version":             s.Version,
		"game_library_sha256": hashes[libRel],
		"asset_count":         s.AssetCount,
		"music_count":         s.MusicCount,
		"sources":             s.Sources,
		"imported_at":         time.Now().Format(time.RFC3339),
		"files":               hashes,
		"note":                "Original files supplied by the user; for local use only. Do not share.",
	}
	data, _ := json.MarshalIndent(record, "", "  ")
	if err := os.WriteFile(filepath.Join(stage, recordFile), append(data, '\n'), 0o644); err != nil {
		return nil, writeErr(appDir, err)
	}

	// Replace the old setup.
	progress("Finishing the setup…", total, total)
	for _, name := range managed {
		if err := os.RemoveAll(filepath.Join(appDir, name)); err != nil {
			return nil, userErr("Files in use", "The old game files could not be replaced.",
				"Close Swordigo (and File Explorer windows showing the Swordigo folder) and try again.")
		}
	}
	for _, name := range managed {
		src := filepath.Join(stage, name)
		if _, err := os.Stat(src); err != nil {
			continue
		}
		if err := os.Rename(src, filepath.Join(appDir, name)); err != nil {
			return nil, writeErr(appDir, err)
		}
	}
	updateStarterScript(appDir, s.Version)
	return &Result{Version: s.Version, AssetCount: s.AssetCount, MusicCount: s.MusicCount, HasIcon: hasIcon, Warnings: s.Warnings}, nil
}

func writeEntry(e *entry, dst string) (string, int64, error) {
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return "", 0, err
	}
	r, err := e.open()
	if err != nil {
		return "", 0, err
	}
	defer r.Close()
	f, err := os.Create(dst)
	if err != nil {
		return "", 0, err
	}
	h := sha256.New()
	n, err := io.Copy(io.MultiWriter(f, h), io.LimitReader(r, maxSingleAsset+1))
	if cerr := f.Close(); err == nil {
		err = cerr
	}
	if err != nil {
		return "", 0, err
	}
	if n > maxSingleAsset {
		return "", 0, errors.New("file too large: " + e.name)
	}
	return hex.EncodeToString(h.Sum(nil)), n, nil
}

func writeErr(appDir string, err error) error {
	msg := err.Error()
	if errors.Is(err, os.ErrPermission) || strings.Contains(strings.ToLower(msg), "access is denied") {
		return userErr("No write access", "Writing to the folder “"+appDir+"” is not allowed.",
			"Move the Swordigo folder, e.g. to “Documents”, and start Swordigo.exe there again. Sometimes the antivirus program blocks it, too (“Controlled folder access”).")
	}
	return userErr("Copying failed", "An error occurred while writing the game files.", msg)
}

// updateStarterScript adapts Start-Swordigo.cmd to the installed version
// (like import_apkm.py), so that the route without Swordigo.exe works too.
func updateStarterScript(appDir, version string) {
	p := filepath.Join(appDir, "Start-Swordigo.cmd")
	data, err := os.ReadFile(p)
	if err != nil {
		return
	}
	text := string(data)
	for _, v := range sortedVersions() {
		text = strings.ReplaceAll(text, "engine/v"+v+"/", "engine/v"+version+"/")
	}
	if text != string(data) {
		os.WriteFile(p, []byte(text), 0o644)
	}
}

// Installed describes the state of the program folder.
type Installed struct {
	OK      bool
	Version string
	LibRel  string // relative to appDir, with "/"
	Problem string // explanation for the user if !OK
	HasIcon bool
}

// DetectInstalled checks whether appDir contains installed game files.
func DetectInstalled(appDir string) Installed {
	var st Installed
	version := ""
	if data, err := os.ReadFile(filepath.Join(appDir, recordFile)); err == nil {
		var rec struct {
			Version string `json:"version"`
		}
		if json.Unmarshal(data, &rec) == nil {
			version = rec.Version
		}
	}
	candidates := sortedVersions()
	if version != "" {
		candidates = append([]string{version}, candidates...)
	}
	for _, v := range candidates {
		rel := "engine/v" + v + "/arm64-v8a/libswordigo.so"
		if isARM64ELF(filepath.Join(appDir, filepath.FromSlash(rel))) {
			st.Version, st.LibRel = v, rel
			break
		}
	}
	st.HasIcon = fileExists(filepath.Join(appDir, iconFile))
	if st.LibRel == "" {
		st.Problem = "No game files have been set up yet."
		return st
	}
	if !fileExists(filepath.Join(appDir, "assets", "resources", "gamedata.gdata")) &&
		!fileExists(filepath.Join(appDir, "assets", "gamedata.gdata")) {
		st.Problem = "The game data is incomplete (gamedata.gdata is missing)."
		return st
	}
	st.OK = true
	return st
}

// GameArgs matches the command line in Start-Swordigo.cmd.
func GameArgs(st Installed) []string {
	return []string{"--no-launcher", "--dynarmic", "--lib", st.LibRel, "--assets", "assets"}
}

// CheckArgs matches the preflight check in Start-Swordigo.cmd.
func CheckArgs(st Installed) []string {
	return []string{"--check-data", "--lib", st.LibRel, "--assets", "assets"}
}

// RequiredClientFiles are files without which the client won't start.
var RequiredClientFiles = []string{"swordfare.exe", "libs/libsre13.so", "libs/libsre12.so", "platforms/qwindows.dll",
	"SDL3.dll", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "libopenal-1.dll", "glew32.dll", "libstdc++-6.dll", "qt.conf"}

// MissingClientFiles lists missing program files (e.g. extracted incompletely
// or removed by an antivirus program).
func MissingClientFiles(appDir string) []string {
	var missing []string
	for _, rel := range RequiredClientFiles {
		if !fileExists(filepath.Join(appDir, filepath.FromSlash(rel))) {
			missing = append(missing, rel)
		}
	}
	return missing
}

func isARM64ELF(p string) bool {
	f, err := os.Open(p)
	if err != nil {
		return false
	}
	defer f.Close()
	h := make([]byte, 20)
	if _, err := io.ReadFull(f, h); err != nil {
		return false
	}
	return h[0] == 0x7f && h[1] == 'E' && h[2] == 'L' && h[3] == 'F' && h[4] == 2 && h[5] == 1 &&
		binary.LittleEndian.Uint16(h[18:]) == 183
}

func isPNG(b []byte) bool {
	return len(b) > 24 && string(b[:8]) == "\x89PNG\r\n\x1a\n"
}

// pngToICO wraps a PNG as a Windows icon (supported since Windows Vista).
func pngToICO(png []byte) []byte {
	w := binary.BigEndian.Uint32(png[16:20])
	h := binary.BigEndian.Uint32(png[20:24])
	b := func(v uint32) byte {
		if v >= 256 {
			return 0
		}
		return byte(v)
	}
	out := make([]byte, 0, 22+len(png))
	out = binary.LittleEndian.AppendUint16(out, 0) // reserved
	out = binary.LittleEndian.AppendUint16(out, 1) // type: icon
	out = binary.LittleEndian.AppendUint16(out, 1) // count
	out = append(out, b(w), b(h), 0, 0)
	out = binary.LittleEndian.AppendUint16(out, 1)  // color planes
	out = binary.LittleEndian.AppendUint16(out, 32) // bits per pixel
	out = binary.LittleEndian.AppendUint32(out, uint32(len(png)))
	out = binary.LittleEndian.AppendUint32(out, 22)
	return append(out, png...)
}

func humanBytes(n uint64) string {
	switch {
	case n >= 1<<30:
		return fmt.Sprintf("%.1f GB", float64(n)/(1<<30))
	case n >= 1<<20:
		return fmt.Sprintf("%.0f MB", float64(n)/(1<<20))
	default:
		return fmt.Sprintf("%d KB", n/1024)
	}
}
