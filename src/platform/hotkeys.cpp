#include "hotkeys.h"

#include <SDL3/SDL_keycode.h>

namespace {
bool s_tas_mode = false;
}

const std::vector<Hotkey>& hotkey_table() {
    static const std::vector<Hotkey> table = {
        {GUI_EXIT, 0, false, false, HK_ALWAYS, "Alt+F4", "Exit"},
        {GUI_SAVE_STATE, SDLK_F5, false, false, HK_TAS, "F5", "Save state to the current slot (TAS)"},
        {GUI_TAS_PREV_SLOT, SDLK_F6, false, false, HK_TAS, "F6", "Previous savestate slot (TAS)"},
        {GUI_TAS_NEXT_SLOT, SDLK_F7, false, false, HK_TAS, "F7", "Next savestate slot (TAS)"},
        {GUI_LOAD_STATE, SDLK_F8, false, false, HK_TAS, "F8", "Load state from the current slot (TAS)"},
        {GUI_TAS_RECORD, SDLK_F9, false, false, HK_TAS, "F9", "Record a movie, or stop recording (TAS)"},
        {GUI_TAS_PLAY, SDLK_F10, false, false, HK_TAS, "F10", "Play the movie from its start, or stop (TAS)"},
        {GUI_PAUSE, SDLK_P, false, false, HK_TAS, "P", "Pause or resume (TAS)"},
        {GUI_PAUSE, SDLK_PAUSE, false, false, HK_TAS, nullptr, nullptr},
        {GUI_STEP_FRAME, SDLK_N, false, false, HK_TAS, "N", "Frame advance; hold to repeat (TAS)"},
        {GUI_TAS_FAST_FORWARD, 0, false, false, HK_TAS, "Tab", "Fast-forward while held (TAS)"},
        {GUI_TAS_SAVE_MOVIE, SDLK_S, true, false, HK_TAS, "Ctrl+S", "Write the movie file (TAS)"},
        {GUI_TAS_REROLL_RNG, SDLK_R, false, false, HK_TAS, "R", "New random numbers from the next frame on (TAS)"},
        {GUI_TOGGLE_MENU_BAR, SDLK_F1, false, false, HK_ALWAYS, "F1", "Show or hide the menu bar"},
        {GUI_CUSTOMIZE_CONTROLS, SDLK_F2, false, false, HK_ALWAYS, "F2", "Controls window"},
        {GUI_TOGGLE_DEBUG_OVERLAY, SDLK_F3, false, false, HK_ALWAYS, "F3", "Debug overlay"},
        {GUI_TOGGLE_CHEATS, SDLK_F4, false, false, HK_ALWAYS, "F4", "Cheats window"},
        {GUI_TOGGLE_CAM, SDLK_F5, false, false, HK_ALWAYS, "F5", "Free camera"},
        {GUI_TOGGLE_GRAPHICS_OPTIONS, SDLK_F6, false, false, HK_ALWAYS, "F6", "Graphics window"},
        {GUI_CAM_FOV_DOWN, SDLK_F7, false, false, HK_FREE_CAMERA, "F7", "Narrower field of view (free camera only)"},
        {GUI_CAM_FOV_UP, SDLK_F8, false, false, HK_FREE_CAMERA, "F8", "Wider field of view (free camera only)"},
        {GUI_PAUSE, SDLK_F8, false, false, HK_ALWAYS, "F8", "Pause or resume"},
        {GUI_STEP_FRAME, SDLK_F9, false, false, HK_ALWAYS, "F9", "Frame advance"},
        {GUI_TOGGLE_TOUCH_CONTROLS, SDLK_F10, false, false, HK_ALWAYS, "F10", "On-screen controls"},
        {GUI_TOGGLE_SCENE_SELECT, SDLK_F11, false, false, HK_ALWAYS, "F11", "Scene Select window"},
        {GUI_TOGGLE_FULLSCREEN, SDLK_F12, false, false, HK_ALWAYS, "F12", "Fullscreen"},
        {GUI_GAME_SPEED_UP, SDLK_PLUS, false, false, HK_ALWAYS, "+", "Speed up"},
        {GUI_GAME_SPEED_UP, SDLK_EQUALS, false, false, HK_ALWAYS, nullptr, nullptr},
        {GUI_GAME_SPEED_UP, SDLK_KP_PLUS, false, false, HK_ALWAYS, nullptr, nullptr},
        {GUI_GAME_SPEED_DOWN, SDLK_MINUS, false, false, HK_ALWAYS, "-", "Speed down"},
        {GUI_GAME_SPEED_DOWN, SDLK_KP_MINUS, false, false, HK_ALWAYS, nullptr, nullptr},
        {GUI_GAME_SPEED_RESET, SDLK_0, false, false, HK_ALWAYS, "0", "Reset speed"},
        {GUI_TOGGLE_CAM_POV, SDLK_M, false, false, HK_FREE_CAMERA, "M", "First-person view (free camera only)"},
        {GUI_CYCLE_TIME_OF_DAY, SDLK_N, false, false, HK_ALWAYS, "N", "Cycle time of day"},
        {GUI_RELOAD_POSTFX, SDLK_R, false, false, HK_ALWAYS, "R", "Reload the graphics config file"},
        {GUI_TOGGLE_LUA_CONSOLE, SDLK_GRAVE, false, false, HK_ALWAYS, "`", "Lua Console window"},
        {GUI_TOGGLE_TYPING_MODE, SDLK_BACKSLASH, false, false, HK_ALWAYS, "\\", "Keyboard typing mode"},
        {GUI_TOGGLE_COLLISION_SHAPES, SDLK_D, true, true, HK_ALWAYS, "Ctrl+Shift+D", "Collision shapes and hitboxes"},
        {GUI_TOGGLE_COMBAT_WIREFRAME, SDLK_W, true, true, HK_ALWAYS, "Ctrl+Shift+W", "Hide HUD and overlays"},
    };
    return table;
}

void hotkeys_set_tas_mode(bool on) { s_tas_mode = on; }

bool hotkeys_tas_mode() { return s_tas_mode; }

bool hotkey_active(const Hotkey& h) {
    if (h.when == HK_TAS) return s_tas_mode;
    if (!s_tas_mode) return true;
    for (const Hotkey& t : hotkey_table()) {
        if (t.when == HK_TAS && t.key != 0 && t.key == h.key && t.ctrl == h.ctrl && t.shift == h.shift)
            return false;
    }
    return true;
}

GuiAction hotkey_lookup(uint32_t key, bool ctrl, bool shift, bool free_camera_on) {
    for (const Hotkey& h : hotkey_table()) {
        if (key != 0 && key == h.key && ctrl == h.ctrl && shift == h.shift &&
            (h.when != HK_FREE_CAMERA || free_camera_on) && hotkey_active(h))
            return h.action;
    }
    return GUI_NONE;
}

const char* hotkey_label(GuiAction action) {
    for (const Hotkey& h : hotkey_table()) {
        if (h.action == action && h.label && hotkey_active(h)) return h.label;
    }
    return nullptr;
}
