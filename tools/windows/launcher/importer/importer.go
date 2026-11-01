// SPDX-License-Identifier: GPL-3.0-or-later

// Package importer checks a copy of Swordigo supplied by the user themselves
// (APKM, APKS, XAPK, APK, several APKs or a folder) and copies only the files
// the Windows client needs into its program folder.
//
// Files are never downloaded or shared. The game files never leave the
// user's computer.
package importer

import (
	"archive/zip"
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path"
	"path/filepath"
	"sort"
	"strings"
	"unicode/utf16"
)

// PackageName is the Android package ID of Swordigo.
const PackageName = "com.touchfoo.swordigo"

// LibEntry is the path of the 64-bit game library inside an APK.
const LibEntry = "lib/arm64-v8a/libswordigo.so"

// KnownLibraries maps the SHA-256 of the ARM64 game library to a version.
// The same values are in tools/windows/import_apkm.py.
var KnownLibraries = map[string]string{
	"f847814d1b6f81268567ed5ec2473fea4d4ee3b75d2c6fec7057227225e989f8": "1.4.12",
	"c2f567a34ec7bed9cf6b0af0ce88e9c925c11e5c2a2f2fbd8604461cf93aae7d": "1.4.13",
}

// SupportedVersionsText is meant for messages.
const SupportedVersionsText = "1.4.13 (recommended) and 1.4.12"

// Limits against broken or malicious archives.
const (
	maxNestedAPK   = 1 << 30 // 1 GiB per embedded APK
	maxSingleAsset = 256 << 20
	maxTotalAssets = 2 << 30
	maxManifest    = 8 << 20
)

// UserError is an error with an understandable text for the user interface.
type UserError struct {
	Title   string
	Message string
	Hint    string
}

func (e *UserError) Error() string {
	if e.Hint == "" {
		return e.Title + ": " + e.Message
	}
	return e.Title + ": " + e.Message + " " + e.Hint
}

func userErr(title, msg, hint string) error {
	return &UserError{Title: title, Message: msg, Hint: hint}
}

// Progress reports progress. total <= 0 means "indeterminate".
type Progress func(text string, done, total int64)

func noProgress(string, int64, int64) {}

// entry is a file from an APK (zip) or from a folder.
type entry struct {
	name   string // original path, e.g. assets/resources/gamedata.gdata
	size   int64
	crc    uint32
	hasCRC bool
	origin string // for error messages
	open   func() (io.ReadCloser, error)
}

func (e *entry) sameAs(o *entry) bool {
	if e.size != o.size {
		return false
	}
	if e.hasCRC && o.hasCRC {
		return e.crc == o.crc
	}
	a, err1 := readAll(e, maxSingleAsset)
	b, err2 := readAll(o, maxSingleAsset)
	return err1 == nil && err2 == nil && bytes.Equal(a, b)
}

func readAll(e *entry, limit int64) ([]byte, error) {
	r, err := e.open()
	if err != nil {
		return nil, err
	}
	defer r.Close()
	data, err := io.ReadAll(io.LimitReader(r, limit+1))
	if err != nil {
		return nil, err
	}
	if int64(len(data)) > limit {
		return nil, fmt.Errorf("%s is too large", e.name)
	}
	return data, nil
}

// Scan is the result of the check. Close must be called.
type Scan struct {
	Version         string   // from the checksum of the game library
	DeclaredVersion string   // from metadata (if present)
	AssetCount      int      // files under assets/
	MusicCount      int      // files under res/raw/music_*
	TotalBytes      int64    // sum of all bytes to copy
	Warnings        []string // non-critical notes
	Sources         []string // file names that were read

	lib     *entry
	files   map[string]*entry // original path -> entry
	icon    *entry
	closers []io.Closer
}

// Close releases opened archives.
func (s *Scan) Close() {
	for _, c := range s.closers {
		c.Close()
	}
	s.closers = nil
}

type collector struct {
	scan          *Scan
	progress      Progress
	apkCount      int
	swordigoAPKs  int
	foreignAPKs   []string
	hasARMv7      bool
	hasX86        bool
	declaredPkgs  []string
	iconCands     []*entry
	encrypted     bool
	notArchive    []string
	unknownFiles  []string
	folderSources int
}

