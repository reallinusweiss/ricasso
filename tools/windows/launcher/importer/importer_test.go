// SPDX-License-Identifier: GPL-3.0-or-later

package importer

import (
	"archive/zip"
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"unicode/utf16"
)

// Test library: valid ARM64 ELF header; the content is of course not a game.
func fakeLib(tag string) []byte {
	h := make([]byte, 64)
	copy(h, []byte{0x7f, 'E', 'L', 'F', 2, 1, 1})
	h[16], h[18] = 3, 183
	return append(h, []byte("TEST-LIBRARY-"+tag)...)
}

func sum(b []byte) string { s := sha256.Sum256(b); return hex.EncodeToString(s[:]) }

func registerFake(t *testing.T, lib []byte, version string) {
	t.Helper()
	k := sum(lib)
	KnownLibraries[k] = version
	t.Cleanup(func() { delete(KnownLibraries, k) })
}

func manifest(pkg string) []byte {
	u := utf16.Encode([]rune(pkg))
	b := []byte{0x03, 0x00, 0x08, 0x00, 0, 0, 0, 0}
	for _, c := range u {
		b = append(b, byte(c), byte(c>>8))
	}
	return append(b, 0, 0)
}

func zipBytes(t *testing.T, files map[string][]byte) []byte {
	t.Helper()
	var buf bytes.Buffer
	w := zip.NewWriter(&buf)
	for name, data := range files {
		f, err := w.Create(name)
		if err != nil {
			t.Fatal(err)
		}
		f.Write(data)
	}
	if err := w.Close(); err != nil {
		t.Fatal(err)
	}
	return buf.Bytes()
}

var tinyPNG = append([]byte("\x89PNG\r\n\x1a\n\x00\x00\x00\x0dIHDR\x00\x00\x00\xc0\x00\x00\x00\xc0"), make([]byte, 40)...)

func baseAPK(t *testing.T, pkg string, extra map[string][]byte) []byte {
	files := map[string][]byte{
		"AndroidManifest.xml":                              manifest(pkg),
		"classes.dex":                                      []byte("dex"),
		"assets/resources/gamedata.gdata":                  []byte("GAMEDATA"),
		"assets/resources/scenes/a.scene":                  []byte("SCENE-FILE"),
		"assets/resources/scenes/a.scene/x.png":            []byte("SCENE-RES"),
		"assets/resources/sounds/hit.ogg":                  []byte("OGG"),
		"res/raw/music_title.mp3":                          []byte("MP3"),
		"res/raw/other.bin":                                []byte("no"),
		"res/mipmap-xxxhdpi-v4/ic_launcher.png":            tinyPNG,
		"res/mipmap-xxxhdpi-v4/ic_launcher_foreground.png": append(append([]byte(nil), tinyPNG...), make([]byte, 400)...),
	}
	for k, v := range extra {
		files[k] = v
	}
	return zipBytes(t, files)
}

func splitAPK(t *testing.T, abi string, lib []byte) []byte {
	return zipBytes(t, map[string][]byte{
		"AndroidManifest.xml":            manifest(PackageName),
		"lib/" + abi + "/libswordigo.so": lib,
	})
}

func write(t *testing.T, dir, name string, data []byte) string {
	t.Helper()
	p := filepath.Join(dir, name)
	os.MkdirAll(filepath.Dir(p), 0o755)
	if err := os.WriteFile(p, data, 0o644); err != nil {
		t.Fatal(err)
	}
	return p
}

func clientDir(t *testing.T) string {
	t.Helper()
	dir := filepath.Join(t.TempDir(), "Swordigo Windows x64", "app")
	for _, rel := range RequiredClientFiles {
		write(t, dir, rel, []byte("x"))
	}
	write(t, dir, "Start-Swordigo.cmd", []byte("swordfare.exe --lib engine/v1.4.13/arm64-v8a/libswordigo.so --assets assets\r\n"))
	return dir
}

func wantUserErr(t *testing.T, err error, title string) {
	t.Helper()
	var ue *UserError
	if !errors.As(err, &ue) {
		t.Fatalf("expected UserError %q, got: %v", title, err)
	}
	if ue.Title != title {
		t.Fatalf("expected %q, got %q (%s)", title, ue.Title, ue.Message)
	}
}

