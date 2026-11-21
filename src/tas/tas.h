// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ricasso TAS mode (swordfare --tas): tool-assisted speedrun support for the
// ARM64 game path, in the spirit of BizHawk.
//
//   * Deterministic timing and random numbers. Every frame advances the game
//     by exactly 1/60 s, the guest's time(), gettimeofday(), clock_gettime()
//     and clock() read a virtual clock derived from the frame counter, and
//     rand()/lrand48() come from a seeded generator (--tas-seed=N). The game
//     draws its own random numbers from Caver::fastrandom(), whose state is
//     guest memory; a movie can reseed both on any frame (Input::rng).
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

#include <array>
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

constexpr int kMaxExtra = 20;      // most extra presses of one button in one frame

// One frame of player input, as the game sees it.
struct Input {
    uint64_t buttons = 0;   // bit i: control button i (controls_arm64.ini order) is held at the end of the frame
    int16_t mouse_x = 0;    // last known pointer position in game coordinates
    int16_t mouse_y = 0;    // (960 x 544, origin bottom left)
    uint8_t mouse_down = 0;
    // Presses of button i (63: Enter) in this frame beyond the one a change
    // from released to held implies. A button can go down several times in
    // one frame (a tap that starts and ends between two frames, or mashing),
    // and the game sees every press. See press_sequence().
    std::array<uint8_t, 64> extra{};
    // Not 0: right before this frame runs, the random generators (the game's
    // own Caver::fastrandom and the libc rand() replacement) are seeded with
    // this 32-bit value.
    uint64_t rng = 0;

    bool operator==(const Input& o) const {
        return buttons == o.buttons && mouse_x == o.mouse_x && mouse_y == o.mouse_y &&
               mouse_down == o.mouse_down && extra == o.extra && rng == o.rng;
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

// A key or gamepad button bound to control `index` (or kButtonEnter) went
// down (key event, no auto-repeat). Counts presses between two frames so a
// short tap or several presses are kept, not only the polled held state.
// Ignored in Play mode.
void note_press(int index);
// Extra presses of `index` in this frame's effective input (call after apply_*).
int extra_presses(int index);
// The states a button goes through in one frame, in order, written to out
// (at most cap entries; returns the count). n = extra + (was_held ? 0 : held ? 1 : 0)
// presses: if was_held and n > 0 it is released first; then n times pressed,
// each press followed by a release except the last one when `held`.
// With n == 0 and was_held && !held it is just released.
int press_sequence(bool was_held, bool held, int extra, bool* out, int cap);

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
// Record (F9): stops a recording; otherwise continues the movie at the
// current frame when the game is on it (during playback, or after playback
// reached the end), and starts a new movie only when there is none.
void toggle_record();
void new_recording();  // always a new movie; the previous files are kept as .bak
void toggle_play();
void stop_movie();
bool save_movie();  // returns false when the movie could not be written
void toggle_pause();
void step_frame();
Mode mode();
void toggle_fast_forward();  // latched fast-forward, independent of Tab
bool fast_forward_latched();
std::string directory();  // folder for states and movies
// Queues a new random generator state for the frame that runs next and
// stores it in the movie at that frame, so playback reproduces it.
void reroll_rng();
uint64_t new_rng_value();  // a fresh seed for Input::rng: 32 bits, never 0
// Called with Input::rng on the frames that have one; main.cpp seeds the
// game's generator there.
using ReseedFn = void (*)(uint32_t seed);
void set_reseed_hook(ReseedFn fn);

uint64_t frame();
double time_seconds();   // virtual time since boot: frames / 60
uint64_t realtime_ns();  // virtual wall clock for time() and friends
uint32_t next_rand();    // deterministic rand()

// --- Movie editing (TAS Studio window, main thread) ---------------------------

struct MovieInfo {
    uint64_t first = 0;                 // first frame of the movie: its start state's frame, else 0
    uint64_t length = 0;                // movie size; frames first..length-1 hold input
    uint64_t current = 0;               // the frame that runs next
    uint64_t stale_from = UINT64_MAX;   // frames from here to current were edited after they ran
    uint64_t seek_target = UINT64_MAX;  // a seek is running towards this frame
    uint64_t version = 0;               // changes whenever the movie's content changes
    Mode mode = Mode::Live;
    bool has_start = false;             // the movie has a start state (it can be replayed)
};
MovieInfo movie_info();
Input movie_frame(uint64_t frame);  // Input() past the end
// Whether a frame can be edited: there is a movie, the frame is not before its
// start, and in Record mode it is before the current frame.
bool editable(uint64_t frame);
bool set_movie_frame(uint64_t frame, const Input& in);  // grows the movie with empty frames if needed
bool insert_movie_frames(uint64_t frame, uint64_t count);  // not in Record mode
bool delete_movie_frames(uint64_t frame, uint64_t count);  // not in Record mode
// Goes to a frame of the movie: replays from the start state when the frame
// lies behind (or edits made the way there stale), runs at full speed in Play
// mode and pauses on arrival. Takes effect at the next begin_frame().
void seek(uint64_t frame);

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
