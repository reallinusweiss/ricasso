#ifndef WIN32_MENU_H
#define WIN32_MENU_H

#include "gui.h"

#include <functional>

struct SDL_Window;

// Native Win32 menu bar on the game window, like a classic emulator.
// Windows only; on other systems every function is a no-op.
namespace win32_menu {

void install(SDL_Window* window);  // build the menu once (after the SRE runtime is loaded) and attach it
void set_visible(bool visible);    // F1: show or hide the bar (fullscreen keeps its own state)
bool visible();
void before_fullscreen();          // detach the bar and clear its fullscreen state; call before any fullscreen switch
void after_windowed();             // re-attach it once the window is windowed again
void fit_window(SDL_Window* window);  // right after window creation: start at a windowed size, centered
GuiAction take_action();           // menu command chosen since the last call, or GUI_NONE
// One game frame, run from a timer while Windows keeps the thread in a modal
// loop (window drag or open menu), so the game does not freeze. nullptr clears it.
void set_modal_frame(std::function<void()> frame);
// Keep game frames running while another window of this app runs a modal loop
void modal_begin();
void modal_end();

}  // namespace win32_menu

#endif