func TestAPKMImportAndInstall(t *testing.T) {
	lib := fakeLib("13")
	registerFake(t, lib, "1.4.13")
	src := t.TempDir()
	apkm := write(t, src, "Swordigo_1.4.13.apkm", zipBytes(t, map[string][]byte{
		"info.json":                  []byte(`{"pname":"com.touchfoo.swordigo","release_version":"1.4.13"}`),
		"base.apk":                   baseAPK(t, PackageName, nil),
		"split_config.arm64_v8a.apk": splitAPK(t, "arm64-v8a", lib),
		"split_config.xxhdpi.apk":    zipBytes(t, map[string][]byte{"AndroidManifest.xml": manifest(PackageName)}),
	}))
	var calls int
	s, err := ScanInputs([]string{apkm}, func(string, int64, int64) { calls++ })
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	if s.Version != "1.4.13" || s.DeclaredVersion != "1.4.13" || s.MusicCount != 1 || s.AssetCount != 4 {
		t.Fatalf("wrong scan: %+v", s)
	}
	app := clientDir(t)
	res, err := Install(s, app, nil)
	if err != nil {
		t.Fatal(err)
	}
	if !res.HasIcon {
		t.Fatal("icon not taken over")
	}
	mustContent := map[string]string{
		"engine/v1.4.13/arm64-v8a/libswordigo.so":   string(lib),
		"assets/resources/gamedata.gdata":           "GAMEDATA",
		"assets/resources/scenes/a.scene":           "SCENE-FILE",
		"assets/resources/scenes/a.scene_res/x.png": "SCENE-RES",
		"res/raw/music_title.mp3":                   "MP3",
	}
	for rel, want := range mustContent {
		got, err := os.ReadFile(filepath.Join(app, filepath.FromSlash(rel)))
		if err != nil || string(got) != want {
			t.Fatalf("%s: %q %v", rel, got, err)
		}
	}
	for _, rel := range []string{"res/raw/other.bin", "classes.dex", StagingDir} {
		if _, err := os.Stat(filepath.Join(app, filepath.FromSlash(rel))); err == nil {
			t.Fatalf("%s should not have been copied", rel)
		}
	}
	ico, _ := os.ReadFile(filepath.Join(app, iconFile))
	if len(ico) != 22+len(tinyPNG) || ico[6] != 192 {
		t.Fatalf("wrong ICO: %d bytes", len(ico))
	}
	var rec map[string]any
	data, _ := os.ReadFile(filepath.Join(app, recordFile))
	if json.Unmarshal(data, &rec) != nil || rec["version"] != "1.4.13" {
		t.Fatalf("wrong game-files.json: %s", data)
	}
	st := DetectInstalled(app)
	if !st.OK || st.Version != "1.4.13" {
		t.Fatalf("DetectInstalled: %+v", st)
	}
	if strings.Join(GameArgs(st), " ") != "--no-launcher --dynarmic --lib engine/v1.4.13/arm64-v8a/libswordigo.so --assets assets" {
		t.Fatal(GameArgs(st))
	}
	if calls == 0 {
		t.Fatal("no progress reported")
	}
}

