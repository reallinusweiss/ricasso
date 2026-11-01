//go:build windows

// SPDX-License-Identifier: GPL-3.0-or-later

package main

// Lean Win32 bindings: only what the starter needs.

import (
	"syscall"
	"unsafe"

	"golang.org/x/sys/windows"
)

var (
	user32   = windows.NewLazySystemDLL("user32.dll")
	gdi32    = windows.NewLazySystemDLL("gdi32.dll")
	shell32  = windows.NewLazySystemDLL("shell32.dll")
	comdlg32 = windows.NewLazySystemDLL("comdlg32.dll")
	ole32    = windows.NewLazySystemDLL("ole32.dll")
	kernel32 = windows.NewLazySystemDLL("kernel32.dll")
	// comctl32 v6 comes from WinSxS via the manifest; after SetDefaultDllDirectories
	// only System32 is searched otherwise.
	comctl32 = windows.NewLazyDLL("comctl32.dll")

	pRegisterClassExW         = user32.NewProc("RegisterClassExW")
	pCreateWindowExW          = user32.NewProc("CreateWindowExW")
	pDefWindowProcW           = user32.NewProc("DefWindowProcW")
	pGetMessageW              = user32.NewProc("GetMessageW")
	pTranslateMessage         = user32.NewProc("TranslateMessage")
	pDispatchMessageW         = user32.NewProc("DispatchMessageW")
	pIsDialogMessageW         = user32.NewProc("IsDialogMessageW")
	pPostMessageW             = user32.NewProc("PostMessageW")
	pSendMessageW             = user32.NewProc("SendMessageW")
	pShowWindow               = user32.NewProc("ShowWindow")
	pUpdateWindow             = user32.NewProc("UpdateWindow")
	pDestroyWindow            = user32.NewProc("DestroyWindow")
	pPostQuitMessage          = user32.NewProc("PostQuitMessage")
	pMessageBoxW              = user32.NewProc("MessageBoxW")
	pSetWindowTextW           = user32.NewProc("SetWindowTextW")
	pSetWindowPos             = user32.NewProc("SetWindowPos")
	pEnableWindow             = user32.NewProc("EnableWindow")
	pGetDpiForWindow          = user32.NewProc("GetDpiForWindow")
	pGetDpiForSystem          = user32.NewProc("GetDpiForSystem")
	pGetClientRect            = user32.NewProc("GetClientRect")
	pInvalidateRect           = user32.NewProc("InvalidateRect")
	pBeginPaint               = user32.NewProc("BeginPaint")
	pEndPaint                 = user32.NewProc("EndPaint")
	pLoadCursorW              = user32.NewProc("LoadCursorW")
	pLoadImageW               = user32.NewProc("LoadImageW")
	pSetForegroundWindow      = user32.NewProc("SetForegroundWindow")
	pDrawTextW                = user32.NewProc("DrawTextW")
	pFillRect                 = user32.NewProc("FillRect")
	pMonitorFromWindow        = user32.NewProc("MonitorFromWindow")
	pGetMonitorInfoW          = user32.NewProc("GetMonitorInfoW")
	pAdjustWindowRectExForDpi = user32.NewProc("AdjustWindowRectExForDpi")
	pAdjustWindowRectEx       = user32.NewProc("AdjustWindowRectEx")
	pSetFocus                 = user32.NewProc("SetFocus")
	pFindWindowW              = user32.NewProc("FindWindowW")
	pIsWindowVisible          = user32.NewProc("IsWindowVisible")
	pSetCursor                = user32.NewProc("SetCursor")
	pGetCursorPos             = user32.NewProc("GetCursorPos")
	pScreenToClient           = user32.NewProc("ScreenToClient")
	pGetSystemMetrics         = user32.NewProc("GetSystemMetrics")

	pCreateFontW      = gdi32.NewProc("CreateFontW")
	pDeleteObject     = gdi32.NewProc("DeleteObject")
	pSelectObject     = gdi32.NewProc("SelectObject")
	pSetBkMode        = gdi32.NewProc("SetBkMode")
	pSetBkColor       = gdi32.NewProc("SetBkColor")
	pSetTextColor     = gdi32.NewProc("SetTextColor")
	pCreateSolidBrush = gdi32.NewProc("CreateSolidBrush")
	pCreatePen        = gdi32.NewProc("CreatePen")
	pRoundRect        = gdi32.NewProc("RoundRect")
	pGetStockObject   = gdi32.NewProc("GetStockObject")

	pDragAcceptFiles = shell32.NewProc("DragAcceptFiles")
	pDragQueryFileW  = shell32.NewProc("DragQueryFileW")
	pDragFinish      = shell32.NewProc("DragFinish")

	pGetOpenFileNameW = comdlg32.NewProc("GetOpenFileNameW")

	pInitCommonControlsEx = comctl32.NewProc("InitCommonControlsEx")

	pCoCreateInstance = ole32.NewProc("CoCreateInstance")

	pGetModuleHandleW = kernel32.NewProc("GetModuleHandleW")
)

