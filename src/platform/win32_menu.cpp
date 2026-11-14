// Native Win32 menu bar for the game window. Menu commands are queued as
// GuiActions and run by the main loop through process_gui_action().
#include "win32_menu.h"
#include "hotkeys.h"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

extern bool g_game_paused;
extern bool g_cam_active;
extern bool g_cam_smooth;
extern int GAME_W, GAME_H;
extern int g_render_preset;
void apply_render_preset(int preset);
void apply_custom_render_resolution(int w, int h);
bool dev_overlay_available(GuiAction a);
bool debug_overlay_visible();
bool menu_check_state(GuiAction a);

namespace win32_menu {
namespace {

constexpr UINT kIdBase = 1000;  // menu command id = kIdBase + GuiAction
constexpr UINT kIdRender = 2000;        // + apply_render_preset() index
constexpr UINT kIdRenderCustom = 2100;
constexpr UINT kIdWindowSize = 2200;    // + index into kWindowSizes

struct RenderItem { int preset; const wchar_t* label; };
// Order and preset indices of the render resolution list; see apply_render_preset() in main.cpp.
const RenderItem kRenderItems[] = {
    {10, L"4K (3840 x 2160)"}, {9, L"1440p (2560 x 1440)"}, {0, L"1080p (1920 x 1080)"},
    {11, L"900p (1600 x 900)"}, {12, L"768p (1366 x 768)"}, {1, L"720p (1280 x 720)"},
    {13, L"600p (1024 x 600)"}, {2, L"540p (960 x 544, native)"}, {14, L"480p (854 x 480)"},
    {15, L"360p (640 x 360)"}, {16, L"240p (426 x 240)"},
    {17, L"WUXGA (1920 x 1200)"}, {18, L"1440 x 900"}, {19, L"1280 x 800"},
    {20, L"UWQHD (3440 x 1440)"}, {21, L"UWHD (2560 x 1080)"}, {22, L"UWQHD+ (3840 x 1600)"},
    {23, L"Retina (2880 x 1800)"}, {24, L"WSXGA+ (1680 x 1050)"}, {25, L"SXGA (1280 x 1024)"},
    {26, L"XGA (1024 x 768)"},
};
struct WindowSize { int w, h; const wchar_t* label; };
const WindowSize kWindowSizes[] = {
    {3840, 2160, L"3840 x 2160 (4K)"}, {2560, 1440, L"2560 x 1440 (2K)"}, {1920, 1080, L"1920 x 1080"},
    {1600, 900, L"1600 x 900"}, {1366, 768, L"1366 x 768"}, {1280, 720, L"1280 x 720"},
    {3440, 1440, L"3440 x 1440 (21:9)"}, {2560, 1080, L"2560 x 1080 (21:9)"},
    {1920, 1200, L"1920 x 1200 (16:10)"}, {1680, 1050, L"1680 x 1050 (16:10)"},
    {1440, 900, L"1440 x 900 (16:10)"}, {1280, 800, L"1280 x 800 (16:10)"},
    {1280, 1024, L"1280 x 1024 (5:4)"}, {1024, 768, L"1024 x 768 (4:3)"},
    {960, 544, L"960 x 544 (native)"}, {854, 480, L"854 x 480"},
};

SDL_Window* s_window = nullptr;
HWND s_hwnd = nullptr;
HMENU s_menu = nullptr;
WNDPROC s_sdl_proc = nullptr;
bool s_visible = true;
bool s_fs_visible = false;  // F1 state while fullscreen (the bar starts hidden there)
GuiAction s_pending = GUI_NONE;
UINT s_pending_display = 0;
WPARAM s_last_syskey = 0;
std::function<void()> s_modal_frame;   // runs one game frame (set by the main loop)
constexpr UINT_PTR kModalTimer = 0x5246;  // timer id while a modal loop runs

// Menu item; the shortcut shown after the tab comes from the shared hotkey table.
void add(HMENU m, GuiAction a, const wchar_t* text) {
    std::wstring item = text;
    if (const char* key = hotkey_label(a)) {
        item += L'\t';
        for (const char* c = key; *c; ++c) item += static_cast<wchar_t>(*c);
    }
    AppendMenuW(m, MF_STRING, kIdBase + a, item.c_str());
}
void separator(HMENU m) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }
void popup(HMENU bar, HMENU sub, const wchar_t* text) { AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), text); }
// Radio-style item: CheckMenuItem then shows a bullet instead of a tick.
void add_radio(HMENU m, UINT id, const wchar_t* text) {
    AppendMenuW(m, MF_STRING, id, text);
    MENUITEMINFOW mi{};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIIM_FTYPE;
    mi.fType = MFT_STRING | MFT_RADIOCHECK;
    SetMenuItemInfoW(m, id, FALSE, &mi);
}