// ScanInputs checks the files or folders chosen by the user.
func ScanInputs(paths []string, progress Progress) (*Scan, error) {
	if progress == nil {
		progress = noProgress
	}
	if len(paths) == 0 {
		return nil, userErr("No file chosen", "Please choose your Swordigo file.", "")
	}
	c := &collector{scan: &Scan{files: map[string]*entry{}}, progress: progress}
	ok := false
	defer func() {
		if !ok {
			c.scan.Close()
		}
	}()
	for _, p := range paths {
		info, err := os.Stat(p)
		if err != nil {
			return nil, userErr("File not found", "“"+filepath.Base(p)+"” could not be opened.",
				"Is the file perhaps still in the cloud or on your phone? Copy it to your PC first.")
		}
		if info.IsDir() {
			if err := c.addDir(p); err != nil {
				return nil, err
			}
			continue
		}
		if err := c.addFile(p, info.Size()); err != nil {
			return nil, err
		}
	}
	if err := c.finish(); err != nil {
		return nil, err
	}
	ok = true
	return c.scan, nil
}

func (c *collector) addFile(p string, size int64) error {
	name := filepath.Base(p)
	c.progress("Opening “"+name+"”…", -1, -1)
	f, err := os.Open(p)
	if err != nil {
		return userErr("File not readable", "“"+name+"” could not be opened.", "Is the file still open in another program?")
	}
	zr, err := zip.NewReader(f, size)
	if err != nil {
		f.Close()
		lower := strings.ToLower(name)
		switch {
		case strings.HasSuffix(lower, ".zip") || strings.HasSuffix(lower, ".apk") || strings.HasSuffix(lower, ".apkm") ||
			strings.HasSuffix(lower, ".apks") || strings.HasSuffix(lower, ".xapk"):
			c.notArchive = append(c.notArchive, name)
			return nil
		default:
			c.unknownFiles = append(c.unknownFiles, name)
			return nil
		}
	}
	c.scan.closers = append(c.scan.closers, f)
	c.scan.Sources = append(c.scan.Sources, name)
	return c.addArchive(zr, name, true)
}

// addArchive handles an APK or a bundle (APKM/APKS/XAPK/ZIP with APKs).
func (c *collector) addArchive(zr *zip.Reader, name string, topLevel bool) error {
	for _, f := range zr.File {
		if f.Flags&0x1 != 0 {
			c.encrypted = true
			return nil
		}
	}
	if findFile(zr, "AndroidManifest.xml") != nil {
		return c.addAPK(zr, name)
	}
	if !topLevel {
		c.notArchive = append(c.notArchive, name)
		return nil
	}
	// Bundle: read metadata (only for better messages) and open embedded APKs.
	c.readBundleMeta(zr)
	nested := 0
	for _, f := range zr.File {
		lower := strings.ToLower(f.Name)
		if f.FileInfo().IsDir() || !strings.HasSuffix(lower, ".apk") {
			continue
		}
		nested++
		if f.UncompressedSize64 > maxNestedAPK {
			return userErr("File too large", "“"+f.Name+"” in “"+name+"” is unexpectedly large.", "")
		}
		c.progress("Reading “"+path.Base(f.Name)+"”…", -1, -1)
		data, err := readZipFile(f, maxNestedAPK)
		if err != nil {
			return userErr("File damaged", "“"+f.Name+"” in “"+name+"” could not be read.",
				"The file was probably not transferred completely. Copy it to your PC again.")
		}
		inner, err := zip.NewReader(bytes.NewReader(data), int64(len(data)))
		if err != nil {
			c.notArchive = append(c.notArchive, path.Base(f.Name))
			continue
		}
		if err := c.addArchive(inner, path.Base(f.Name), false); err != nil {
			return err
		}
	}
	if nested == 0 {
		c.unknownFiles = append(c.unknownFiles, name)
	}
	return nil
}

func (c *collector) readBundleMeta(zr *zip.Reader) {
	type meta struct {
		Pname          string `json:"pname"`
		ReleaseVersion string `json:"release_version"`
		PackageName    string `json:"package_name"`
		VersionName    string `json:"version_name"`
		Package        string `json:"package"`
	}
	for _, n := range []string{"info.json", "manifest.json", "meta.sai_v2.json", "meta.sai_v1.json"} {
		f := findFile(zr, n)
		if f == nil || f.UncompressedSize64 > 1<<20 {
			continue
		}
		data, err := readZipFile(f, 1<<20)
		if err != nil {
			continue
		}
		var m meta
		if json.Unmarshal(data, &m) != nil {
			continue
		}
		for _, p := range []string{m.Pname, m.PackageName, m.Package} {
			if p != "" {
				c.declaredPkgs = append(c.declaredPkgs, p)
			}
		}
		for _, v := range []string{m.ReleaseVersion, m.VersionName} {
			if v != "" && c.scan.DeclaredVersion == "" {
				c.scan.DeclaredVersion = v
			}
		}
	}
}

