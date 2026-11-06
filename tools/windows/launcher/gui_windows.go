//go:build windows

// SPDX-License-Identifier: GPL-3.0-or-later

package main

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"runtime/debug"
	"strings"
	"sync"
	"sync/atomic"
	"unicode/utf16"
	"unsafe"

	"golang.org/x/sys/windows"

	"ricasso-launcher/importer"
)

const className = "RicassoStarter"

type page int

const (
	pgSetup page = 1 << iota
	pgBusy
	pgReady
	pgError
	pgHelp
)

// Custom window messages (posted from background goroutines).
const (
	msgProgress = wmApp + 1 + iota
	msgImportDone
	msgGameStarted
	msgGameExited
	msgGameFailed
	msgCopyDone
	msgStartup
)

// Fonts
const (
	fNormal = iota
	fTitle
	fBold
	fSmall
	fDrop
	fMono
	fCount
)

// Logical layout at 96 dpi.
const (
	clientW = 600
	clientH = 440
)

var dropRect = rect{24, 150, 576, 258}

type control struct {
	hwnd       uintptr
	id         int
	x, y, w, h int
	pages      page
	font       int
	color      uintptr
	hidden     bool // additionally hidden (independent of the page)
	onClick    func()
	isCmdLink  bool
	isProgress bool
}

type gameFailure struct {
	title, hint string
	showLog     bool
	retry       bool
}

type app struct {
	hwnd    uintptr
	inst    uintptr
	dpi     int
	page    page
	back    page // page to return to from help/setup
	exePath string
	exeDir  string
	appDir  string
	extra   []string
	mutex   windows.Handle

	controls []*control
	byHwnd   map[uintptr]*control
	byID     map[int]*control
	fonts    [fCount]uintptr
	white    uintptr
	dropBg   uintptr
	nextID   int

	installed   importer.Installed
	prevInstall string
	warnings    []string
	busy        bool

	// Progress / results from goroutines
	mu          sync.Mutex
	progText    string
	progDone    int64
	progTotal   int64
	progPending int32
	importRes   *importer.Result
	importErr   error
	failure     gameFailure
	exitCode    uint32
	copyErr     error
	copyTarget  string
	logPath     string

	// Controls
	title, subtitle, footer                                                     *control
	intro, cmdChoose, cmdHelp, cmdPrev, btnSetupBack                            *control
	busyLbl, busyBar, busyMarquee, busyDetail                                   *control
	status, cmdPlay, controlsLbl, btnShortcut, btnSaves, btnLog, btnRe, warnLbl *control
	errTitle, errHint, errLog, btnRetry, btnOpenLog, btnErrBack                 *control
	helpText, btnHelpChoose, btnHelpBack                                        *control
}

var theApp *app

func (a *app) s(v int) int32 { return int32(mulDiv(v, a.dpi, 96)) }

func (a *app) add(class, text string, style, exStyle uint32, x, y, w, h int, pages page, font int) *control {
	a.nextID++
	c := &control{id: 100 + a.nextID, x: x, y: y, w: w, h: h, pages: pages, font: font}
	c.hwnd = call(pCreateWindowExW, uintptr(exStyle), uintptr(unsafe.Pointer(u16(class))), uintptr(unsafe.Pointer(u16(text))),
		uintptr(style|wsChild), 0, 0, 10, 10, a.hwnd, uintptr(c.id), a.inst, 0)
	a.controls = append(a.controls, c)
	a.byHwnd[c.hwnd] = c
	a.byID[c.id] = c
	return c
}

func (a *app) label(text string, x, y, w, h int, pages page, font int) *control {
	return a.add("STATIC", text, ssLeft|ssNoPrefix, 0, x, y, w, h, pages, font)
}

func (a *app) button(text string, x, y, w, h int, pages page, onClick func()) *control {
	c := a.add("BUTTON", text, bsPushButton|wsTabStop, 0, x, y, w, h, pages, fNormal)
	c.onClick = onClick
	return c
}

func (a *app) cmdLink(text, note string, x, y, w, h int, pages page, onClick func()) *control {
	c := a.add("BUTTON", text, bsCommandLink|wsTabStop, 0, x, y, w, h, pages, fNormal)
	c.isCmdLink = true
	c.onClick = onClick
	a.setNote(c, note)
	return c
}

func (a *app) setNote(c *control, note string) {
	sendMessage(c.hwnd, bcmSetNote, 0, uintptr(unsafe.Pointer(u16(note))))
}

