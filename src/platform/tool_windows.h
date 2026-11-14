#ifndef TOOL_WINDOWS_H
#define TOOL_WINDOWS_H

struct SDL_Window;

// Tool windows opened from the menu bar: real top-level windows in the native
// Windows style (Qt Widgets), owned by the game window. Main thread only.
namespace tool_windows {

enum Id { CONTROLS, GRAPHICS, CHEATS, SCENE_SELECT, LUA_CONSOLE, LUA_SCRIPTS, ERROR_LOG, HOTKEYS, ABOUT, COUNT };

void set_owner(SDL_Window* game_window);  // call once the game window exists
void show(Id id);                         // open the window and bring it to the front
void toggle(Id id);                       // menu items and hotkeys: open, or close if open
bool visible(Id id);
void tick();                              // once per frame: Qt housekeeping, refresh open windows
void shutdown();                          // close everything before the game window goes away

}  // namespace tool_windows

#endif