func (c *collector) addAPK(zr *zip.Reader, name string) error {
	c.apkCount++
	mf := findFile(zr, "AndroidManifest.xml")
	data, err := readZipFile(mf, maxManifest)
	if err != nil || !containsPackage(data) {
		c.foreignAPKs = append(c.foreignAPKs, name)
		return nil
	}
	c.swordigoAPKs++
	for _, f := range zr.File {
		if f.FileInfo().IsDir() {
			continue
		}
		e := zipEntry(f, name)
		switch {
		case f.Name == LibEntry:
			if err := c.setLib(e); err != nil {
				return err
			}
		case strings.HasPrefix(f.Name, "lib/armeabi") && strings.HasSuffix(f.Name, "/libswordigo.so"):
			c.hasARMv7 = true
		case strings.HasPrefix(f.Name, "lib/x86") && strings.HasSuffix(f.Name, "/libswordigo.so"):
			c.hasX86 = true
		case strings.HasPrefix(f.Name, "assets/") || isMusic(f.Name):
			if err := c.addGameFile(e); err != nil {
				return err
			}
		case isIconCandidate(f.Name):
			c.iconCands = append(c.iconCands, e)
		}
	}
	return nil
}

func (c *collector) setLib(e *entry) error {
	if c.scan.lib != nil && !c.scan.lib.sameAs(e) {
		return userErr("Mixed versions", "The chosen files contain two different game versions.",
			"Choose only the files of a single Swordigo version.")
	}
	if c.scan.lib == nil {
		c.scan.lib = e
	}
	return nil
}

func (c *collector) addGameFile(e *entry) error {
	if prev, ok := c.scan.files[e.name]; ok {
		if !prev.sameAs(e) {
			return userErr("Mixed versions", "The file “"+e.name+"” appears in the chosen files with different contents.",
				"Choose only the files of a single Swordigo version.")
		}
		return nil
	}
	c.scan.files[e.name] = e
	return nil
}

// addDir supports three kinds of folders:
//  1. a folder with package files (e.g. base.apk + split_config.arm64_v8a.apk),
//  2. an extracted APK (lib/arm64-v8a/…, assets/, res/raw/),
//  3. an existing Swordigo Windows installation (engine/v*/arm64-v8a/…, assets/, res/raw/),
//     including its main folder with the subfolder "app".
func (c *collector) addDir(dir string) error {
	if fileExists(filepath.Join(dir, "app", "swordfare.exe")) {
		dir = filepath.Join(dir, "app")
	}
	libCandidates := []string{filepath.Join(dir, filepath.FromSlash(LibEntry))}
	for _, v := range sortedVersions() {
		libCandidates = append(libCandidates, filepath.Join(dir, "engine", "v"+v, "arm64-v8a", "libswordigo.so"))
	}
	lib := ""
	for _, cand := range libCandidates {
		if fileExists(cand) {
			lib = cand
			break
		}
	}
	if lib != "" && dirExists(filepath.Join(dir, "assets")) {
		return c.addExtractedDir(dir, lib)
	}
	// Look for package files in the folder (at most two levels deep).
	found := 0
	err := filepath.WalkDir(dir, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return nil
		}
		rel, _ := filepath.Rel(dir, p)
		depth := strings.Count(rel, string(filepath.Separator))
		if d.IsDir() {
			if p != dir && depth >= 2 {
				return filepath.SkipDir
			}
			return nil
		}
		lower := strings.ToLower(d.Name())
		for _, ext := range []string{".apk", ".apkm", ".apks", ".xapk"} {
			if strings.HasSuffix(lower, ext) {
				info, err := d.Info()
				if err != nil {
					return nil
				}
				found++
				return c.addFile(p, info.Size())
			}
		}
		return nil
	})
	if err != nil {
		return err
	}
	if found == 0 {
		return userErr("Nothing found", "No Swordigo file was found in the folder “"+filepath.Base(dir)+"”.",
			"Choose the file itself (extension .apkm, .apks, .xapk or .apk).")
	}
	return nil
}