func (a *app) createControls() {
	all := pgSetup | pgBusy | pgReady | pgError | pgHelp
	grey := rgb(100, 100, 100)

	a.title = a.label("Ricasso", 24, 16, 552, 40, all, fTitle)
	a.subtitle = a.label("", 24, 56, 552, 22, all, fNormal)
	a.subtitle.color = grey
	a.footer = a.label("Unofficial fan project, not made by Touch Foo. Swordigo © Touch Foo – game files are not included.", 24, 414, 552, 18, all, fSmall)
	a.footer.color = rgb(120, 120, 120)

	// Setup
	a.intro = a.label("To play, you need your own copy of Swordigo for Android "+
		"(supported: "+importer.SupportedVersionsText+"). This program contains no game files and downloads nothing from the internet.",
		24, 88, 552, 54, pgSetup, fNormal)
	a.cmdChoose = a.cmdLink("Choose file…", "Your own copy of Swordigo (.apkm, .apks, .xapk or .apk)", 24, 270, 270, 70, pgSetup, a.chooseFile)
	a.cmdHelp = a.cmdLink("How do I get the file?", "Guide: get Swordigo from your own phone", 306, 270, 270, 70, pgSetup, func() { a.showPage(pgHelp) })
	a.cmdPrev = a.cmdLink("Reuse previous installation", "", 24, 348, 400, 58, pgSetup, a.takeOverPrevious)
	a.btnSetupBack = a.button("Back", 436, 360, 140, 34, pgSetup, func() { a.showPage(pgReady) })

	// Busy
	a.busyLbl = a.label("", 24, 150, 552, 26, pgBusy, fBold)
	a.busyBar = a.add("msctls_progress32", "", 0, 0, 24, 186, 552, 20, pgBusy, fNormal)
	a.busyBar.isProgress = true
	a.busyMarquee = a.add("msctls_progress32", "", pbsMarquee, 0, 24, 186, 552, 20, pgBusy, fNormal)
	a.busyMarquee.isProgress = true
	a.busyDetail = a.label("", 24, 214, 552, 44, pgBusy, fNormal)
	a.busyDetail.color = grey
	sendMessage(a.busyBar.hwnd, pbmSetRange32, 0, 1000)
	sendMessage(a.busyMarquee.hwnd, pbmSetMarquee, 1, 30)

	// Ready
	a.status = a.label("", 24, 88, 552, 24, pgReady, fBold)
	a.status.color = rgb(24, 120, 48)
	a.cmdPlay = a.cmdLink("Play", "", 24, 120, 552, 74, pgReady, a.play)
	a.controlsLbl = a.label("Controls (keyboard)\r\n"+
		"A / D or arrow keys: move   ·   Space or W: jump   ·   Attack/magic: see F2\r\n"+
		"F1: menu   ·   F2: change keys (R = rebind, F2 = save)   ·   F3: performance overlay",
		24, 206, 552, 62, pgReady, fNormal)
	a.btnShortcut = a.button("Desktop shortcut", 24, 282, 132, 34, pgReady, a.createShortcuts)
	a.btnSaves = a.button("Saves", 164, 282, 132, 34, pgReady, a.openSaves)
	a.btnLog = a.button("Log", 304, 282, 132, 34, pgReady, a.openLog)
	a.btnRe = a.button("Set up again…", 444, 282, 132, 34, pgReady, func() { a.showPage(pgSetup) })
	a.warnLbl = a.label("", 24, 330, 552, 74, pgReady, fNormal)
	a.warnLbl.color = rgb(150, 90, 0)

	// Error while playing
	a.errTitle = a.label("", 24, 88, 552, 24, pgError, fBold)
	a.errTitle.color = rgb(176, 32, 32)
	a.errHint = a.label("", 24, 114, 552, 66, pgError, fNormal)
	a.errLog = a.add("EDIT", "", esMultiline|esReadOnly|esAutoVScroll|wsVScroll|wsTabStop, wsExClientEdge, 24, 184, 552, 162, pgError, fMono)
	a.btnRetry = a.button("Try again", 24, 358, 170, 34, pgError, a.play)
	a.btnOpenLog = a.button("Open log", 204, 358, 170, 34, pgError, a.openLog)
	a.btnErrBack = a.button("Back", 436, 358, 140, 34, pgError, func() { a.refresh() })

	// Help
	a.helpText = a.add("EDIT", strings.ReplaceAll(helpText, "\n", "\r\n"), esMultiline|esReadOnly|esAutoVScroll|wsVScroll|wsTabStop, wsExClientEdge,
		24, 88, 552, 262, pgHelp, fNormal)
	a.btnHelpChoose = a.button("Choose file…", 24, 360, 200, 34, pgHelp, a.chooseFile)
	a.btnHelpBack = a.button("Back", 436, 360, 140, 34, pgHelp, func() { a.showPage(pgSetup) })
}