HMENU build() {
    HMENU bar = CreateMenu();

    HMENU file = CreatePopupMenu();
    add(file, GUI_EXIT, L"E&xit");
    popup(bar, file, L"&File");

    HMENU emu = CreatePopupMenu();
    add(emu, GUI_PAUSE, L"&Pause");
    add(emu, GUI_STEP_FRAME, L"&Frame Advance");
    separator(emu);
    add(emu, GUI_GAME_SPEED_UP, L"Speed &Up");
    add(emu, GUI_GAME_SPEED_DOWN, L"Speed &Down");
    add(emu, GUI_GAME_SPEED_RESET, L"&Reset Speed");
    popup(bar, emu, L"&Emulation");

    HMENU view = CreatePopupMenu();
    add(view, GUI_TOGGLE_FULLSCREEN, L"&Fullscreen");
    HMENU winsize = CreatePopupMenu();
    for (UINT i = 0; i < sizeof(kWindowSizes) / sizeof(kWindowSizes[0]); i++)
        add_radio(winsize, kIdWindowSize + i, kWindowSizes[i].label);
    popup(view, winsize, L"&Window Size");
    HMENU render = CreatePopupMenu();
    add_radio(render, kIdRender + 3, L"&Match Window");
    separator(render);
    for (const RenderItem& r : kRenderItems) add_radio(render, kIdRender + r.preset, r.label);
    separator(render);
    add_radio(render, kIdRenderCustom, L"&Custom...");
    popup(view, render, L"&Render Resolution");
    separator(view);
    add(view, GUI_TOGGLE_MENU_BAR, L"&Menu Bar");
    add(view, GUI_TOGGLE_TOUCH_CONTROLS, L"&On-screen Controls");
    add(view, GUI_TOGGLE_DEBUG_OVERLAY, L"&Debug Overlay");
    // The SRE overlays exist only when the runtime exports their request flags.
    if (dev_overlay_available(GUI_TOGGLE_COLLISION_SHAPES))
        add(view, GUI_TOGGLE_COLLISION_SHAPES, L"&Collision && Hitboxes");
    if (dev_overlay_available(GUI_TOGGLE_COMBAT_WIREFRAME))
        add(view, GUI_TOGGLE_COMBAT_WIREFRAME, L"&Hide HUD && Overlays");
    popup(bar, view, L"&View");

    HMENU config = CreatePopupMenu();
    add(config, GUI_CUSTOMIZE_CONTROLS, L"&Controls...");
    add(config, GUI_TOGGLE_GRAPHICS_OPTIONS, L"&Graphics...");
    separator(config);
    add(config, GUI_TOGGLE_TYPING_MODE, L"Keyboard &Typing Mode");
    separator(config);
    add(config, GUI_MUSIC_MUTE, L"&Mute Music");
    add(config, GUI_MUSIC_VOL_UP, L"Music Volume &Up");
    add(config, GUI_MUSIC_VOL_DOWN, L"Music Volume &Down");
    popup(bar, config, L"&Config");

    HMENU tools = CreatePopupMenu();
    add(tools, GUI_TOGGLE_CHEATS, L"&Cheats...");
    add(tools, GUI_TOGGLE_SCENE_SELECT, L"&Scene Select...");
    separator(tools);
    add(tools, GUI_TOGGLE_CAM, L"&Free Camera");
    add(tools, GUI_TOGGLE_SMOOTH_CAM, L"S&mooth Camera");
    add(tools, GUI_TOGGLE_CAM_POV, L"First-&Person View");
    add(tools, GUI_CAM_FOV_DOWN, L"&Narrower Field of View");
    add(tools, GUI_CAM_FOV_UP, L"&Wider Field of View");
    separator(tools);
    add(tools, GUI_CYCLE_TIME_OF_DAY, L"Cycle &Time of Day");
    separator(tools);
    add(tools, GUI_TOGGLE_LUA_CONSOLE, L"&Lua Console...");
    add(tools, GUI_TOGGLE_LUA_SCRIPTS, L"Lua Scr&ipts...");
    add(tools, GUI_TOGGLE_ERROR_LOG, L"Lua &Error Log...");
    popup(bar, tools, L"&Tools");

    HMENU help = CreatePopupMenu();
    add(help, GUI_KEYBINDS, L"&Hotkeys...");
    add(help, GUI_ABOUT, L"&About Ricasso...");
    popup(bar, help, L"&Help");
    return bar;
}