func (c *collector) addExtractedDir(dir, lib string) error {
	c.folderSources++
	c.scan.Sources = append(c.scan.Sources, filepath.Base(dir))
	mf := filepath.Join(dir, "AndroidManifest.xml")
	if fileExists(mf) {
		if data, err := os.ReadFile(mf); err == nil && !containsPackage(data) {
			c.foreignAPKs = append(c.foreignAPKs, filepath.Base(dir))
			return nil
		}
	}
	c.swordigoAPKs++
	if err := c.setLib(diskEntry(lib, LibEntry, filepath.Base(dir))); err != nil {
		return err
	}
	// Folder names like "X.scene_res" come from an earlier import and
	// are taken over unchanged.
	walk := func(root, prefix string, filter func(rel string) bool) error {
		if !dirExists(root) {
			return nil
		}
		return filepath.WalkDir(root, func(p string, d fs.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if d.IsDir() {
				return nil
			}
			rel, err := filepath.Rel(root, p)
			if err != nil {
				return err
			}
			name := prefix + filepath.ToSlash(rel)
			if !filter(name) {
				return nil
			}
			return c.addGameFile(diskEntry(p, name, filepath.Base(dir)))
		})
	}
	if err := walk(filepath.Join(dir, "assets"), "assets/", func(string) bool { return true }); err != nil {
		return userErr("Folder not readable", "The folder “assets” could not be read completely.", err.Error())
	}
	if err := walk(filepath.Join(dir, "res", "raw"), "res/raw/", isMusic); err != nil {
		return userErr("Folder not readable", "The folder “res/raw” could not be read completely.", err.Error())
	}
	icon := filepath.Join(dir, "game-icon.png")
	if fileExists(icon) {
		c.iconCands = append(c.iconCands, diskEntry(icon, "game-icon.png", filepath.Base(dir)))
	}
	return nil
}

func (c *collector) finish() error {
	s := c.scan
	if c.encrypted {
		return userErr("File encrypted", "This file is encrypted or saved in an old format and can't be read.",
			"Export Swordigo from your phone again, preferably as an .apks file or as individual APK files.")
	}
	if c.apkCount == 0 && c.folderSources == 0 {
		if len(c.notArchive) > 0 {
			return userErr("File damaged", "“"+c.notArchive[0]+"” is not a readable Android package.",
				"The file was probably not transferred completely (or is encrypted). Export it or copy it to your PC again.")
		}
		what := "The chosen file"
		if len(c.unknownFiles) > 0 {
			what = "“" + c.unknownFiles[0] + "”"
		}
		return userErr("Wrong file type", what+" is not an Android package.",
			"You need a file with the extension .apkm, .apks, .xapk or .apk.")
	}
	if c.swordigoAPKs == 0 {
		other := ""
		for _, p := range c.declaredPkgs {
			if p != PackageName {
				other = " (found: " + p + ")"
				break
			}
		}
		return userErr("This isn't Swordigo", "The chosen file belongs to a different app"+other+".",
			"Choose the Swordigo file (package name "+PackageName+").")
	}
	if s.lib == nil {
		switch {
		case c.hasARMv7 || c.hasX86:
			return userErr("Wrong device variant", "The file only contains the 32-bit variant of the game. The Windows port needs the 64-bit variant (arm64-v8a).",
				"Export Swordigo from a 64-bit phone (almost every phone from about 2017 onwards).")
		default:
			return userErr("Part missing", "The game library (arm64-v8a) is missing. For split apps it is inside “split_config.arm64_v8a.apk”.",
				"Choose all APK files together (Ctrl+A in the file dialog) or export Swordigo as a single .apks file.")
		}
	}
	c.progress("Checking the game version…", -1, -1)
	sum, err := hashEntry(s.lib)
	if err != nil {
		return userErr("File damaged", "The game library could not be read.",
			"The file was probably not transferred completely. Copy it to your PC again.")
	}
	version, ok := KnownLibraries[sum]
	if !ok {
		for _, v := range KnownLibraries {
			if s.DeclaredVersion != "" && v == s.DeclaredVersion {
				return userErr("File modified or damaged", "Your file says it is version "+v+", but the game library "+
					"differs from the tested one (modified, damaged or a different build).",
					"Use an unmodified copy from Google Play, ideally freshly exported from your phone.")
			}
		}
		found := ""
		if s.DeclaredVersion != "" {
			found = " Your file: version " + s.DeclaredVersion + "."
		}
		return userErr("Version not supported", "This Swordigo version is not supported by the Windows port yet."+found+
			" Supported: "+SupportedVersionsText+".",
			"The checksum of the game library matches no tested version. Other versions don't run reliably.")
	}
	s.Version = version
	if s.files["assets/resources/gamedata.gdata"] == nil && s.files["assets/gamedata.gdata"] == nil {
		return userErr("Part missing", "The game data (gamedata.gdata) is missing. It is inside “base.apk”.",
			"Choose all APK files together (Ctrl+A in the file dialog) or export Swordigo as a single .apks file.")
	}
	var total int64 = s.lib.size
	for name, e := range s.files {
		if err := checkName(name); err != nil {
			return err
		}
		if e.size > maxSingleAsset {
			return userErr("File too large", "“"+name+"” is unexpectedly large.", "")
		}
		total += e.size
		if strings.HasPrefix(name, "assets/") {
			s.AssetCount++
		} else {
			s.MusicCount++
		}
	}
	if total > maxTotalAssets {
		return userErr("File too large", "The game data is unexpectedly large.", "")
	}
	s.TotalBytes = total
	if _, err := s.targets(); err != nil {
		return err
	}
	if s.MusicCount == 0 {
		s.Warnings = append(s.Warnings, "No music files (res/raw/music_*) were found. The game will run without music.")
	}
	s.icon = pickIcon(c.iconCands)
	return nil
}