func (a *app) makeFonts() {
	for i, f := range a.fonts {
		if f != 0 {
			call(pDeleteObject, f)
			a.fonts[i] = 0
		}
	}
	mk := func(pt float64, weight int, face string) uintptr {
		h := -int(pt*float64(a.dpi)/72 + 0.5)
		return call(pCreateFontW, uintptr(h), 0, 0, 0, uintptr(weight), 0, 0, 0, defaultCharset, 0, 0, clearTypeQuality, 0,
			uintptr(unsafe.Pointer(u16(face))))
	}
	a.fonts[fNormal] = mk(9, fwNormal, "Segoe UI")
	a.fonts[fTitle] = mk(18, fwSemiBold, "Segoe UI")
	a.fonts[fBold] = mk(10.5, fwSemiBold, "Segoe UI")
	a.fonts[fSmall] = mk(8, fwNormal, "Segoe UI")
	a.fonts[fDrop] = mk(12, fwSemiBold, "Segoe UI")
	a.fonts[fMono] = mk(8.5, fwNormal, "Consolas")
}

func (a *app) layout() {
	for _, c := range a.controls {
		call(pSetWindowPos, c.hwnd, 0, uintptr(a.s(c.x)), uintptr(a.s(c.y)), uintptr(a.s(c.w)), uintptr(a.s(c.h)), swpNoZOrder|swpNoActivate)
		sendMessage(c.hwnd, wmSetFont, a.fonts[c.font], 1)
	}
	call(pInvalidateRect, a.hwnd, 0, 1)
}

func (a *app) showPage(p page) {
	if p == pgSetup || p == pgHelp {
		a.updatePrevious()
	}
	a.page = p
	subtitles := map[page]string{
		pgSetup: "One-time setup: choose your own Swordigo file.",
		pgBusy:  "Please wait…",
		pgReady: "Unofficial compatibility layer for Swordigo",
		pgError: "Something went wrong.",
		pgHelp:  "How to get your Swordigo file – no terminal needed",
	}
	setText(a.subtitle.hwnd, subtitles[p])
	a.btnSetupBack.hidden = !a.installed.OK
	a.cmdPrev.hidden = a.prevInstall == ""
	for _, c := range a.controls {
		show := c.pages&p != 0 && !c.hidden
		if show {
			call(pShowWindow, c.hwnd, swShow)
		} else {
			call(pShowWindow, c.hwnd, swHide)
		}
	}
	call(pInvalidateRect, a.hwnd, 0, 1)
	focus := map[page]*control{pgSetup: a.cmdChoose, pgReady: a.cmdPlay, pgError: a.btnRetry, pgHelp: a.btnHelpChoose}
	if c := focus[p]; c != nil {
		call(pSetFocus, c.hwnd)
	}
}

// refresh re-detects the state and shows the matching page.
func (a *app) refresh() {
	a.installed = importer.DetectInstalled(a.appDir)
	if a.installed.OK {
		setText(a.status.hwnd, "Ready: Swordigo "+a.installed.Version+" is set up.")
		a.setNote(a.cmdPlay, "Starts Swordigo "+a.installed.Version+". The first start can take a few seconds.")
		setText(a.warnLbl.hwnd, strings.Join(a.warnings, "\r\n"))
		a.showPage(pgReady)
	} else {
		a.showPage(pgSetup)
	}
}

func (a *app) updatePrevious() {
	a.prevInstall = ""
	data, err := os.ReadFile(lastInstallFile())
	if err != nil {
		return
	}
	prev := strings.TrimSpace(string(data))
	if prev == "" || samePath(prev, a.appDir) {
		return
	}
	if importer.DetectInstalled(prev).OK {
		a.prevInstall = prev
		a.setNote(a.cmdPrev, prev)
	}
}

func (a *app) setBusy(headline, detail string) {
	a.busy = true
	setText(a.busyLbl.hwnd, headline)
	setText(a.busyDetail.hwnd, detail)
	a.showProgress(-1, -1)
	a.showPage(pgBusy)
}