const (
	wsOverlapped    = 0x00000000
	wsCaption       = 0x00C00000
	wsSysMenu       = 0x00080000
	wsMinimizeBox   = 0x00020000
	wsClipChildren  = 0x02000000
	wsChild         = 0x40000000
	wsVisible       = 0x10000000
	wsTabStop       = 0x00010000
	wsVScroll       = 0x00200000
	wsBorder        = 0x00800000
	wsExControlPar  = 0x00010000
	wsExAcceptFiles = 0x00000010
	wsExClientEdge  = 0x00000200

	ssLeft        = 0x0
	ssNoPrefix    = 0x80
	ssEndEllipsis = 0x4000

	bsPushButton    = 0x0
	bsDefPushButton = 0x1
	bsCommandLink   = 0xE
	bsDefCmdLink    = 0xF

	esMultiline   = 0x0004
	esReadOnly    = 0x0800
	esAutoVScroll = 0x0040

	pbsMarquee = 0x08

	cwUseDefault = 0x80000000

	wmCreate          = 0x0001
	wmDestroy         = 0x0002
	wmPaint           = 0x000F
	wmClose           = 0x0010
	wmSetFont         = 0x0030
	wmCommand         = 0x0111
	wmSetCursor       = 0x0020
	wmCtlColorStatic  = 0x0138
	wmCtlColorBtn     = 0x0135
	wmLButtonUp       = 0x0202
	wmDropFiles       = 0x0233
	wmDpiChanged      = 0x02E0
	wmApp             = 0x8000
	bcmSetNote        = 0x1609
	pbmSetRange32     = 0x0406
	pbmSetPos         = 0x0402
	pbmSetMarquee     = 0x040A
	emSetSel          = 0x00B1
	emScrollCaret     = 0x00B7
	htClient          = 1
	swHide            = 0
	swShow            = 5
	swRestore         = 9
	swpNoZOrder       = 0x0004
	swpNoActivate     = 0x0010
	idOK              = 1
	idCancel          = 2
	idYes             = 6
	mbOK              = 0x0
	mbYesNo           = 0x4
	mbIconError       = 0x10
	mbIconQuestion    = 0x20
	mbIconWarning     = 0x30
	mbIconInfo        = 0x40
	mbSetForeground   = 0x10000
	dtCenter          = 0x1
	dtVCenter         = 0x4
	dtSingleLine      = 0x20
	dtWordBreak       = 0x10
	dtNoPrefix        = 0x800
	transparent       = 1
	psDash            = 1
	nullBrush         = 5
	whiteBrush        = 0
	imageIcon         = 1
	lrDefaultColor    = 0
	lrShared          = 0x8000
	idcArrow          = 32512
	idcHand           = 32649
	monitorNearest    = 2
	smCxIcon          = 11
	smCxSmIcon        = 49
	wmSetIcon         = 0x0080
	iccStandardClass  = 0x00004000
	iccProgressClass  = 0x00000020
	iccLinkClass      = 0x00008000
	fwNormal          = 400
	fwSemiBold        = 600
	defaultCharset    = 1
	clearTypeQuality  = 5
	ofnAllowMulti     = 0x00000200
	ofnExplorer       = 0x00080000
	ofnFileMustExist  = 0x00001000
	ofnPathMustExist  = 0x00000800
	ofnHideReadOnly   = 0x00000004
	ofnNoChangeDir    = 0x00000008
	ofnDontAddRecent  = 0x02000000
	createNoWindow    = 0x08000000
	errorAlreadyExist = 183
)