// targets computes the target paths. Zip archives can contain both the file "X.scene"
// and the folder "X.scene/…"; Windows can't. The folder then becomes
// "X.scene_res/" (supported by the client, as in import_apkm.py).
func (s *Scan) targets() (map[string]*entry, error) {
	out := make(map[string]*entry, len(s.files))
	folded := map[string]string{}
	names := make([]string, 0, len(s.files))
	for n := range s.files {
		names = append(names, n)
	}
	sort.Strings(names)
	for _, name := range names {
		parts := strings.Split(name, "/")
		renamed := append([]string(nil), parts...)
		for i := 1; i < len(parts); i++ {
			if _, isFile := s.files[strings.Join(parts[:i], "/")]; isFile {
				renamed[i-1] += "_res"
			}
		}
		target := strings.Join(renamed, "/")
		key := strings.ToLower(target)
		if prev, ok := folded[key]; ok {
			if !s.files[prev].sameAs(s.files[name]) {
				return nil, userErr("File name conflict", "“"+prev+"” and “"+name+"” differ only in upper/lower case.",
					"Windows can't store both at the same time. Please report this to the port developer.")
			}
			continue
		}
		folded[key] = name
		out[target] = s.files[name]
	}
	// A file must not also be the folder of another target.
	for target := range out {
		parts := strings.Split(target, "/")
		for i := 1; i < len(parts); i++ {
			if _, clash := out[strings.Join(parts[:i], "/")]; clash {
				return nil, userErr("File name conflict", "“"+strings.Join(parts[:i], "/")+"” is both a file and a folder.",
					"Please report this to the port developer.")
			}
		}
	}
	return out, nil
}

var reservedNames = map[string]bool{"con": true, "prn": true, "aux": true, "nul": true,
	"com1": true, "com2": true, "com3": true, "com4": true, "com5": true, "com6": true, "com7": true, "com8": true, "com9": true,
	"lpt1": true, "lpt2": true, "lpt3": true, "lpt4": true, "lpt5": true, "lpt6": true, "lpt7": true, "lpt8": true, "lpt9": true}

func checkName(name string) error {
	bad := func() error {
		return userErr("Invalid file name", "The package contains a file name that Windows can't store: “"+name+"”.",
			"The file may be damaged or modified. Use an unmodified copy.")
	}
	if name == "" || strings.HasPrefix(name, "/") || strings.Contains(name, "\\") || strings.ContainsRune(name, 0) {
		return bad()
	}
	for _, part := range strings.Split(name, "/") {
		if part == "" || part == "." || part == ".." {
			return bad()
		}
		if strings.ContainsAny(part, `<>:"|?*`) || strings.HasSuffix(part, ".") || strings.HasSuffix(part, " ") {
			return bad()
		}
		for _, r := range part {
			if r < 32 {
				return bad()
			}
		}
		base := strings.ToLower(part)
		if i := strings.IndexByte(base, '.'); i >= 0 {
			base = base[:i]
		}
		if reservedNames[base] {
			return bad()
		}
	}
	return nil
}