func (a *app) showProgress(done, total int64) {
	if total > 0 {
		call(pShowWindow, a.busyMarquee.hwnd, swHide)
		call(pShowWindow, a.busyBar.hwnd, swShow)
		sendMessage(a.busyBar.hwnd, pbmSetPos, uintptr(done*1000/total), 0)
	} else {
		call(pShowWindow, a.busyBar.hwnd, swHide)
		call(pShowWindow, a.busyMarquee.hwnd, swShow)
	}
}

// reportProgress may be called from any goroutine.
func (a *app) reportProgress(text string, done, total int64) {
	a.mu.Lock()
	a.progText, a.progDone, a.progTotal = text, done, total
	a.mu.Unlock()
	if atomic.CompareAndSwapInt32(&a.progPending, 0, 1) {
		postMessage(a.hwnd, msgProgress, 0, 0)
	}
}

// ---------------------------------------------------------------- Setup

func (a *app) chooseFile() {
	if a.busy {
		return
	}
	paths, err := openFileDialog(a.hwnd)
	if err != nil {
		messageBox(a.hwnd, "Choose file", err.Error(), mbIconWarning)
		return
	}
	if len(paths) > 0 {
		a.startImport(paths)
	}
}

func (a *app) takeOverPrevious() {
	if a.prevInstall != "" {
		a.startImport([]string{a.prevInstall})
	}
}

func (a *app) startImport(paths []string) {
	if a.busy {
		return
	}
	names := make([]string, 0, len(paths))
	for i, p := range paths {
		if i == 6 {
			names = append(names, fmt.Sprintf("… and %d more", len(paths)-6))
			break
		}
		if dirExists(p) {
			names = append(names, "• Folder "+p)
		} else {
			names = append(names, "• "+filepath.Base(p))
		}
	}
	text := "Set up the game files from:\n\n" + strings.Join(names, "\n") + "\n\n" +
		"Please confirm: this is your own, lawfully obtained copy of Swordigo, " +
		"and you won't share the installed game files with others."
	if a.installed.OK {
		text += "\n\nThe game files set up so far will be replaced. Your saves are kept."
	}
	if messageBox(a.hwnd, "Set up Swordigo", text, mbYesNo|mbIconQuestion) != idYes {
		return
	}
	a.back = a.page
	a.setBusy("Setting up the game files…", "Checking your file…")
	go func() {
		var res *importer.Result
		var err error
		defer func() {
			if r := recover(); r != nil {
				err = fmt.Errorf("internal error: %v\n%s", r, debug.Stack())
			}
			a.mu.Lock()
			a.importRes, a.importErr = res, err
			a.mu.Unlock()
			postMessage(a.hwnd, msgImportDone, 0, 0)
		}()
		scan, err := importer.ScanInputs(paths, a.reportProgress)
		if err != nil {
			return
		}
		defer scan.Close()
		res, err = importer.Install(scan, a.appDir, a.reportProgress)
	}()
}

func (a *app) onImportDone() {
	a.busy = false
	a.mu.Lock()
	res, err := a.importRes, a.importErr
	a.mu.Unlock()
	if err != nil {
		if a.back == pgBusy || a.back == 0 {
			a.back = pgSetup
		}
		a.showPage(a.back)
		showError(a.hwnd, err)
		return
	}
	os.MkdirAll(filepath.Dir(lastInstallFile()), 0o755)
	os.WriteFile(lastInstallFile(), []byte(a.appDir), 0o644)
	a.warnings = res.Warnings
	a.refresh()
	text := fmt.Sprintf("Done! Swordigo %s is set up (%d game files", res.Version, res.AssetCount)
	if res.MusicCount > 0 {
		text += fmt.Sprintf(", %d music tracks", res.MusicCount)
	}
	text += ").\n\n"
	for _, w := range res.Warnings {
		text += "Note: " + w + "\n\n"
	}
	text += "Create a shortcut on the desktop and in the Start menu?\nThen you can start Swordigo with a double-click from now on."
	if messageBox(a.hwnd, "Swordigo is ready", text, mbYesNo|mbIconInfo) == idYes {
		a.createShortcuts()
	}
}

// ---------------------------------------------------------------- Play

