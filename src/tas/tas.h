// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ricasso TAS mode (swordfare --tas): tool-assisted speedrun support for the
// ARM64 game path, in the spirit of BizHawk.
//
//   * Deterministic timing and random numbers. Every frame advances the game
//     by exactly 1/60 s, the guest's time(), gettimeofday(), clock_gettime()
//     and clock() read a virtual clock derived from the frame counter, and
//     rand()/lrand48() come from a seeded generator (--tas-seed=N).
//   * Input recording and playback. The input of every frame (control
//     buttons, Enter, the mouse) is kept in a movie that can be written to
//     and read from a text file (--tas-movie=FILE).
//   * Savestates. The whole guest memory (sparse: only pages that were ever
//     written) plus the host-side tables that describe it (heap bookkeeping,
//     JNI strings, TLS, the virtual clock, button states, the game's OpenGL
//     textures) go into one of ten slots in RAM and, in the background, to
//     disk.
//
// Limits: OpenAL sources are not part of a state, so sounds are not rewound.
// Files on disk (the game's own saves) are not rolled back either. Text input
// (typing mode) is not recorded.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

class GuiRenderer;

namespace tas {

constexpr float kFrameDt = 1.0f / 60.0f;
constexpr int kSlots = 10;
constexpr int kButtonEnter = 63;  // bit of Input::buttons that carries the Enter key

// One frame of player input, as the game sees it.
struct Input {
    uint64_t buttons = 0;   // bit i: control button i (controls_arm64.ini order) is held
    int16_t mouse_x = 0;    // last known pointer position in game coordinates
    int16_t mouse_y = 0;    // (960 x 544, origin bottom left)
    uint8_t mouse_down = 0;

    bool operator==(const Input& o) const {
        return buttons == o.buttons && mouse_x == o.mouse_x && mouse_y == o.mouse_y &&
               mouse_down == o.mouse_down;
    }
    bool operator!=(const Input& o) const { return !(*this == o); }
};

enum class Mode { Live, Record, Play };

// --- Setup -----------------------------------------------------------------

// Reads --tas, --tas-seed=N, --tas-movie=FILE, --tas-load=SLOT (restore that
// slot from disk on the first frame) and --tas-selftest=FRAME,LEN.
// Must run before the guest memory is reserved.
void parse_args(int argc, char** argv);
bool enabled();
bool write_watch_wanted();  // reserve the guest region with MEM_WRITE_WATCH

void set_guest_memory(uint8_t* base, uint64_t size);
void set_library_id(const std::string& id);  // identifies the game library in states and movies

// --- Frame flow (ARM64 loop) -------------------------------------------------

// Top of the frame: performs a pending state save/load and decides whether
// the guest update runs this frame.
void begin_frame();
bool frame_runs();     // the guest update runs this frame (not paused, or stepping)
bool fast_forward();   // Tab is held or fast-forward is latched: skip the frame limiter
bool wants_exit();     // the self test finished

// Input of this frame. In Live and Record mode the device state is taken,
// in Play mode the movie's; the return value is what the game gets.
bool apply_button(int index, bool device_down);
bool apply_enter(bool device_down);
void apply_mouse(bool device_down, float game_x, float game_y);
const Input& input();           // effective input of this frame
const Input& previous_input();  // effective input of the previous frame

// End of the frame: records the input and advances the frame counter when the
// guest update ran.
void end_frame();

// TAS hotkeys and menu actions. Actions are no-ops when TAS mode is off.
// consume_key handles only Tab (fast-forward while held); returns true when
// the event was consumed.
bool consume_key(const SDL_KeyboardEvent& key);
void request_save(int slot = -1);  // -1: current slot
void request_load(int slot = -1);
void prev_slot();
void next_slot();
void select_slot(int slot);
int slot();
bool slot_filled(int slot);
void toggle_record();
void toggle_play();
void stop_movie();
bool save_movie();  // returns false when the movie could not be written
void toggle_pause();
void step_frame();
Mode mode();
void toggle_fast_forward();  // latched fast-forward, independent of Tab
bool fast_forward_latched();
std::string directory();  // folder for states and movies

uint64_t frame();
double time_seconds();   // virtual time since boot: frames / 60
uint64_t realtime_ns();  // virtual wall clock for time() and friends
uint32_t next_rand();    // deterministic rand()

void render_overlay(GuiRenderer& gui, int win_w, int win_h);
void shutdown();  // writes the movie

// --- Host state that belongs in a savestate ----------------------------------

struct Writer;
struct Reader;
using SaveFn = void (*)(Writer& w);
using LoadFn = bool (*)(Reader& r);
using ResetFn = void (*)();
void register_state(const char* name, SaveFn save, LoadFn load);
void register_reset(ResetFn fn);  // runs after a state was restored

// Large immutable host data (texture pixels). A state keeps a reference, so
// saving copies nothing and states made one after the other share the data;
// the bytes are written out only when the state goes to disk.
using Blob = std::shared_ptr<const std::vector<uint8_t>>;

// Little-endian serialisation helpers for SaveFn/LoadFn.
struct Writer {
    std::vector<uint8_t>& out;
    std::vector<Blob>* blobs;
    explicit Writer(std::vector<uint8_t>& o, std::vector<Blob>* b = nullptr) : out(o), blobs(b) {}
    void blob(const Blob& b) {  // stored as 0 (none) or index + 1
        if (!b || !blobs) { u32(0); return; }
        blobs->push_back(b);
        u32((uint32_t)blobs->size());
    }
    void u8(uint8_t v) { out.push_back(v); }
    void u32(uint32_t v) { for (int i = 0; i < 4; i++) out.push_back((uint8_t)(v >> (8 * i))); }
    void u64(uint64_t v) { for (int i = 0; i < 8; i++) out.push_back((uint8_t)(v >> (8 * i))); }
    void f32(float v) { uint32_t u; static_assert(sizeof u == sizeof v, ""); memcpy(&u, &v, 4); u32(u); }
    void bytes(const void* p, size_t n) { out.insert(out.end(), (const uint8_t*)p, (const uint8_t*)p + n); }
    void str(const std::string& s) { u32((uint32_t)s.size()); bytes(s.data(), s.size()); }
};

struct Reader {
    const uint8_t* p;
    const uint8_t* end;
    const std::vector<Blob>* blobs;
    bool ok = true;
    Reader(const uint8_t* data, size_t size, const std::vector<Blob>* b = nullptr)
        : p(data), end(data + size), blobs(b) {}
    Blob blob() {
        const uint32_t i = u32();
        if (i == 0) return nullptr;
        if (!blobs || i > blobs->size()) { ok = false; return nullptr; }
        return (*blobs)[i - 1];
    }
    bool need(size_t n) { if ((size_t)(end - p) < n) { ok = false; return false; } return true; }
    uint8_t u8() { if (!need(1)) return 0; return *p++; }
    uint32_t u32() { if (!need(4)) return 0; uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)p[i] << (8 * i); p += 4; return v; }
    uint64_t u64() { if (!need(8)) return 0; uint64_t v = 0; for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i); p += 8; return v; }
    float f32() { uint32_t u = u32(); float v; memcpy(&v, &u, 4); return v; }
    bool bytes(void* dst, size_t n) { if (!need(n)) return false; memcpy(dst, p, n); p += n; return true; }
    std::string str() { uint32_t n = u32(); if (!need(n)) return std::string(); std::string s((const char*)p, n); p += n; return s; }
};

}  // namespace tas