bool is_fullscreen() {
    return s_window && (SDL_GetWindowFlags(s_window) & SDL_WINDOW_FULLSCREEN) != 0;
}

// Attaching or detaching the menu would change the client area and leave the
// game letterboxed. In a normal window the outer frame grows or shrinks by the
// menu height instead, so the game area keeps its size (like BizHawk).
void attach(bool on) {
    if (!s_hwnd || !s_menu) return;
    if ((GetMenu(s_hwnd) != nullptr) == on) return;
    RECT client{};
    const bool keep_client = !is_fullscreen() && !IsZoomed(s_hwnd) && !IsIconic(s_hwnd) &&
                             GetClientRect(s_hwnd, &client);
    SetMenu(s_hwnd, on ? s_menu : nullptr);
    DrawMenuBar(s_hwnd);
    if (!keep_client) return;
    RECT frame = client;
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(s_hwnd, GWL_STYLE));
    const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(s_hwnd, GWL_EXSTYLE));
    AdjustWindowRectEx(&frame, style, on ? TRUE : FALSE, ex_style);
    SetWindowPos(s_hwnd, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void check_id(UINT id, bool on) {
    CheckMenuItem(s_menu, id, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED));
}

void check(GuiAction a, bool on) {
    check_id(kIdBase + a, on);
}

void refresh_checks() {
    check(GUI_PAUSE, g_game_paused);
    check(GUI_TOGGLE_FULLSCREEN, is_fullscreen());
    check(GUI_TOGGLE_MENU_BAR, visible());
    check(GUI_TOGGLE_TOUCH_CONTROLS, menu_check_state(GUI_TOGGLE_TOUCH_CONTROLS));
    check(GUI_TOGGLE_DEBUG_OVERLAY, debug_overlay_visible());
    check(GUI_TOGGLE_TYPING_MODE, menu_check_state(GUI_TOGGLE_TYPING_MODE));
    check(GUI_TOGGLE_CAM, g_cam_active);
    check(GUI_TOGGLE_SMOOTH_CAM, g_cam_smooth);
    check(GUI_TOGGLE_CAM_POV, menu_check_state(GUI_TOGGLE_CAM_POV));
    // These three only work while the free camera is on.
    for (GuiAction a : {GUI_TOGGLE_CAM_POV, GUI_CAM_FOV_DOWN, GUI_CAM_FOV_UP})
        EnableMenuItem(s_menu, kIdBase + a, MF_BYCOMMAND | (g_cam_active ? MF_ENABLED : MF_GRAYED));
    for (const RenderItem& r : kRenderItems) check_id(kIdRender + r.preset, g_render_preset == r.preset);
    check_id(kIdRender + 3, g_render_preset == 3);
    check_id(kIdRenderCustom, g_render_preset == -1);
    int w = 0, h = 0;
    if (s_window) SDL_GetWindowSize(s_window, &w, &h);
    for (UINT i = 0; i < sizeof(kWindowSizes) / sizeof(kWindowSizes[0]); i++)
        check_id(kIdWindowSize + i, kWindowSizes[i].w == w && kWindowSizes[i].h == h);
}