func (a *app) play() {
	if a.busy {
		return
	}
	if missing := importer.MissingClientFiles(a.appDir); len(missing) > 0 {
		messageBox(a.hwnd, "Program files missing",
			"These program files are missing:\n\n"+strings.Join(missing, "\n")+
				"\n\nMost likely the ZIP file wasn't extracted completely, or an antivirus program removed a file. "+
				"Extract the ZIP file again completely (“Extract All…”). Afterwards you can reuse your game files with "+
				"“Reuse previous installation”.", mbIconError)
		return
	}
	a.installed = importer.DetectInstalled(a.appDir)
	if !a.installed.OK {
		messageBox(a.hwnd, "Game files missing", a.installed.Problem+"\n\nPlease set up Swordigo first.", mbIconWarning)
		a.showPage(pgSetup)
		return
	}
	a.setBusy("Starting Swordigo…", "Checking the game files…")
	st := a.installed
	go func() {
		defer func() {
			if r := recover(); r != nil {
				a.mu.Lock()
				a.failure = gameFailure{title: "Internal error", hint: fmt.Sprint(r)}
				a.mu.Unlock()
				postMessage(a.hwnd, msgGameFailed, 0, 0)
			}
		}()
		a.runGame(st)
	}()
}

func (a *app) onGameExited(code uint32) {
	a.busy = false
	if code == 0 || code == 0xC000013A {
		call(pDestroyWindow, a.hwnd)
		return
	}
	title, hint := describeExit(code)
	a.showFailure(gameFailure{title: title, hint: hint, showLog: true, retry: true})
}

func (a *app) showFailure(f gameFailure) {
	a.busy = false
	setText(a.errTitle.hwnd, f.title)
	setText(a.errHint.hwnd, f.hint)
	logText := "(No log available.)"
	if f.showLog {
		logText = tailFile(a.logPath, 40)
	}
	setText(a.errLog.hwnd, logText)
	a.showPage(pgError)
	n := len(utf16.Encode([]rune(logText)))
	sendMessage(a.errLog.hwnd, emSetSel, uintptr(n), uintptr(n))
	sendMessage(a.errLog.hwnd, emScrollCaret, 0, 0)
	call(pShowWindow, a.hwnd, swShow)
	call(pSetForegroundWindow, a.hwnd)
}

// ---------------------------------------------------------------- Odds and ends

func (a *app) createShortcuts() {
	icon, idx := a.exePath, 0
	if p := filepath.Join(a.appDir, "game-icon.ico"); fileExists(p) {
		icon = p
	}
	var made, failed []string
	for _, folder := range []*windows.KNOWNFOLDERID{windows.FOLDERID_Desktop, windows.FOLDERID_Programs} {
		dir, err := windows.KnownFolderPath(folder, 0)
		if err != nil {
			failed = append(failed, err.Error())
			continue
		}
		lnk := filepath.Join(dir, "Swordigo.lnk")
		if err := createShortcut(lnk, a.exePath, a.exeDir, "Swordigo (via Ricasso)", icon, idx); err != nil {
			failed = append(failed, lnk+": "+err.Error())
		} else {
			made = append(made, lnk)
		}
	}
	if len(failed) > 0 {
		messageBox(a.hwnd, "Shortcut", "Not all shortcuts could be created:\n\n"+strings.Join(failed, "\n"), mbIconWarning)
	} else {
		messageBox(a.hwnd, "Shortcut", "Shortcuts created:\n\n• Desktop\n• Start menu (search for “Swordigo”)", mbIconInfo)
	}
}

func (a *app) openSaves() {
	base := filepath.Join(os.Getenv("APPDATA"), "ricasso")
	for _, p := range []string{filepath.Join(base, "save", "Documents"), filepath.Join(base, "save"), base} {
		if dirExists(p) {
			shellOpen(p, "")
			return
		}
	}
	messageBox(a.hwnd, "Saves", "There are no saves yet. They are created here the first time you play:\n\n"+filepath.Join(base, "save"), mbIconInfo)
}

func (a *app) openLog() {
	p := logFilePath()
	if !fileExists(p) {
		messageBox(a.hwnd, "Log", "There is no log yet. It is created the first time the game starts:\n\n"+p, mbIconInfo)
		return
	}
	shellOpen("notepad.exe", `"`+p+`"`)
}

func showError(owner uintptr, err error) {
	var ue *importer.UserError
	if errors.As(err, &ue) {
		text := ue.Message
		if ue.Hint != "" {
			text += "\n\n" + ue.Hint
		}
		messageBox(owner, ue.Title, text, mbIconWarning)
		return
	}
	messageBox(owner, "Unexpected error", err.Error(), mbIconError)
}

