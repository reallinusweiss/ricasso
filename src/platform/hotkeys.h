#ifndef HOTKEYS_H
#define HOTKEYS_H

#include "gui.h"

#include <cstdint>
#include <vector>

// One table for every keyboard shortcut. The key handler, the menu bar texts
// and the Hotkeys window all read it, so they cannot disagree.
struct Hotkey {
    GuiAction action;
    uint32_t key;           // SDL_Keycode; 0 = shown only (Windows handles it, e.g. Alt+F4)
    bool ctrl, shift;
    bool free_camera_only;  // works only while the free camera is on; such rows come before the normal use of the same key
    const char* label;      // "F6", "Ctrl+Shift+D"; nullptr = alternative key that is not listed
    const char* description;
};

const std::vector<Hotkey>& hotkey_table();
// Action for a key press, or GUI_NONE. Ctrl and Shift must match exactly.
GuiAction hotkey_lookup(uint32_t key, bool ctrl, bool shift, bool free_camera_on);
// Shortcut text for menus ("F6"), or nullptr when the action has none.
const char* hotkey_label(GuiAction action);

#endif
