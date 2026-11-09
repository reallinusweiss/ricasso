// SPDX-License-Identifier: GPL-3.0-or-later

package importer

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"image"
	"image/color"
	"image/png"
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
			"Move the Ricasso folder to a shorter location, e.g. “C:\\Games\\Ricasso”, and start Ricasso.exe there again.")
	}
	if FreeSpace != nil {
		if free, err := FreeSpace(appDir); err == nil && free < uint64(s.TotalBytes)+(64<<20) {
			return nil, userErr("Not enough disk space", fmt.Sprintf("The game files need about %s, but only %s is free.",
				humanBytes(uint64(s.TotalBytes)), humanBytes(free)), "Free up disk space or move the Ricasso folder to another drive.")
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
				"Close Swordigo (and File Explorer windows showing the Ricasso folder) and try again.")
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
			"Move the Ricasso folder, e.g. to “Documents”, and start Ricasso.exe there again. Sometimes the antivirus program blocks it, too (“Controlled folder access”).")
	}
	return userErr("Copying failed", "An error occurred while writing the game files.", msg)
}

// updateStarterScript adapts Start-Swordigo.cmd to the installed version
// (like import_apkm.py), so that the route without Ricasso.exe works too.
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

// iconSizes are the bitmap sizes Windows asks for in Explorer, the taskbar,
// the Start menu and title bars at 100–200 % display scaling.
var iconSizes = []int{16, 20, 24, 32, 40, 48, 64}

// pngToICO builds a Windows icon from a PNG: the standard small sizes as 32-bit
// bitmaps, which every part of Windows can read, plus the original PNG as the
// largest image. An icon holding only one PNG of an unusual size (such as the
// 192×192 launcher icon) is shown blank or blurry in some places. If the PNG
// can't be decoded, it is wrapped on its own.
func pngToICO(pngData []byte) []byte {
	type iconImage struct {
		w, h int
		data []byte
	}
	var images []iconImage
	if src, err := png.Decode(bytes.NewReader(pngData)); err == nil {
		b := src.Bounds()
		for _, s := range iconSizes {
			if s < b.Dx() && s < b.Dy() {
				images = append(images, iconImage{s, s, iconBitmap(scaleImage(src, s))})
			}
		}
	}
	images = append(images, iconImage{int(binary.BigEndian.Uint32(pngData[16:20])), int(binary.BigEndian.Uint32(pngData[20:24])), pngData})

	dim := func(v int) byte {
		if v >= 256 {
			return 0 // 0 means 256 or more
		}
		return byte(v)
	}
	out := binary.LittleEndian.AppendUint16(nil, 0)                  // reserved
	out = binary.LittleEndian.AppendUint16(out, 1)                   // type: icon
	out = binary.LittleEndian.AppendUint16(out, uint16(len(images))) // count
	offset := 6 + 16*len(images)
	for _, im := range images {
		out = append(out, dim(im.w), dim(im.h), 0, 0)
		out = binary.LittleEndian.AppendUint16(out, 1)  // color planes
		out = binary.LittleEndian.AppendUint16(out, 32) // bits per pixel
		out = binary.LittleEndian.AppendUint32(out, uint32(len(im.data)))
		out = binary.LittleEndian.AppendUint32(out, uint32(offset))
		offset += len(im.data)
	}
	for _, im := range images {
		out = append(out, im.data...)
	}
	return out
}

// scaleImage shrinks src to s×s by averaging the covered source pixels
// (with premultiplied alpha, so transparent edges don't turn dark).
func scaleImage(src image.Image, s int) *image.NRGBA {
	b := src.Bounds()
	dst := image.NewNRGBA(image.Rect(0, 0, s, s))
	for y := 0; y < s; y++ {
		y0, y1 := b.Min.Y+y*b.Dy()/s, b.Min.Y+(y+1)*b.Dy()/s
		if y1 <= y0 {
			y1 = y0 + 1
		}
		for x := 0; x < s; x++ {
			x0, x1 := b.Min.X+x*b.Dx()/s, b.Min.X+(x+1)*b.Dx()/s
			if x1 <= x0 {
				x1 = x0 + 1
			}
			var r, g, bl, a, n uint64
			for sy := y0; sy < y1; sy++ {
				for sx := x0; sx < x1; sx++ {
					cr, cg, cb, ca := src.At(sx, sy).RGBA()
					r, g, bl, a, n = r+uint64(cr), g+uint64(cg), bl+uint64(cb), a+uint64(ca), n+1
				}
			}
			if a == 0 {
				continue // fully transparent
			}
			dst.SetNRGBA(x, y, color.NRGBA{
				R: uint8(r * 0xff / a), G: uint8(g * 0xff / a), B: uint8(bl * 0xff / a),
				A: uint8((a / n) >> 8),
			})
		}
	}
	return dst
}

// iconBitmap encodes an image as the bitmap of an icon entry: a
// BITMAPINFOHEADER with doubled height, bottom-up BGRA pixels and an empty
// AND mask (the alpha channel decides transparency).
func iconBitmap(m *image.NRGBA) []byte {
	s := m.Bounds().Dx()
	maskRow := (s + 31) / 32 * 4
	out := binary.LittleEndian.AppendUint32(nil, 40) // header size
	out = binary.LittleEndian.AppendUint32(out, uint32(s))
	out = binary.LittleEndian.AppendUint32(out, uint32(2*s)) // colour bitmap + mask
	out = binary.LittleEndian.AppendUint16(out, 1)           // planes
	out = binary.LittleEndian.AppendUint16(out, 32)          // bits per pixel
	out = binary.LittleEndian.AppendUint32(out, 0)           // BI_RGB
	out = binary.LittleEndian.AppendUint32(out, uint32(s*s*4+maskRow*s))
	out = append(out, make([]byte, 16)...) // resolution and palette: unused
	for y := s - 1; y >= 0; y-- {
		for x := 0; x < s; x++ {
			c := m.NRGBAAt(x, y)
			out = append(out, c.B, c.G, c.R, c.A)
		}
	}
	return append(out, make([]byte, maskRow*s)...)
}

// RefreshIcon rewrites game-icon.ico of an older setup, which held only the
// 192×192 PNG, with all standard sizes. It returns true if the file changed;
// the caller should then let Windows refresh its icon cache.
func RefreshIcon(appDir string) bool {
	icoPath := filepath.Join(appDir, iconFile)
	ico, err := os.ReadFile(icoPath)
	if err != nil || len(ico) < 6 || binary.LittleEndian.Uint16(ico[4:6]) != 1 {
		return false
	}
	pngData, err := os.ReadFile(filepath.Join(appDir, iconPNGFile))
	if err != nil || !isPNG(pngData) {
		return false
	}
	fresh := pngToICO(pngData)
	if binary.LittleEndian.Uint16(fresh[4:6]) <= 1 {
		return false // not decodable; keep the old file
	}
	tmp := icoPath + ".tmp"
	if err := os.WriteFile(tmp, fresh, 0o644); err != nil {
		os.Remove(tmp)
		return false
	}
	if err := os.Rename(tmp, icoPath); err != nil {
		os.Remove(tmp)
		return false
	}
	return true
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