// ---------------------------------------------------------------- Window

func (a *app) paintDrop(hdc uintptr) {
	r := rect{a.s(int(dropRect.Left)), a.s(int(dropRect.Top)), a.s(int(dropRect.Right)), a.s(int(dropRect.Bottom))}
	call(pFillRect, hdc, uintptr(unsafe.Pointer(&r)), a.dropBg)
	pen := call(pCreatePen, psDash, 1, rgb(120, 140, 170))
	oldPen := call(pSelectObject, hdc, pen)
	oldBrush := call(pSelectObject, hdc, call(pGetStockObject, nullBrush))
	rad := uintptr(a.s(12))
	call(pRoundRect, hdc, uintptr(r.Left), uintptr(r.Top), uintptr(r.Right), uintptr(r.Bottom), rad, rad)
	call(pSelectObject, hdc, oldPen)
	call(pSelectObject, hdc, oldBrush)
	call(pDeleteObject, pen)

	call(pSetBkMode, hdc, transparent)
	line := func(text string, font int, color uintptr, top, bottom int) {
		old := call(pSelectObject, hdc, a.fonts[font])
		call(pSetTextColor, hdc, color)
		lr := rect{r.Left, a.s(top), r.Right, a.s(bottom)}
		call(pDrawTextW, hdc, uintptr(unsafe.Pointer(u16(text))), ^uintptr(0), uintptr(unsafe.Pointer(&lr)), dtCenter|dtVCenter|dtSingleLine|dtNoPrefix)
		call(pSelectObject, hdc, old)
	}
	line("Drag your Swordigo file here", fDrop, rgb(30, 50, 80), 164, 200)
	line("or click here  ·  .apkm  ·  .apks  ·  .xapk  ·  .apk", fNormal, rgb(90, 100, 115), 200, 224)
	line("Several APK files? Just select them all and drag them here together.", fSmall, rgb(110, 120, 135), 224, 246)
}

func (a *app) inDrop(x, y int32) bool {
	return x >= a.s(int(dropRect.Left)) && x < a.s(int(dropRect.Right)) && y >= a.s(int(dropRect.Top)) && y < a.s(int(dropRect.Bottom))
}

func (a *app) primaryAction() {
	switch a.page {
	case pgSetup:
		a.chooseFile()
	case pgReady:
		a.play()
	case pgError:
		a.play()
	case pgHelp:
		a.showPage(pgSetup)
	}
}

