#include "platform/input_config.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>

namespace fs = std::filesystem;
static int find(InputConfig& config, const std::string& name) {
    for (int i=0;i<config.button_count();++i)
        if (config.get_button(i)->name==name) return i;
    return -1;
}
static void rebind(InputConfig& config, const std::string& name, int key) {
    auto* button=config.get_button(find(config,name));
    config.set_editing(true);
    config.set_editor_mode(EDITOR_REBIND);
    config.editor_mouse_down(button->game_x,button->game_y);
    config.editor_handle_key(key);
}
int main() {
    const auto root=fs::current_path()/fs::u8path(u8"Swordigo Tastenprüfung ä "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path=root/fs::u8path(u8"Profil ü/save/controls_arm64.ini");
    InputConfig current;
    current.load(path.u8string()); // No existing file or parent directory.
    rebind(current,"attack",SDL_SCANCODE_DELETE);
    rebind(current,"magic",SDL_SCANCODE_END);
    rebind(current,"jump",SDL_SCANCODE_W);
    // No F2 close, explicit save, or destructor: a new reader must already see
    // all three edits, including the formerly discarded W binding.
    InputConfig restart; restart.load(path.u8string());
    if(restart.get_button(find(restart,"attack"))->sdl_scancode!=SDL_SCANCODE_DELETE) return 1;
    if(restart.get_button(find(restart,"magic"))->sdl_scancode!=SDL_SCANCODE_END) return 2;
    if(restart.get_button(find(restart,"jump"))->sdl_scancode_alt!=SDL_SCANCODE_W) return 3;
    auto* button=current.get_button(find(current,"attack"));
    const float target_x=button->game_x+12.25f,target_y=button->game_y+8.5f;
    current.editor_mouse_down(button->game_x,button->game_y);
    current.editor_mouse_move(target_x,target_y);
    current.editor_mouse_up();
    restart.load(path.u8string());
    button=restart.get_button(find(restart,"attack"));
    if(button->game_x!=target_x || button->game_y!=target_y) return 4;
    int added=current.add_custom_button("Custom",410.25f,250.5f,SDL_SCANCODE_V);
    restart.load(path.u8string());
    if(restart.button_count()!=current.button_count() || restart.get_button(added)->game_x!=410.25f) return 5;
    current.remove_button(added); restart.load(path.u8string());
    if(restart.button_count()!=current.button_count()) return 6;
    // Force replacement failure. The last complete file must survive intact.
    std::ifstream before_file(path,std::ios::binary);
    const std::string before((std::istreambuf_iterator<char>(before_file)),{});
    before_file.close();
    const fs::path temporary(path.native()+fs::path(".tmp").native());
    fs::create_directory(temporary);
    rebind(current,"attack",SDL_SCANCODE_HOME);
    std::ifstream after_file(path,std::ios::binary);
    const std::string after((std::istreambuf_iterator<char>(after_file)),{});
    after_file.close();
    if(before!=after || after.empty()) return 7;
    fs::remove(temporary); // Explicitly known empty test directory only.
    fs::remove(path);
    fs::remove(path.parent_path()); fs::remove(path.parent_path().parent_path()); fs::remove(root);
    std::cout << "Input: immediate rebind/drag/custom persistence, Unicode, W and failed-write preservation passed\n";
}