func TestMultipleAPKsAnd1412(t *testing.T) {
	lib := fakeLib("12")
	registerFake(t, lib, "1.4.12")
	src := t.TempDir()
	a := write(t, src, "base.apk", baseAPK(t, PackageName, nil))
	b := write(t, src, "split_config.arm64_v8a.apk", splitAPK(t, "arm64-v8a", lib))
	s, err := ScanInputs([]string{a, b}, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	app := clientDir(t)
	if _, err := Install(s, app, nil); err != nil {
		t.Fatal(err)
	}
	st := DetectInstalled(app)
	if !st.OK || st.Version != "1.4.12" {
		t.Fatalf("%+v", st)
	}
	cmd, _ := os.ReadFile(filepath.Join(app, "Start-Swordigo.cmd"))
	if !strings.Contains(string(cmd), "engine/v1.4.12/") {
		t.Fatalf("start script not adapted: %s", cmd)
	}
}

func TestFolderWithAPKsAndXAPK(t *testing.T) {
	lib := fakeLib("13")
	registerFake(t, lib, "1.4.13")
	src := t.TempDir()
	write(t, src, "Export/Swordigo.xapk", zipBytes(t, map[string][]byte{
		"manifest.json":             []byte(`{"package_name":"com.touchfoo.swordigo","version_name":"1.4.13"}`),
		"com.touchfoo.swordigo.apk": baseAPK(t, PackageName, nil),
		"config.arm64_v8a.apk":      splitAPK(t, "arm64-v8a", lib),
		"icon.png":                  tinyPNG,
	}))
	s, err := ScanInputs([]string{src}, nil)
	if err != nil {
		t.Fatal(err)
	}
	s.Close()
}

func TestSingleUniversalAPK(t *testing.T) {
	lib := fakeLib("13")
	registerFake(t, lib, "1.4.13")
	src := t.TempDir()
	p := write(t, src, "swordigo.apk", baseAPK(t, PackageName, map[string][]byte{
		LibEntry:                         lib,
		"lib/armeabi-v7a/libswordigo.so": []byte("arm32"),
	}))
	s, err := ScanInputs([]string{p}, nil)
	if err != nil {
		t.Fatal(err)
	}
	s.Close()
}

func TestErrors(t *testing.T) {
	lib := fakeLib("13")
	registerFake(t, lib, "1.4.13")
	src := t.TempDir()
	base := write(t, src, "base.apk", baseAPK(t, PackageName, nil))

	_, err := ScanInputs([]string{base}, nil)
	wantUserErr(t, err, "Part missing")

	v7 := write(t, src, "split_config.armeabi_v7a.apk", splitAPK(t, "armeabi-v7a", []byte("arm32")))
	_, err = ScanInputs([]string{base, v7}, nil)
	wantUserErr(t, err, "Wrong device variant")

	other := write(t, src, "other.apkm", zipBytes(t, map[string][]byte{
		"info.json": []byte(`{"pname":"com.example.game","release_version":"2.0"}`),
		"base.apk":  baseAPK(t, "com.example.game", nil),
	}))
	_, err = ScanInputs([]string{other}, nil)
	wantUserErr(t, err, "This isn't Swordigo")
	if !strings.Contains(err.Error(), "com.example.game") {
		t.Fatal(err)
	}

	newer := write(t, src, "new.apkm", zipBytes(t, map[string][]byte{
		"info.json":                  []byte(`{"pname":"com.touchfoo.swordigo","release_version":"1.5.0"}`),
		"base.apk":                   baseAPK(t, PackageName, nil),
		"split_config.arm64_v8a.apk": splitAPK(t, "arm64-v8a", fakeLib("unknown")),
	}))
	_, err = ScanInputs([]string{newer}, nil)
	wantUserErr(t, err, "Version not supported")
	if !strings.Contains(err.Error(), "1.5.0") {
		t.Fatal(err)
	}

	modified := write(t, src, "modified.apkm", zipBytes(t, map[string][]byte{
		"info.json":                  []byte(`{"pname":"com.touchfoo.swordigo","release_version":"1.4.13"}`),
		"base.apk":                   baseAPK(t, PackageName, nil),
		"split_config.arm64_v8a.apk": splitAPK(t, "arm64-v8a", fakeLib("patched")),
	}))
	_, err = ScanInputs([]string{modified}, nil)
	wantUserErr(t, err, "File modified or damaged")

	txt := write(t, src, "note.txt", []byte("hello"))
	_, err = ScanInputs([]string{txt}, nil)
	wantUserErr(t, err, "Wrong file type")

	broken := write(t, src, "broken.apks", []byte("PK\x03\x04truncated"))
	_, err = ScanInputs([]string{broken}, nil)
	wantUserErr(t, err, "File damaged")

	arm := write(t, src, "split_config.arm64_v8a.apk", splitAPK(t, "arm64-v8a", lib))
	evil := write(t, src, "evil.apk", baseAPK(t, PackageName, map[string][]byte{"assets/../../evil.txt": []byte("x")}))
	_, err = ScanInputs([]string{evil, arm}, nil)
	wantUserErr(t, err, "Invalid file name")

	nogd := write(t, src, "nogd.apk", zipBytes(t, map[string][]byte{"AndroidManifest.xml": manifest(PackageName), "assets/x": []byte("1")}))
	_, err = ScanInputs([]string{nogd, arm}, nil)
	wantUserErr(t, err, "Part missing")

	conflict := write(t, src, "conflict.apk", zipBytes(t, map[string][]byte{"AndroidManifest.xml": manifest(PackageName), "assets/resources/gamedata.gdata": []byte("DIFFERENT")}))
	_, err = ScanInputs([]string{base, conflict, arm}, nil)
	wantUserErr(t, err, "Mixed versions")

	_, err = ScanInputs([]string{filepath.Join(src, "doesnotexist.apkm")}, nil)
	wantUserErr(t, err, "File not found")

	empty := t.TempDir()
	_, err = ScanInputs([]string{empty}, nil)
	wantUserErr(t, err, "Nothing found")

	// Duplicate but identical files in several APKs are allowed.
	dup := write(t, src, "dup.apk", zipBytes(t, map[string][]byte{"AndroidManifest.xml": manifest(PackageName), "assets/resources/gamedata.gdata": []byte("GAMEDATA")}))
	s, err := ScanInputs([]string{base, dup, arm}, nil)
	if err != nil {
		t.Fatal(err)
	}
	s.Close()
}

func TestTakeOverPreviousInstallAndReinstall(t *testing.T) {
	lib := fakeLib("13")
	registerFake(t, lib, "1.4.13")
	src := t.TempDir()
	a := write(t, src, "base.apk", baseAPK(t, PackageName, nil))
	b := write(t, src, "split_config.arm64_v8a.apk", splitAPK(t, "arm64-v8a", lib))
	s, err := ScanInputs([]string{a, b}, nil)
	if err != nil {
		t.Fatal(err)
	}
	old := clientDir(t)
	if _, err := Install(s, old, nil); err != nil {
		t.Fatal(err)
	}
	s.Close()
	write(t, old, "assets/stale-from-old.txt", []byte("old"))

	// New client version: take over game files from the old main folder.
	fresh := clientDir(t)
	s2, err := ScanInputs([]string{filepath.Dir(old)}, nil)
	if err != nil {
		t.Fatal(err)
	}
	if s2.Version != "1.4.13" || s2.MusicCount != 1 {
		t.Fatalf("%+v", s2)
	}
	if _, err := Install(s2, fresh, nil); err != nil {
		t.Fatal(err)
	}
	s2.Close()
	got, _ := os.ReadFile(filepath.Join(fresh, "assets/resources/scenes/a.scene_res/x.png"))
	if string(got) != "SCENE-RES" || !DetectInstalled(fresh).OK || !DetectInstalled(fresh).HasIcon {
		t.Fatal("takeover incomplete")
	}

	// Setting up again in the same folder replaces old files completely.
	s3, _ := ScanInputs([]string{a, b}, nil)
	if _, err := Install(s3, old, nil); err != nil {
		t.Fatal(err)
	}
	s3.Close()
	if _, err := os.Stat(filepath.Join(old, "assets/stale-from-old.txt")); err == nil {
		t.Fatal("old file not removed")
	}
}

func TestPathTooLongAndMissingClient(t *testing.T) {
	lib := fakeLib("13")
	registerFake(t, lib, "1.4.13")
	src := t.TempDir()
	a := write(t, src, "base.apk", baseAPK(t, PackageName, nil))
	b := write(t, src, "split_config.arm64_v8a.apk", splitAPK(t, "arm64-v8a", lib))
	s, err := ScanInputs([]string{a, b}, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	deep := filepath.Join(t.TempDir(), strings.Repeat("very-long-folder-name-xx-", 10), "app")
	for _, rel := range RequiredClientFiles {
		write(t, deep, rel, []byte("x"))
	}
	_, err = Install(s, deep, nil)
	wantUserErr(t, err, "Folder path too long")

	_, err = Install(s, t.TempDir(), nil)
	wantUserErr(t, err, "Program files missing")

	if len(MissingClientFiles(t.TempDir())) != len(RequiredClientFiles) {
		t.Fatal("missing files not detected")
	}
	FreeSpace = func(string) (uint64, error) { return 1024, nil }
	defer func() { FreeSpace = nil }()
	_, err = Install(s, clientDir(t), nil)
	wantUserErr(t, err, "Not enough disk space")
}

func TestCheckName(t *testing.T) {
	for _, bad := range []string{"assets/a:b", "assets/con.txt", "assets/x./y", "assets//y", "/abs", "assets/a\\b", "assets/q?"} {
		if checkName(bad) == nil {
			t.Errorf("%q should have been rejected", bad)
		}
	}
	for _, ok := range []string{"assets/resources/a.scene", "res/raw/music_x.mp3", "assets/Ünïcode ok.png", "assets/console.txt"} {
		if checkName(ok) != nil {
			t.Errorf("%q should have been allowed", ok)
		}
	}
}