func wndProc(hwnd uintptr, m uint32, wp, lp uintptr) (ret uintptr) {
	a := theApp
	if a == nil || a.hwnd == 0 {
		return call(pDefWindowProcW, hwnd, uintptr(m), wp, lp)
	}
	defer func() {
		if r := recover(); r != nil {
			messageBox(hwnd, "Internal error", fmt.Sprintf("%v\n\n%s", r, debug.Stack()), mbIconError)
			ret = 0
		}
	}()
	switch m {
	case wmCommand:
		id := int(loword(wp))
		if id == idOK {
			a.primaryAction()
			return 0
		}
		if id == idCancel {
			return 0
		}
		if c := a.byID[id]; c != nil && c.onClick != nil && hiword(wp) == 0 {
			c.onClick()
		}
		return 0
	case wmCtlColorStatic, wmCtlColorBtn:
		color := rgb(28, 28, 28)
		if c := a.byHwnd[lp]; c != nil && c.color != 0 {
			color = c.color
		}
		call(pSetTextColor, wp, color)
		call(pSetBkColor, wp, rgb(255, 255, 255))
		return a.white
	case wmPaint:
		var ps paintStruct
		hdc := call(pBeginPaint, hwnd, uintptr(unsafe.Pointer(&ps)))
		if a.page == pgSetup {
			a.paintDrop(hdc)
		}
		call(pEndPaint, hwnd, uintptr(unsafe.Pointer(&ps)))
		return 0
	case wmSetCursor:
		if a.page == pgSetup && wp == hwnd && loword(lp) == htClient {
			var pt point
			call(pGetCursorPos, uintptr(unsafe.Pointer(&pt)))
			call(pScreenToClient, hwnd, uintptr(unsafe.Pointer(&pt)))
			if a.inDrop(pt.X, pt.Y) {
				call(pSetCursor, call(pLoadCursorW, 0, idcHand))
				return 1
			}
		}
	case wmLButtonUp:
		if a.page == pgSetup && a.inDrop(int32(int16(loword(lp))), int32(int16(hiword(lp)))) {
			a.chooseFile()
			return 0
		}
	case wmDropFiles:
		paths := droppedFiles(wp)
		if a.busy || len(paths) == 0 {
			return 0
		}
		call(pSetForegroundWindow, hwnd)
		a.startImport(paths)
		return 0
	case wmDpiChanged:
		a.dpi = int(loword(wp))
		r := (*rect)(unsafe.Pointer(lp))
		call(pSetWindowPos, hwnd, 0, uintptr(r.Left), uintptr(r.Top), uintptr(r.Right-r.Left), uintptr(r.Bottom-r.Top), swpNoZOrder|swpNoActivate)
		a.makeFonts()
		a.layout()
		return 0
	case msgProgress:
		atomic.StoreInt32(&a.progPending, 0)
		a.mu.Lock()
		text, done, total := a.progText, a.progDone, a.progTotal
		a.mu.Unlock()
		if a.page == pgBusy {
			setText(a.busyDetail.hwnd, text)
			a.showProgress(done, total)
		}
		return 0
	case msgImportDone:
		a.onImportDone()
		return 0
	case msgGameStarted:
		call(pShowWindow, hwnd, swHide)
		return 0
	case msgGameExited:
		a.onGameExited(uint32(wp))
		return 0
	case msgGameFailed:
		a.mu.Lock()
		f := a.failure
		a.mu.Unlock()
		a.showFailure(f)
		return 0
	case msgCopyDone:
		a.onCopyDone()
		return 0
	case msgStartup:
		a.startupChecks()
		return 0
	case wmClose:
		call(pDestroyWindow, hwnd)
		return 0
	case wmDestroy:
		call(pPostQuitMessage, 0)
		return 0
	}
	return call(pDefWindowProcW, hwnd, uintptr(m), wp, lp)
}

func (a *app) createWindow() error {
	a.inst = call(pGetModuleHandleW, 0)
	a.byHwnd = map[uintptr]*control{}
	a.byID = map[int]*control{}
	a.white = call(pGetStockObject, whiteBrush)
	a.dropBg = call(pCreateSolidBrush, rgb(243, 246, 250))

	sysDPI := 96
	if pGetDpiForSystem.Find() == nil {
		sysDPI = int(call(pGetDpiForSystem))
	}
	icon := call(pLoadImageW, a.inst, 1, imageIcon, uintptr(mulDiv(32, sysDPI, 96)), uintptr(mulDiv(32, sysDPI, 96)), lrDefaultColor)
	iconSm := call(pLoadImageW, a.inst, 1, imageIcon, uintptr(mulDiv(16, sysDPI, 96)), uintptr(mulDiv(16, sysDPI, 96)), lrDefaultColor)
	wc := wndClassEx{
		Style:      0x3, // CS_HREDRAW | CS_VREDRAW
		WndProc:    newCallback(wndProc),
		Instance:   a.inst,
		Icon:       icon,
		Cursor:     call(pLoadCursorW, 0, idcArrow),
		Background: a.white,
		ClassName:  u16(className),
		IconSm:     iconSm,
	}
	wc.Size = uint32(unsafe.Sizeof(wc))
	if call(pRegisterClassExW, uintptr(unsafe.Pointer(&wc))) == 0 {
		return errors.New("RegisterClassEx failed")
	}
	style := uint32(wsOverlapped | wsCaption | wsSysMenu | wsMinimizeBox | wsClipChildren)
	ex := uint32(wsExControlPar | wsExAcceptFiles)
	a.hwnd = call(pCreateWindowExW, uintptr(ex), uintptr(unsafe.Pointer(u16(className))), uintptr(unsafe.Pointer(u16("Ricasso"))),
		uintptr(style), cwUseDefault, cwUseDefault, 640, 480, 0, 0, a.inst, 0)
	if a.hwnd == 0 {
		return errors.New("CreateWindowEx failed")
	}
	a.dpi = sysDPI
	if pGetDpiForWindow.Find() == nil {
		if d := int(call(pGetDpiForWindow, a.hwnd)); d > 0 {
			a.dpi = d
		}
	}
	call(pDragAcceptFiles, a.hwnd, 1)
	a.makeFonts()
	a.createControls()
	a.layout()

	// Window size matching the DPI, centered on the screen.
	r := rect{0, 0, a.s(clientW), a.s(clientH)}
	if pAdjustWindowRectExForDpi.Find() == nil {
		call(pAdjustWindowRectExForDpi, uintptr(unsafe.Pointer(&r)), uintptr(style), 0, uintptr(ex), uintptr(a.dpi))
	} else {
		call(pAdjustWindowRectEx, uintptr(unsafe.Pointer(&r)), uintptr(style), 0, uintptr(ex))
	}
	w, h := r.Right-r.Left, r.Bottom-r.Top
	mi := monitorInfo{}
	mi.Size = uint32(unsafe.Sizeof(mi))
	mon := call(pMonitorFromWindow, a.hwnd, monitorNearest)
	x, y := int32(100), int32(100)
	if call(pGetMonitorInfoW, mon, uintptr(unsafe.Pointer(&mi))) != 0 {
		x = mi.Work.Left + (mi.Work.Right-mi.Work.Left-w)/2
		y = mi.Work.Top + (mi.Work.Bottom-mi.Work.Top-h)/2
	}
	call(pSetWindowPos, a.hwnd, 0, uintptr(x), uintptr(y), uintptr(w), uintptr(h), swpNoZOrder)
	return nil
}