// In-memory dialog template for "Custom..." (no .rc file needed).
struct DialogTemplate {
    std::vector<WORD> data;
    void word(WORD v) { data.push_back(v); }
    void dword(DWORD v) { word(LOWORD(v)); word(HIWORD(v)); }
    void text(const wchar_t* s) { do { word(*s); } while (*s++); }
    void align() { if (data.size() & 1) word(0); }  // DWORD alignment
    void item(DWORD style, short x, short y, short cx, short cy, WORD id, WORD cls, const wchar_t* title) {
        align();
        dword(style | WS_CHILD | WS_VISIBLE); dword(0);
        word(x); word(y); word(cx); word(cy); word(id);
        word(0xFFFF); word(cls); text(title); word(0);
    }
};

constexpr int kIdEditW = 101, kIdEditH = 102;

INT_PTR CALLBACK custom_res_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    int* size = reinterpret_cast<int*>(GetWindowLongPtrW(dlg, DWLP_USER));
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLongPtrW(dlg, DWLP_USER, lp);
        size = reinterpret_cast<int*>(lp);
        SetDlgItemInt(dlg, kIdEditW, size[0], FALSE);
        SetDlgItemInt(dlg, kIdEditH, size[1], FALSE);
        PostMessageW(dlg, WM_APP, 0, 0);
        return TRUE;
    case WM_APP:
        // A client started with "hide window" (the Ricasso starter) has its first
        // ShowWindow call ignored by Windows; without this the dialog could stay invisible.
        if (!IsWindowVisible(dlg)) ShowWindow(dlg, SW_SHOWNORMAL);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            BOOL ok_w = FALSE, ok_h = FALSE;
            const UINT w = GetDlgItemInt(dlg, kIdEditW, &ok_w, FALSE);
            const UINT h = GetDlgItemInt(dlg, kIdEditH, &ok_h, FALSE);
            if (!ok_w || !ok_h) { MessageBeep(MB_ICONWARNING); return TRUE; }
            size[0] = static_cast<int>(w);
            size[1] = static_cast<int>(h);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(dlg, IDCANCEL); return TRUE; }
        break;
    }
    return FALSE;
}

// Asks for a render size; false when cancelled. size holds the start values.
bool ask_custom_render_size(int size[2]) {
    DialogTemplate t;
    t.dword(DS_MODALFRAME | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    t.dword(0);
    t.word(6);                       // item count
    t.word(0); t.word(0); t.word(156); t.word(70);
    t.word(0); t.word(0);            // no menu, default class
    t.text(L"Custom Render Resolution");
    t.word(9); t.text(L"Segoe UI");
    t.item(SS_LEFT, 8, 10, 40, 10, 0xFFFF, 0x0082, L"Width:");
    t.item(WS_BORDER | WS_TABSTOP | ES_NUMBER, 52, 8, 96, 12, kIdEditW, 0x0081, L"");
    t.item(SS_LEFT, 8, 28, 40, 10, 0xFFFF, 0x0082, L"Height:");
    t.item(WS_BORDER | WS_TABSTOP | ES_NUMBER, 52, 26, 96, 12, kIdEditH, 0x0081, L"");
    t.item(BS_DEFPUSHBUTTON | WS_TABSTOP, 42, 48, 50, 14, IDOK, 0x0080, L"OK");
    t.item(BS_PUSHBUTTON | WS_TABSTOP, 98, 48, 50, 14, IDCANCEL, 0x0080, L"Cancel");
    return DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
                                   reinterpret_cast<LPCDLGTEMPLATEW>(t.data.data()), s_hwnd,
                                   custom_res_proc, reinterpret_cast<LPARAM>(size)) == IDOK;
}

