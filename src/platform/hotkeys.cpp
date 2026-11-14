#include "hotkeys.h"

#include <SDL3/SDL_keycode.h>

const std::vector<Hotkey>& hotkey_table() {
    static const std::vector<Hotkey> table = {
        {GUI_EXIT, 0, false, false, false, "Alt+F4", "Exit"},
        {GUI_TOGGLE_MENU_BAR, SDLK_F1, false, false, false, "F1", "Show or hide the menu bar"},
        {GUI_CUSTOMIZE_CONTROLS, SDLK_F2, false, false, false, "F2", "Controls window"},
        {GUI_TOGGLE_DEBUG_OVERLAY, SDLK_F3, false, false, false, "F3", "Debug overlay"},
        {GUI_TOGGLE_CHEATS, SDLK_F4, false, false, false, "F4", "Cheats window"},
        {GUI_TOGGLE_CAM, SDLK_F5, false, false, false, "F5", "Free camera"},
        {GUI_TOGGLE_GRAPHICS_OPTIONS, SDLK_F6, false, false, false, "F6", "Graphics window"},
        {GUI_CAM_FOV_DOWN, SDLK_F7, false, false, true, "F7", "Narrower field of view (free camera only)"},
        {GUI_CAM_FOV_UP, SDLK_F8, false, false, true, "F8", "Wider field of view (free camera only)"},
        {GUI_PAUSE, SDLK_F8, false, false, false, "F8", "Pause or resume"},
        {GUI_STEP_FRAME, SDLK_F9, false, false, false, "F9", "Frame advance"},
        {GUI_TOGGLE_TOUCH_CONTROLS, SDLK_F10, false, false, false, "F10", "On-screen controls"},
        {GUI_TOGGLE_SCENE_SELECT, SDLK_F11, false, false, false, "F11", "Scene Select window"},
        {GUI_TOGGLE_FULLSCREEN, SDLK_F12, false, false, false, "F12", "Fullscreen"},
        {GUI_GAME_SPEED_UP, SDLK_PLUS, false, false, false, "+", "Speed up"},
        {GUI_GAME_SPEED_UP, SDLK_EQUALS, false, false, false, nullptr, nullptr},
        {GUI_GAME_SPEED_UP, SDLK_KP_PLUS, false, false, false, nullptr, nullptr},
        {GUI_GAME_SPEED_DOWN, SDLK_MINUS, false, false, false, "-", "Speed down"},
        {GUI_GAME_SPEED_DOWN, SDLK_KP_MINUS, false, false, false, nullptr, nullptr},
        {GUI_GAME_SPEED_RESET, SDLK_0, false, false, false, "0", "Reset speed"},
        {GUI_TOGGLE_CAM_POV, SDLK_M, false, false, true, "M", "First-person view (free camera only)"},
        {GUI_CYCLE_TIME_OF_DAY, SDLK_N, false, false, false, "N", "Cycle time of day"},
        {GUI_RELOAD_POSTFX, SDLK_R, false, false, false, "R", "Reload the graphics config file"},
        {GUI_TOGGLE_LUA_CONSOLE, SDLK_GRAVE, false, false, false, "`", "Lua Console window"},
        {GUI_TOGGLE_TYPING_MODE, SDLK_BACKSLASH, false, false, false, "\\", "Keyboard typing mode"},
        {GUI_TOGGLE_COLLISION_SHAPES, SDLK_D, true, true, false, "Ctrl+Shift+D", "Collision shapes and hitboxes"},
        {GUI_TOGGLE_COMBAT_WIREFRAME, SDLK_W, true, true, false, "Ctrl+Shift+W", "Hide HUD and overlays"},
    };
    return table;
}

GuiAction hotkey_lookup(uint32_t key, bool ctrl, bool shift, bool free_camera_on) {
    for (const Hotkey& h : hotkey_table()) {
        if (key != 0 && key == h.key && ctrl == h.ctrl && shift == h.shift &&
            (!h.free_camera_only || free_camera_on))
            return h.action;
    }
    return GUI_NONE;
}

const char* hotkey_label(GuiAction action) {
    for (const Hotkey& h : hotkey_table()) {
        if (h.action == action && h.label) return h.label;
    }
    return nullptr;
}