func isMusic(name string) bool {
	if !strings.HasPrefix(name, "res/raw/music_") || strings.Count(name, "/") != 2 {
		return false
	}
	switch strings.ToLower(path.Ext(name)) {
	case ".mp3", ".ogg", ".wav":
		return true
	}
	return false
}

func isIconCandidate(name string) bool {
	if !strings.HasPrefix(name, "res/") || !strings.HasSuffix(strings.ToLower(name), ".png") {
		return false
	}
	base := strings.ToLower(path.Base(name))
	dir := path.Dir(name)
	return (strings.Contains(dir, "mipmap") || strings.Contains(dir, "drawable")) &&
		(strings.HasPrefix(base, "ic_launcher") || strings.HasPrefix(base, "icon") || strings.HasPrefix(base, "app_icon"))
}

func pickIcon(cands []*entry) *entry {
	var best *entry
	score := func(e *entry) int64 {
		base := strings.ToLower(path.Base(e.name))
		s := e.size
		if base == "ic_launcher.png" || base == "icon.png" || base == "game-icon.png" {
			s += 1 << 40
		}
		if strings.Contains(base, "foreground") || strings.Contains(base, "background") || strings.Contains(base, "monochrome") {
			s -= 1 << 41
		}
		return s
	}
	for _, e := range cands {
		if e.size <= 0 || e.size > 4<<20 {
			continue
		}
		if best == nil || score(e) > score(best) {
			best = e
		}
	}
	return best
}

// containsPackage checks whether a (binary) AndroidManifest.xml contains the package ID.
// Binary XML stores strings as UTF-16LE or UTF-8.
func containsPackage(manifest []byte) bool {
	if bytes.Contains(manifest, []byte(PackageName)) {
		return true
	}
	u := utf16.Encode([]rune(PackageName))
	b := make([]byte, 0, len(u)*2)
	for _, c := range u {
		b = append(b, byte(c), byte(c>>8))
	}
	return bytes.Contains(manifest, b)
}

func findFile(zr *zip.Reader, name string) *zip.File {
	for _, f := range zr.File {
		if f.Name == name {
			return f
		}
	}
	return nil
}

func readZipFile(f *zip.File, limit int64) ([]byte, error) {
	if f == nil {
		return nil, errors.New("missing")
	}
	r, err := f.Open()
	if err != nil {
		return nil, err
	}
	defer r.Close()
	data, err := io.ReadAll(io.LimitReader(r, limit+1))
	if err != nil {
		return nil, err
	}
	if int64(len(data)) > limit {
		return nil, errors.New("too large")
	}
	return data, nil
}

func zipEntry(f *zip.File, origin string) *entry {
	return &entry{name: f.Name, size: int64(f.UncompressedSize64), crc: f.CRC32, hasCRC: true, origin: origin,
		open: func() (io.ReadCloser, error) { return f.Open() }}
}

func diskEntry(p, name, origin string) *entry {
	var size int64
	if info, err := os.Stat(p); err == nil {
		size = info.Size()
	}
	return &entry{name: name, size: size, origin: origin,
		open: func() (io.ReadCloser, error) { return os.Open(p) }}
}

func hashEntry(e *entry) (string, error) {
	r, err := e.open()
	if err != nil {
		return "", err
	}
	defer r.Close()
	h := sha256.New()
	if _, err := io.Copy(h, r); err != nil {
		return "", err
	}
	return hex.EncodeToString(h.Sum(nil)), nil
}

func sortedVersions() []string {
	var vs []string
	for _, v := range KnownLibraries {
		vs = append(vs, v)
	}
	sort.Sort(sort.Reverse(sort.StringSlice(vs)))
	return vs
}

func fileExists(p string) bool {
	info, err := os.Stat(p)
	return err == nil && info.Mode().IsRegular()
}

func dirExists(p string) bool {
	info, err := os.Stat(p)
	return err == nil && info.IsDir()
}