// Sets the client size to w x h and centers the whole frame (title bar and menu
// included) on the window's display, so it never starts off-screen.
void place_window(int w, int h) {
    SDL_DisplayID d = SDL_GetDisplayForWindow(s_window);
    SDL_Rect ub;
    SDL_GetDisplayUsableBounds(d, &ub);
    SDL_SetWindowSize(s_window, w, h);
    SDL_SyncWindow(s_window);
    int top = 0, left = 0, bottom = 0, right = 0;
    SDL_GetWindowBordersSize(s_window, &top, &left, &bottom, &right);  // top includes title bar and menu
    int tw = w + left + right, th = h + top + bottom;
    int x = ub.x + left + (tw < ub.w ? (ub.w - tw) / 2 : 0);
    int y = ub.y + top + (th < ub.h ? (ub.h - th) / 2 : 0);
    SDL_SetWindowPosition(s_window, x, y);
    SDL_SyncWindow(s_window);
}

void set_window_size(int w, int h) {
    if (!s_window || is_fullscreen()) return;
    if (SDL_GetWindowFlags(s_window) & SDL_WINDOW_MAXIMIZED) SDL_RestoreWindow(s_window);
    place_window(w, h);
}

// Startup: the largest 16:9 size from kWindowSizes whose frame fits 85% of the display.
void fit_initial_window() {
    if (!s_window || is_fullscreen()) return;
    SDL_DisplayID d = SDL_GetDisplayForWindow(s_window);
    SDL_Rect ub;
    SDL_GetDisplayUsableBounds(d, &ub);
    int top = 0, left = 0, bottom = 0, right = 0;
    SDL_GetWindowBordersSize(s_window, &top, &left, &bottom, &right);
    int pick_w = 854, pick_h = 480;
    for (const WindowSize& s : kWindowSizes) {
        if (std::abs(s.w * 9 - s.h * 16) > 16) continue;
        if (s.w + left + right <= ub.w * 85 / 100 && s.h + top + bottom <= ub.h * 85 / 100) {
            pick_w = s.w;
            pick_h = s.h;
            break;
        }
    }
    place_window(pick_w, pick_h);
}

// Runs a View menu command; called from take_action(), i.e. between frames.
void run_display_command(UINT id) {
    const UINT window_count = sizeof(kWindowSizes) / sizeof(kWindowSizes[0]);
    if (id == kIdRenderCustom) {
        int size[2] = {GAME_W, GAME_H};
        if (ask_custom_render_size(size)) apply_custom_render_resolution(size[0], size[1]);
    } else if (id >= kIdWindowSize && id < kIdWindowSize + window_count) {
        set_window_size(kWindowSizes[id - kIdWindowSize].w, kWindowSizes[id - kIdWindowSize].h);
    } else if (id >= kIdRender && id < kIdRenderCustom) {
        apply_render_preset(static_cast<int>(id - kIdRender));
    }
}