func (a *app) run() int {
	a.refresh()
	call(pShowWindow, a.hwnd, swShow)
	call(pUpdateWindow, a.hwnd)
	call(pSetForegroundWindow, a.hwnd)
	postMessage(a.hwnd, msgStartup, 0, 0)
	var m msg
	for {
		r := call(pGetMessageW, uintptr(unsafe.Pointer(&m)), 0, 0, 0)
		if r == 0 || int32(r) == -1 {
			break
		}
		if call(pIsDialogMessageW, a.hwnd, uintptr(unsafe.Pointer(&m))) != 0 {
			continue
		}
		call(pTranslateMessage, uintptr(unsafe.Pointer(&m)))
		call(pDispatchMessageW, uintptr(unsafe.Pointer(&m)))
	}
	return 0
}

func droppedFiles(hdrop uintptr) []string {
	defer call(pDragFinish, hdrop)
	n := int(call(pDragQueryFileW, hdrop, 0xFFFFFFFF, 0, 0))
	var out []string
	for i := 0; i < n; i++ {
		l := int(call(pDragQueryFileW, hdrop, uintptr(i), 0, 0))
		if l <= 0 {
			continue
		}
		buf := make([]uint16, l+1)
		call(pDragQueryFileW, hdrop, uintptr(i), uintptr(unsafe.Pointer(&buf[0])), uintptr(len(buf)))
		out = append(out, windows.UTF16ToString(buf))
	}
	return out
}

func openFileDialog(owner uintptr) ([]string, error) {
	filter := utf16.Encode([]rune("Swordigo package (*.apkm; *.apks; *.xapk; *.apk)\x00*.apkm;*.apks;*.xapk;*.apk\x00All files (*.*)\x00*.*\x00\x00"))
	buf := make([]uint16, 1<<16)
	ofn := openFileName{
		Owner:   owner,
		Filter:  &filter[0],
		File:    &buf[0],
		MaxFile: uint32(len(buf)),
		Title:   u16("Choose your Swordigo file (several APKs: select them all)"),
		Flags:   ofnAllowMulti | ofnExplorer | ofnFileMustExist | ofnPathMustExist | ofnHideReadOnly | ofnNoChangeDir | ofnDontAddRecent,
	}
	if dl, err := windows.KnownFolderPath(windows.FOLDERID_Downloads, 0); err == nil {
		ofn.InitialDir = u16(dl)
	}
	ofn.StructSize = uint32(unsafe.Sizeof(ofn))
	if call(pGetOpenFileNameW, uintptr(unsafe.Pointer(&ofn))) == 0 {
		return nil, nil // cancelled (or an error – then simply try again)
	}
	var parts []string
	start := 0
	for i, c := range buf {
		if c == 0 {
			if i == start {
				break
			}
			parts = append(parts, string(utf16.Decode(buf[start:i])))
			start = i + 1
		}
	}
	if len(parts) <= 1 {
		return parts, nil
	}
	dir := parts[0]
	out := make([]string, 0, len(parts)-1)
	for _, name := range parts[1:] {
		out = append(out, filepath.Join(dir, name))
	}
	return out, nil
}