type point struct{ X, Y int32 }

type rect struct{ Left, Top, Right, Bottom int32 }

type msg struct {
	Hwnd    uintptr
	Message uint32
	WParam  uintptr
	LParam  uintptr
	Time    uint32
	Pt      point
	Private uint32
}

type wndClassEx struct {
	Size       uint32
	Style      uint32
	WndProc    uintptr
	ClsExtra   int32
	WndExtra   int32
	Instance   uintptr
	Icon       uintptr
	Cursor     uintptr
	Background uintptr
	MenuName   *uint16
	ClassName  *uint16
	IconSm     uintptr
}

type paintStruct struct {
	Hdc       uintptr
	Erase     int32
	RcPaint   rect
	Restore   int32
	IncUpdate int32
	Reserved  [32]byte
}

type monitorInfo struct {
	Size    uint32
	Monitor rect
	Work    rect
	Flags   uint32
}

type initCommonControlsEx struct {
	Size uint32
	ICC  uint32
}

type openFileName struct {
	StructSize    uint32
	Owner         uintptr
	Instance      uintptr
	Filter        *uint16
	CustomFilter  *uint16
	MaxCustFilter uint32
	FilterIndex   uint32
	File          *uint16
	MaxFile       uint32
	FileTitle     *uint16
	MaxFileTitle  uint32
	InitialDir    *uint16
	Title         *uint16
	Flags         uint32
	FileOffset    uint16
	FileExtension uint16
	DefExt        *uint16
	CustData      uintptr
	Hook          uintptr
	TemplateName  *uint16
	Reserved      uintptr
	ReservedInt   uint32
	FlagsEx       uint32
}

func u16(s string) *uint16 {
	p, err := windows.UTF16PtrFromString(s)
	if err != nil {
		p, _ = windows.UTF16PtrFromString("?")
	}
	return p
}

//go:uintptrescapes
func call(p *windows.LazyProc, args ...uintptr) uintptr {
	r, _, _ := p.Call(args...)
	return r
}

func rgb(r, g, b byte) uintptr { return uintptr(r) | uintptr(g)<<8 | uintptr(b)<<16 }

func loword(v uintptr) uint32 { return uint32(v) & 0xFFFF }
func hiword(v uintptr) uint32 { return (uint32(v) >> 16) & 0xFFFF }

//go:uintptrescapes
func sendMessage(hwnd uintptr, m uint32, w, l uintptr) uintptr {
	return call(pSendMessageW, hwnd, uintptr(m), w, l)
}

//go:uintptrescapes
func postMessage(hwnd uintptr, m uint32, w, l uintptr) {
	call(pPostMessageW, hwnd, uintptr(m), w, l)
}

func setText(hwnd uintptr, s string) {
	call(pSetWindowTextW, hwnd, uintptr(unsafe.Pointer(u16(s))))
}

func messageBox(owner uintptr, title, text string, flags uint32) int {
	return int(call(pMessageBoxW, owner, uintptr(unsafe.Pointer(u16(text))), uintptr(unsafe.Pointer(u16(title))), uintptr(flags|mbSetForeground)))
}

func mulDiv(v, num, den int) int {
	return (v*num + den/2) / den
}

var newCallback = syscall.NewCallback