LRESULT CALLBACK menu_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND:
        if (HIWORD(wp) == 0 && LOWORD(wp) >= kIdRender) {
            s_pending_display = LOWORD(wp);
            return 0;
        }
        if (HIWORD(wp) == 0 && LOWORD(wp) >= kIdBase) {
            s_pending = static_cast<GuiAction>(LOWORD(wp) - kIdBase);
            return 0;
        }
        break;
    case WM_INITMENUPOPUP:
        refresh_checks();
        break;
    case WM_ENTERSIZEMOVE:
    case WM_ENTERMENULOOP:
        // Windows now runs its own message loop until the drag ends or the menu
        // closes; a timer keeps game frames coming from inside it.
        if (s_modal_frame) SetTimer(hwnd, kModalTimer, USER_TIMER_MINIMUM, nullptr);
        break;
    case WM_EXITSIZEMOVE:
    case WM_EXITMENULOOP:
        KillTimer(hwnd, kModalTimer);
        break;
    case WM_TIMER:
        if (wp == kModalTimer) {
            // A frame pumps messages itself and may dispatch this timer again.
            static bool in_frame = false;
            if (s_modal_frame && !in_frame) {
                in_frame = true;
                s_modal_frame();
                in_frame = false;
            }
            return 0;
        }
        break;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
        // SDL turns these into key events and never hands them to
        // DefWindowProc, which is what opens a menu with Alt.
        if (msg == WM_SYSKEYDOWN) s_last_syskey = wp;
        const LRESULT r = CallWindowProcW(s_sdl_proc, hwnd, msg, wp, lp);
        if (GetMenu(hwnd)) DefWindowProcW(hwnd, msg, wp, lp);
        return r;
    }
    case WM_SYSCHAR:
        // Alt+letter opens the matching menu.
        if (GetMenu(hwnd)) return DefWindowProcW(hwnd, msg, wp, lp);
        break;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU) {
            // F10 is a game hotkey; only Alt opens the menu.
            if (!GetMenu(hwnd) || s_last_syskey == VK_F10) return 0;
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        break;
    }
    return CallWindowProcW(s_sdl_proc, hwnd, msg, wp, lp);
}

}  // namespace

void install(SDL_Window* window) {
    if (s_menu || !window) return;
    HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                         SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) return;
    s_window = window;
    s_hwnd = hwnd;
    s_menu = build();
    s_sdl_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(menu_proc)));
    attach(s_visible && !is_fullscreen());
    fit_initial_window();
    std::cout << "[Menu] Native menu bar installed" << std::endl;
}

void set_visible(bool visible) {
    if (is_fullscreen()) {
        s_fs_visible = visible;
        attach(visible);
        return;
    }
    s_visible = visible;
    attach(visible);
}

bool visible() { return is_fullscreen() ? s_fs_visible : s_visible; }

void before_fullscreen() {
    s_fs_visible = false;
    attach(false);
}

void after_windowed() { attach(s_visible); }

void fit_window(SDL_Window* window) {
    if (!window) return;
    if (!s_window) s_window = window;
    fit_initial_window();
}

void set_modal_frame(std::function<void()> frame) {
    s_modal_frame = std::move(frame);
    if (!s_modal_frame && s_hwnd) KillTimer(s_hwnd, kModalTimer);
}

void modal_begin() { if (s_modal_frame && s_hwnd) SetTimer(s_hwnd, kModalTimer, USER_TIMER_MINIMUM, nullptr); }
void modal_end()   { if (s_hwnd) KillTimer(s_hwnd, kModalTimer); }

GuiAction take_action() {
    // A fullscreen switch that bypassed toggle_fullscreen() must not keep the bar.
    if (is_fullscreen() && !s_fs_visible) attach(false);
    if (s_pending_display) {
        const UINT id = s_pending_display;
        s_pending_display = 0;
        run_display_command(id);
    }
    const GuiAction a = s_pending;
    s_pending = GUI_NONE;
    return a;
}

}  // namespace win32_menu

#else  // !_WIN32

namespace win32_menu {
void install(SDL_Window*) {}
void set_visible(bool) {}
bool visible() { return false; }
void before_fullscreen() {}
void after_windowed() {}
void fit_window(SDL_Window*) {}
void set_modal_frame(std::function<void()>) {}
void modal_begin() {}
void modal_end() {}
GuiAction take_action() { return GUI_NONE; }
}  // namespace win32_menu

#endif
