// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ricasso TAS mode. See tas.h for the overview.

#include "tas.h"

#include "game/mod_tools.h"
#include "platform/data_path.h"
#include "platform/gui.h"
#include "platform/input_config.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

extern InputConfig g_input_config;
std::string tas_bridge_describe_addr(uint64_t addr);  // jni_bridge_arm64.cpp

namespace tas {

namespace {

constexpr uint32_t kPageShift = 12;
constexpr uint32_t kPageSize = 1u << kPageShift;
constexpr uint64_t kEpochSeconds = 1767225600ull;  // 2026-01-01T00:00:00Z: the virtual wall clock at boot
constexpr uint32_t kStateVersion = 2;  // 1: host blocks without blobs (still readable)
constexpr const char* kStateMagic = "RTAS";

// One registered piece of host state (register_state) inside a snapshot.
struct HostBlock {
    std::string name;
    std::vector<uint8_t> bytes;
    std::vector<Blob> blobs;  // shared data the bytes refer to (Writer::blob)
};

struct Snapshot {
    uint64_t frame = 0;
    uint64_t rng = 0;
    Input prev;
    std::vector<uint32_t> pages;   // ascending page indices
    std::vector<uint8_t> data;     // pages.size() * kPageSize
    std::vector<uint64_t> bitmap;  // page index -> included
    std::vector<HostBlock> host;

    size_t bytes() const {  // size on disk; in RAM the blobs are shared between states
        size_t n = data.size();
        for (const auto& h : host) {
            n += h.bytes.size();
            for (const Blob& b : h.blobs) n += b->size();
        }
        return n;
    }
};

struct StateHooks {
    std::string name;
    SaveFn save;
    LoadFn load;
};

bool s_enabled = false;
uint64_t s_seed = 0;
std::string s_movie_arg;
std::string s_lib_id;

uint8_t* s_mem = nullptr;
uint64_t s_mem_size = 0;
std::vector<uint64_t> s_written;  // pages ever written (bitmap)
bool s_watch_ok = false;

uint64_t s_frame = 0;
uint64_t s_rng = 0;
Mode s_mode = Mode::Live;
std::vector<Input> s_movie;
Input s_cur, s_prev;
bool s_frame_runs = false;
bool s_fast_forward = false;   // Tab is held
bool s_ff_latched = false;     // toggled from the menu
bool s_wants_exit = false;

int s_slot = 0;
std::array<std::shared_ptr<Snapshot>, kSlots> s_slots;
int s_pending_save = -1;
int s_pending_load = -1;
int s_boot_load = -1;  // --tas-load=N: load that slot (from disk) on the first frame

// The movie's start state: taken on the first recorded frame, written next
// to the movie, restored by F10 so playback always begins where the
// recording began (like a BizHawk movie that starts from a savestate).
std::shared_ptr<Snapshot> s_movie_start;
bool s_movie_start_pending = false;  // take the start state at the next begin_frame
bool s_boot_movie_start = false;     // --tas-movie playback: restore the start state on the first frame
bool s_pending_play = false;         // F10: restore the start state and play at the next begin_frame

std::vector<StateHooks> s_hooks;
std::vector<ResetFn> s_resets;

std::vector<std::future<void>> s_writers;
std::mutex s_disk_mutex;

// --tas-selftest=FRAME,LEN
bool s_selftest = false;
uint64_t s_selftest_frame = 0;
uint64_t s_selftest_len = 0;
int s_selftest_stage = 0;
uint64_t s_selftest_hash = 0;
std::shared_ptr<Snapshot> s_selftest_ref;  // memory at the end of the first run
std::vector<uint64_t> s_selftest_hashes;    // per frame of the first run, from the save frame on
std::vector<std::pair<uint64_t, std::shared_ptr<Snapshot>>> s_selftest_early;  // first frames after the save
bool s_selftest_reported = false;

std::string s_last_message;

void say(const std::string& msg, float seconds = 2.0f) {
    s_last_message = msg;
    std::cout << "[TAS] " << msg << std::endl;
    mod_toast(msg, seconds);
}

std::string tas_dir() {
    std::string dir = get_user_data_dir();
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '/';
    dir += "tas/";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string movie_path() {
    return s_movie_arg.empty() ? tas_dir() + "movie.rtas" : s_movie_arg;
}

std::string movie_start_path() {
    std::string path = movie_path();
    const std::string ext = ".rtas";
    if (path.size() > ext.size() && path.compare(path.size() - ext.size(), ext.size(), ext) == 0)
        path.erase(path.size() - ext.size());
    return path + ".start.rsav";
}

std::string slot_path(int slot) {
    return tas_dir() + "state" + std::to_string(slot) + ".rsav";
}

// --- Random numbers (splitmix64) ---------------------------------------------

uint64_t splitmix(uint64_t& state) {
    uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// --- Guest memory tracking --------------------------------------------------

inline bool bit(const std::vector<uint64_t>& bm, uint32_t i) {
    return (bm[i >> 6] >> (i & 63)) & 1;
}
inline void set_bit(std::vector<uint64_t>& bm, uint32_t i) {
    bm[i >> 6] |= 1ull << (i & 63);
}

// Folds the pages written since the last call into s_written.
void sync_written() {
#ifdef _WIN32
    if (!s_mem || !s_watch_ok) return;
    static std::vector<void*> buf(1 << 16);
    for (;;) {
        ULONG_PTR count = buf.size();
        DWORD granularity = 0;
        if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, s_mem, (SIZE_T)s_mem_size, buf.data(), &count,
                          &granularity) != 0) {
            s_watch_ok = false;
            std::cerr << "[TAS] GetWriteWatch failed (" << GetLastError() << "); savestates are off" << std::endl;
            return;
        }
        for (ULONG_PTR i = 0; i < count; i++)
            set_bit(s_written, (uint32_t)(((uint8_t*)buf[i] - s_mem) >> kPageShift));
        if (count < buf.size()) break;
    }
#endif
}

uint64_t hash_guest_memory() {
    sync_written();
    uint64_t h = 0xcbf29ce484222325ull;
    const uint32_t words = (uint32_t)s_written.size();
    for (uint32_t w = 0; w < words; w++) {
        uint64_t bits = s_written[w];
        while (bits) {
            const uint32_t page = w * 64 + (uint32_t)__builtin_ctzll(bits);
            bits &= bits - 1;
            const uint64_t* p = (const uint64_t*)(s_mem + (uint64_t)page * kPageSize);
            for (uint32_t i = 0; i < kPageSize / 8; i++) {
                h ^= p[i];
                h *= 0x100000001b3ull;
            }
        }
    }
    return h;
}

std::shared_ptr<Snapshot> take_snapshot() {
    auto snap = std::make_shared<Snapshot>();
    snap->frame = s_frame;
    snap->rng = s_rng;
    snap->prev = s_prev;
    sync_written();
    snap->bitmap = s_written;
    size_t count = 0;
    for (uint64_t w : s_written) count += (size_t)__builtin_popcountll(w);
    snap->pages.reserve(count);
    snap->data.resize(count * (size_t)kPageSize);
    uint8_t* dst = snap->data.data();
    const uint32_t words = (uint32_t)s_written.size();
    for (uint32_t w = 0; w < words; w++) {
        uint64_t bits = s_written[w];
        while (bits) {
            const uint32_t page = w * 64 + (uint32_t)__builtin_ctzll(bits);
            bits &= bits - 1;
            snap->pages.push_back(page);
            memcpy(dst, s_mem + (uint64_t)page * kPageSize, kPageSize);
            dst += kPageSize;
        }
    }
    for (const auto& h : s_hooks) {
        HostBlock block;
        block.name = h.name;
        Writer w(block.bytes, &block.blobs);
        h.save(w);
        snap->host.push_back(std::move(block));
    }
    return snap;
}

bool restore_snapshot(const Snapshot& snap) {
    sync_written();
    if (snap.bitmap.size() != s_written.size()) return false;
    const uint8_t* src = snap.data.data();
    const uint32_t words = (uint32_t)s_written.size();
    for (uint32_t w = 0; w < words; w++) {
        const uint64_t in_snap = snap.bitmap[w];
        uint64_t to_zero = s_written[w] & ~in_snap;
        while (to_zero) {
            const uint32_t page = w * 64 + (uint32_t)__builtin_ctzll(to_zero);
            to_zero &= to_zero - 1;
            memset(s_mem + (uint64_t)page * kPageSize, 0, kPageSize);
        }
        uint64_t to_copy = in_snap;
        while (to_copy) {
            const uint32_t page = w * 64 + (uint32_t)__builtin_ctzll(to_copy);
            to_copy &= to_copy - 1;
            memcpy(s_mem + (uint64_t)page * kPageSize, src, kPageSize);
            src += kPageSize;
        }
        s_written[w] |= in_snap;
    }
    for (const auto& h : s_hooks) {
        bool found = false;
        for (const HostBlock& block : snap.host) {
            if (block.name != h.name) continue;
            found = true;
            Reader r(block.bytes.data(), block.bytes.size(), &block.blobs);
            if (!h.load(r))
                std::cerr << "[TAS] host state '" << h.name << "' did not load cleanly" << std::endl;
        }
        // A state from an older build: that part of the host stays as it is.
        if (!found) say("State has no '" + h.name + "' data (saved by an older build)", 4.0f);
    }
    for (ResetFn fn : s_resets) fn();
    s_frame = snap.frame;
    s_rng = snap.rng;
    s_prev = snap.prev;
    s_cur = s_prev;
    sync_written();  // drop the dirty flags the restore itself produced
    return true;
}

// --- Disk ---------------------------------------------------------------------

void write_input(Writer& w, const Input& in) {
    w.u64(in.buttons);
    w.u32((uint32_t)(uint16_t)in.mouse_x);
    w.u32((uint32_t)(uint16_t)in.mouse_y);
    w.u8(in.mouse_down);
}

Input read_input(Reader& r) {
    Input in;
    in.buttons = r.u64();
    in.mouse_x = (int16_t)(uint16_t)r.u32();
    in.mouse_y = (int16_t)(uint16_t)r.u32();
    in.mouse_down = r.u8();
    return in;
}

void write_snapshot_file(const std::string& path, std::shared_ptr<const Snapshot> snap) {
    std::lock_guard<std::mutex> lock(s_disk_mutex);
    const std::string tmp = path + ".tmp";
    std::vector<uint8_t> header;
    Writer w(header);
    w.bytes(kStateMagic, 4);
    w.u32(kStateVersion);
    w.str(s_lib_id);
    w.u64(snap->frame);
    w.u64(snap->rng);
    write_input(w, snap->prev);
    w.u32((uint32_t)snap->host.size());
    for (const HostBlock& h : snap->host) {
        w.str(h.name);
        w.u32((uint32_t)h.bytes.size());
        w.bytes(h.bytes.data(), h.bytes.size());
        w.u32((uint32_t)h.blobs.size());
        for (const Blob& b : h.blobs) w.u32((uint32_t)b->size());
    }
    w.u32((uint32_t)snap->pages.size());
    std::ofstream f(tmp, std::ios::binary);
    if (!f) {
        std::cerr << "[TAS] cannot write " << tmp << std::endl;
        return;
    }
    f.write((const char*)header.data(), (std::streamsize)header.size());
    for (const HostBlock& h : snap->host)
        for (const Blob& b : h.blobs) f.write((const char*)b->data(), (std::streamsize)b->size());
    f.write((const char*)snap->pages.data(), (std::streamsize)(snap->pages.size() * 4));
    f.write((const char*)snap->data.data(), (std::streamsize)snap->data.size());
    f.close();
    if (!f) {
        std::cerr << "[TAS] write failed: " << tmp << std::endl;
        return;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        std::filesystem::rename(tmp, path, ec);
    }
    std::cout << "[TAS] wrote " << path << " (" << (snap->bytes() >> 20) << " MB)" << std::endl;
}

std::shared_ptr<Snapshot> read_snapshot_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return nullptr;
    std::vector<uint8_t> all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Reader r(all.data(), all.size());
    char magic[4] = {0};
    r.bytes(magic, 4);
    const uint32_t version = r.u32();
    if (memcmp(magic, kStateMagic, 4) != 0 || version < 1 || version > kStateVersion) {
        say("Not a Ricasso savestate: " + path);
        return nullptr;
    }
    const std::string lib = r.str();
    if (lib != s_lib_id) {
        say("State was made with another game library");
        return nullptr;
    }
    auto snap = std::make_shared<Snapshot>();
    snap->frame = r.u64();
    snap->rng = r.u64();
    snap->prev = read_input(r);
    const uint32_t blocks = r.u32();
    std::vector<std::vector<uint32_t>> blob_sizes(blocks);
    for (uint32_t i = 0; i < blocks && r.ok; i++) {
        HostBlock block;
        block.name = r.str();
        const uint32_t n = r.u32();
        if (!r.need(n)) break;
        block.bytes.resize(n);
        r.bytes(block.bytes.data(), n);
        const uint32_t blobs = version >= 2 ? r.u32() : 0;
        for (uint32_t k = 0; k < blobs && r.ok; k++) blob_sizes[i].push_back(r.u32());
        snap->host.push_back(std::move(block));
    }
    const uint32_t count = r.u32();
    for (size_t i = 0; i < snap->host.size() && r.ok; i++) {
        for (uint32_t size : blob_sizes[i]) {
            if (!r.need(size)) break;
            auto blob = std::make_shared<std::vector<uint8_t>>(size);
            r.bytes(blob->data(), size);
            snap->host[i].blobs.push_back(std::move(blob));
        }
    }
    snap->pages.resize(count);
    if (!r.bytes(snap->pages.data(), (size_t)count * 4)) r.ok = false;
    snap->data.resize((size_t)count * kPageSize);
    if (!r.bytes(snap->data.data(), snap->data.size())) r.ok = false;
    if (!r.ok) {
        say("Savestate file is damaged: " + path);
        return nullptr;
    }
    snap->bitmap.assign(s_written.size(), 0);
    for (uint32_t page : snap->pages) {
        if ((page >> 6) >= snap->bitmap.size()) {
            say("Savestate does not fit this guest memory");
            return nullptr;
        }
        set_bit(snap->bitmap, page);
    }
    return snap;
}

void prune_writers() {
    s_writers.erase(std::remove_if(s_writers.begin(), s_writers.end(),
                                   [](std::future<void>& f) {
                                       return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                                   }),
                    s_writers.end());
}

// --- Movie --------------------------------------------------------------------

bool write_movie(const std::string& path) {
    std::ofstream f(path);
    if (!f) {
        say("Cannot write movie " + path);
        return false;
    }
    f << "RicassoTAS 1\n";
    f << "library " << s_lib_id << "\n";
    f << "seed " << s_seed << "\n";
    f << "buttons";
    for (int i = 0; i < g_input_config.button_count(); i++) {
        const TouchButton* b = g_input_config.get_button(i);
        f << ' ' << (b ? b->name : std::string("?"));
    }
    size_t start = 0;
    while (start < s_movie.size() && s_movie[start] == Input()) start++;
    f << "\nstart " << start << "\n";
    f << "frames " << (s_movie.size() - start) << "\n";
    char line[64];
    for (size_t i = start; i < s_movie.size(); i++) {
        const Input& in = s_movie[i];
        snprintf(line, sizeof line, "|%llx|%d,%d,%d|\n", (unsigned long long)in.buttons, (int)in.mouse_x,
                 (int)in.mouse_y, (int)in.mouse_down);
        f << line;
    }
    return (bool)f;
}

bool read_movie(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    std::vector<Input> frames;
    bool header = false;
    size_t start = 0;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line[0] == '|') {
            unsigned long long buttons = 0;
            int mx = 0, my = 0, down = 0;
            if (sscanf(line.c_str(), "|%llx|%d,%d,%d|", &buttons, &mx, &my, &down) != 4) {
                say("Bad movie line: " + line);
                return false;
            }
            Input in;
            in.buttons = buttons;
            in.mouse_x = (int16_t)mx;
            in.mouse_y = (int16_t)my;
            in.mouse_down = (uint8_t)(down != 0);
            frames.push_back(in);
            continue;
        }
        if (line.rfind("RicassoTAS ", 0) == 0) header = true;
        else if (line.rfind("seed ", 0) == 0) s_seed = strtoull(line.c_str() + 5, nullptr, 10);
        else if (line.rfind("start ", 0) == 0) start = (size_t)strtoull(line.c_str() + 6, nullptr, 10);
        else if (line.rfind("library ", 0) == 0 && line.substr(8) != s_lib_id)
            std::cerr << "[TAS] movie was recorded with another game library: " << line.substr(8) << std::endl;
    }
    if (!header) {
        say("Not a Ricasso movie: " + path);
        return false;
    }
    s_movie.assign(start, Input());
    s_movie.insert(s_movie.end(), frames.begin(), frames.end());
    return true;
}

const char* mode_name(Mode m) {
    switch (m) {
        case Mode::Record: return "REC";
        case Mode::Play: return "PLAY";
        default: return "LIVE";
    }
}

void set_mode(Mode m) {
    if (s_mode == Mode::Record && m != Mode::Record) write_movie(movie_path());
    if (m == Mode::Record && s_mode != Mode::Record) {
        s_movie_start.reset();
        s_movie_start_pending = true;
    }
    s_mode = m;
}

// --- Savestate requests -------------------------------------------------------

void do_save(int slot) {
    if (!s_watch_ok) {
        say("Savestates are not available (guest memory is not write-watched)");
        return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    auto snap = take_snapshot();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    s_slots[slot] = snap;
    char msg[128];
    snprintf(msg, sizeof msg, "Saved state %d at frame %llu (%zu MB, %.0f ms)", slot, (unsigned long long)s_frame,
             snap->bytes() >> 20, ms);
    say(msg);
    prune_writers();
    s_writers.push_back(std::async(std::launch::async, write_snapshot_file, slot_path(slot),
                                   std::shared_ptr<const Snapshot>(snap)));
    if (s_mode == Mode::Record) write_movie(movie_path());
}

void do_load(int slot) {
    if (!s_watch_ok) {
        say("Savestates are not available (guest memory is not write-watched)");
        return;
    }
    std::shared_ptr<Snapshot> snap = s_slots[slot];
    if (!snap) {
        snap = read_snapshot_file(slot_path(slot));
        if (!snap) {
            say("Slot " + std::to_string(slot) + " is empty");
            return;
        }
        s_slots[slot] = snap;
    }
    const auto t0 = std::chrono::steady_clock::now();
    if (!restore_snapshot(*snap)) {
        say("Could not restore state " + std::to_string(slot));
        return;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (s_mode == Mode::Record && s_movie.size() > s_frame) s_movie.resize((size_t)s_frame);
    if (s_mode == Mode::Play && s_frame > s_movie.size()) {
        say("Movie ends before this state; switching to live input");
        s_mode = Mode::Live;
    }
    char msg[128];
    snprintf(msg, sizeof msg, "Loaded state %d: frame %llu (%.0f ms)", slot, (unsigned long long)s_frame, ms);
    say(msg);
}

// Lists where guest memory differs from a snapshot (self test diagnostics).
void report_memory_diff(const Snapshot& ref) {
    sync_written();
    size_t pages_differ = 0, bytes_differ = 0, ranges_shown = 0;
    const uint8_t* src = ref.data.data();
    const uint32_t words = (uint32_t)s_written.size();
    static const uint8_t zero_page[kPageSize] = {0};
    for (uint32_t w = 0; w < words; w++) {
        uint64_t bits = s_written[w] | ref.bitmap[w];
        while (bits) {
            const uint32_t page = w * 64 + (uint32_t)__builtin_ctzll(bits);
            bits &= bits - 1;
            const bool in_ref = bit(ref.bitmap, page);
            const uint8_t* a = in_ref ? src : zero_page;
            if (in_ref) src += kPageSize;
            const uint8_t* b = s_mem + (uint64_t)page * kPageSize;
            if (memcmp(a, b, kPageSize) == 0) continue;
            pages_differ++;
            for (uint32_t i = 0; i < kPageSize;) {
                if (a[i] == b[i]) { i++; continue; }
                uint32_t j = i;
                while (j < kPageSize && (a[j] != b[j] || (j + 1 < kPageSize && a[j + 1] != b[j + 1]))) j++;
                bytes_differ += j - i;
                if (ranges_shown < 40) {
                    char line[256];
                    int n = snprintf(line, sizeof line, "[TAS/selftest]   diff at 0x%llx (%u bytes): ",
                                     (unsigned long long)page * kPageSize + i, j - i);
                    for (uint32_t k = i; k < i + 8 && k < kPageSize; k++) n += snprintf(line + n, sizeof line - n, "%02x", a[k]);
                    n += snprintf(line + n, sizeof line - n, " -> ");
                    for (uint32_t k = i; k < i + 8 && k < kPageSize; k++) n += snprintf(line + n, sizeof line - n, "%02x", b[k]);
                    std::cout << line << "  [" << tas_bridge_describe_addr((uint64_t)page * kPageSize + i) << "]"
                              << std::endl;
                    ranges_shown++;
                }
                i = j;
            }
        }
    }
    std::cout << "[TAS/selftest] " << pages_differ << " pages / " << bytes_differ << " bytes differ" << std::endl;
}

void run_selftest_step() {
    if (!s_selftest) return;
    // RICASSO_TAS_WATCH=<hex guest address>: print that 32-bit word every frame.
    static const char* watch_env = getenv("RICASSO_TAS_WATCH");
    static const uint64_t watch = watch_env ? strtoull(watch_env, nullptr, 16) : 0;
    if (watch && watch + 4 <= s_mem_size && s_frame >= s_selftest_frame - 2 && s_frame <= s_selftest_frame + 12) {
        uint32_t v = 0;
        memcpy(&v, s_mem + watch, 4);
        std::cout << "[TAS/watch] stage " << s_selftest_stage << " frame " << s_frame << " value " << v << std::endl;
    }
    // Per-frame hashes find the first frame on which the second run differs.
    if (s_selftest_stage == 1 && s_frame > s_selftest_frame && s_frame < s_selftest_frame + s_selftest_len) {
        s_selftest_hashes.push_back(hash_guest_memory());
        if (s_frame <= s_selftest_frame + 12) s_selftest_early.emplace_back(s_frame, take_snapshot());
    } else if (s_selftest_stage == 2 && s_frame > s_selftest_frame && s_frame < s_selftest_frame + s_selftest_len) {
        const size_t i = (size_t)(s_frame - s_selftest_frame - 1);
        if (!s_selftest_reported && i < s_selftest_hashes.size() && hash_guest_memory() != s_selftest_hashes[i]) {
            s_selftest_reported = true;
            std::cout << "[TAS/selftest] first difference at frame " << s_frame << std::endl;
            for (const auto& e : s_selftest_early)
                if (e.first == s_frame) report_memory_diff(*e.second);
        }
    }
    if (s_selftest_stage == 0 && s_frame == s_selftest_frame) {
        do_save(0);
        s_selftest_hash = hash_guest_memory();
        s_selftest_stage = 1;
        std::cout << "[TAS/selftest] frame " << s_frame << ": state saved, hash " << std::hex << s_selftest_hash
                  << std::dec << std::endl;
    } else if (s_selftest_stage == 1 && s_frame == s_selftest_frame + s_selftest_len) {
        s_selftest_hash = hash_guest_memory();
        s_selftest_ref = take_snapshot();
        std::cout << "[TAS/selftest] frame " << s_frame << ": hash " << std::hex << s_selftest_hash << std::dec
                  << ", loading the state" << std::endl;
        do_load(0);
        s_selftest_stage = 2;
    } else if (s_selftest_stage == 2 && s_frame == s_selftest_frame + s_selftest_len) {
        const uint64_t h = hash_guest_memory();
        const bool pass = h == s_selftest_hash;
        std::cout << "[TAS/selftest] frame " << s_frame << ": hash " << std::hex << h << std::dec << " -> "
                  << (pass ? "PASS" : "FAIL") << " (guest memory "
                  << (pass ? "matches" : "differs from") << " the first run)" << std::endl;
        if (!pass && !s_selftest_reported && s_selftest_ref) report_memory_diff(*s_selftest_ref);
        s_selftest_ref.reset();
        s_selftest_early.clear();
        s_selftest_stage = 3;
        s_wants_exit = true;
    }
}

}  // namespace

// --- Setup -----------------------------------------------------------------

void parse_args(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--tas") {
            s_enabled = true;
        } else if (a.rfind("--tas-seed=", 0) == 0) {
            s_enabled = true;
            s_seed = strtoull(a.c_str() + 11, nullptr, 10);
        } else if (a.rfind("--tas-movie=", 0) == 0) {
            s_enabled = true;
            s_movie_arg = a.substr(12);
        } else if (a.rfind("--tas-load=", 0) == 0) {
            s_enabled = true;
            s_boot_load = atoi(a.c_str() + 11);
            if (s_boot_load < 0 || s_boot_load >= kSlots) s_boot_load = -1;
        } else if (a.rfind("--tas-selftest=", 0) == 0) {
            s_enabled = true;
            s_selftest = true;
            unsigned long long f = 0, n = 0;
            if (sscanf(a.c_str() + 15, "%llu,%llu", &f, &n) != 2 || n == 0) {
                f = 300;
                n = 300;
            }
            s_selftest_frame = f;
            s_selftest_len = n;
        }
    }
    if (s_enabled) {
        s_rng = s_seed;
        std::atexit(shutdown);
        std::at_quick_exit(shutdown);  // the process ends with std::quick_exit
        std::cout << "[TAS] mode on: fixed 60 fps timestep, seed " << s_seed
                  << (s_selftest ? ", self test" : "") << std::endl;
    }
}

bool enabled() { return s_enabled; }
bool write_watch_wanted() { return s_enabled; }

void set_guest_memory(uint8_t* base, uint64_t size) {
    s_mem = base;
    s_mem_size = size;
    s_written.assign((size_t)((size >> kPageShift) + 63) / 64, 0);
#ifdef _WIN32
    ULONG_PTR count = 1;
    DWORD granularity = 0;
    void* probe = nullptr;
    // A short buffer only limits what comes back; the call fails (with
    // ERROR_INVALID_PARAMETER) when the region has no write watch.
    s_watch_ok = GetWriteWatch(0, s_mem, (SIZE_T)s_mem_size, &probe, &count, &granularity) == 0;
    if (!s_watch_ok) std::cerr << "[TAS] guest memory has no write watch; savestates are off" << std::endl;
#else
    s_watch_ok = false;
    std::cerr << "[TAS] savestates need Windows (MEM_WRITE_WATCH)" << std::endl;
#endif
}

void set_library_id(const std::string& id) {
    s_lib_id = id;
    if (!s_enabled) return;
    // A movie given on the command line starts at boot: an existing file is
    // played back, a new one is recorded.
    if (!s_movie_arg.empty()) {
        if (read_movie(s_movie_arg)) {
            s_rng = s_seed;
            s_mode = Mode::Play;
            std::error_code ec;
            s_boot_movie_start = std::filesystem::exists(movie_start_path(), ec);
            say("Playing movie " + s_movie_arg + " (" + std::to_string(s_movie.size()) + " frames)", 4.0f);
        } else {
            s_mode = Mode::Record;
            s_movie_start_pending = true;
            say("Recording movie to " + s_movie_arg, 4.0f);
        }
    }
}

// --- Frame flow ---------------------------------------------------------------

void begin_frame() {
    if (!s_enabled) return;
    if (s_boot_movie_start) {
        s_boot_movie_start = false;
        s_movie_start = read_snapshot_file(movie_start_path());
        if (s_movie_start && restore_snapshot(*s_movie_start))
            say("Movie starts from frame " + std::to_string(s_frame), 3.0f);
    }
    if (s_boot_load >= 0) {
        s_pending_load = s_boot_load;
        s_boot_load = -1;
    }
    if (s_pending_load >= 0) {
        do_load(s_pending_load);
        s_pending_load = -1;
    }
    if (s_pending_save >= 0) {
        do_save(s_pending_save);
        s_pending_save = -1;
    }
    if (s_pending_play) {
        s_pending_play = false;
        if (s_movie_start && restore_snapshot(*s_movie_start)) {
            s_mode = Mode::Play;
            say("Playing movie from frame " + std::to_string(s_frame));
        } else {
            say("Could not restore the movie's start state");
        }
    }
    if (s_movie_start_pending && s_mode == Mode::Record) {
        s_movie_start_pending = false;
        if (s_watch_ok) {
            s_movie_start = take_snapshot();
            prune_writers();
            s_writers.push_back(std::async(std::launch::async, write_snapshot_file, movie_start_path(),
                                           std::shared_ptr<const Snapshot>(s_movie_start)));
        }
    }
    run_selftest_step();
    s_frame_runs = !g_game_paused || g_step_one_frame;
    if (s_mode == Mode::Play) {
        if (s_frame < s_movie.size()) {
            s_cur = s_movie[(size_t)s_frame];
        } else {
            say("Movie finished at frame " + std::to_string(s_frame));
            s_mode = Mode::Live;
        }
    }
    if (s_mode != Mode::Play) {
        s_cur.buttons = 0;
        s_cur.mouse_down = 0;
        // mouse_x/y keep the last known position
    }
}

bool frame_runs() { return s_enabled && s_frame_runs; }
bool fast_forward() { return s_fast_forward || s_ff_latched; }
bool wants_exit() { return s_wants_exit; }

bool apply_button(int index, bool device_down) {
    if (index < 0 || index >= 63) return device_down;
    const uint64_t mask = 1ull << index;
    if (s_mode == Mode::Play) return (s_cur.buttons & mask) != 0;
    if (device_down) s_cur.buttons |= mask;
    return device_down;
}

bool apply_enter(bool device_down) {
    const uint64_t mask = 1ull << kButtonEnter;
    if (s_mode == Mode::Play) return (s_cur.buttons & mask) != 0;
    if (device_down) s_cur.buttons |= mask;
    return device_down;
}

void apply_mouse(bool device_down, float game_x, float game_y) {
    if (s_mode == Mode::Play) return;
    s_cur.mouse_down = device_down ? 1 : 0;
    s_cur.mouse_x = (int16_t)std::clamp((int)std::lround(game_x), -32768, 32767);
    s_cur.mouse_y = (int16_t)std::clamp((int)std::lround(game_y), -32768, 32767);
}

const Input& input() { return s_cur; }
const Input& previous_input() { return s_prev; }

void end_frame() {
    if (!s_enabled || !s_frame_runs) return;
    if (s_mode == Mode::Record) {
        // The movie is indexed by frame number: frames before the recording
        // started stay empty, frames after it (rerecording) are dropped.
        s_movie.resize((size_t)s_frame);
        s_movie.push_back(s_cur);
    }
    s_prev = s_cur;
    s_frame++;
}

// --- Hotkeys ------------------------------------------------------------------

void prev_slot() {
    if (!s_enabled) return;
    s_slot = (s_slot + kSlots - 1) % kSlots;
    say("Slot " + std::to_string(s_slot) + (s_slots[s_slot] ? "" : " (empty)"), 1.0f);
}

void next_slot() {
    if (!s_enabled) return;
    s_slot = (s_slot + 1) % kSlots;
    say("Slot " + std::to_string(s_slot) + (s_slots[s_slot] ? "" : " (empty)"), 1.0f);
}

void select_slot(int slot) {
    if (!s_enabled || slot < 0 || slot >= kSlots) return;
    s_slot = slot;
    say("Slot " + std::to_string(s_slot) + (s_slots[s_slot] ? "" : " (empty)"), 1.0f);
}

int slot() { return s_slot; }

bool slot_filled(int slot) { return slot >= 0 && slot < kSlots && s_slots[slot] != nullptr; }

void toggle_record() {
    if (!s_enabled) return;
    if (s_mode == Mode::Record) {
        set_mode(Mode::Live);
        say("Recording stopped; movie written to " + movie_path());
    } else {
        set_mode(Mode::Record);
        say("Recording from frame " + std::to_string(s_frame));
    }
}

void toggle_play() {
    if (!s_enabled) return;
    if (s_mode == Mode::Play) {
        set_mode(Mode::Live);
        say("Playback stopped at frame " + std::to_string(s_frame));
        return;
    }
    if (s_mode == Mode::Record) set_mode(Mode::Live);  // writes the movie
    if (s_movie.empty() && !read_movie(movie_path())) {
        say("No movie to play (" + movie_path() + ")");
        return;
    }
    if (!s_movie_start) {
        std::error_code ec;
        if (std::filesystem::exists(movie_start_path(), ec)) s_movie_start = read_snapshot_file(movie_start_path());
    }
    if (s_movie_start) {
        s_pending_play = true;  // restored at the top of the next frame
    } else if (s_frame >= s_movie.size()) {
        say("Movie has only " + std::to_string(s_movie.size()) +
            " frames and no start state; load an earlier state first");
    } else {
        set_mode(Mode::Play);
        say("Playing from frame " + std::to_string(s_frame));
    }
}

void stop_movie() {
    if (!s_enabled) return;
    if (s_mode == Mode::Record) {
        set_mode(Mode::Live);  // writes the movie
        say("Recording stopped; movie written to " + movie_path());
    } else if (s_mode == Mode::Play) {
        set_mode(Mode::Live);
        say("Playback stopped at frame " + std::to_string(s_frame));
    } else {
        say("No movie is recording or playing");
    }
}

bool save_movie() {
    if (!s_enabled) return false;
    const bool ok = write_movie(movie_path());
    if (ok) say("Movie written to " + movie_path());
    return ok;
}

void toggle_pause() {
    if (!s_enabled) return;
    g_game_paused = !g_game_paused;
    g_step_one_frame = false;
    say(g_game_paused ? "Paused (N = frame advance)" : "Resumed", 1.0f);
}

void step_frame() {
    if (!s_enabled) return;
    if (!g_game_paused) {
        g_game_paused = true;
        say("Paused (N = frame advance)", 1.0f);
    } else {
        g_step_one_frame = true;
    }
}

Mode mode() { return s_mode; }

void toggle_fast_forward() {
    if (!s_enabled) return;
    s_ff_latched = !s_ff_latched;
    say(s_ff_latched ? "Fast-forward on" : "Fast-forward off", 1.0f);
}

bool fast_forward_latched() { return s_ff_latched; }

std::string directory() { return tas_dir(); }

// Tab fast-forwards while it is held. The other TAS keys are menu items and
// hotkeys (platform/hotkeys.cpp), not handled here.
bool consume_key(const SDL_KeyboardEvent& key) {
    if (!s_enabled) return false;
    if (key.key != SDLK_TAB) return false;
    s_fast_forward = key.type == SDL_EVENT_KEY_DOWN;
    return true;
}

void request_save(int slot) {
    if (!s_enabled) return;
    s_pending_save = slot < 0 ? s_slot : slot;
}

void request_load(int slot) {
    if (!s_enabled) return;
    s_pending_load = slot < 0 ? s_slot : slot;
}

uint64_t frame() { return s_frame; }
double time_seconds() { return (double)s_frame / 60.0; }

uint64_t realtime_ns() {
    return kEpochSeconds * 1000000000ull + (s_frame * 1000000000ull) / 60ull;
}

uint32_t next_rand() { return (uint32_t)(splitmix(s_rng) >> 32); }

// --- Overlay ------------------------------------------------------------------

void render_overlay(GuiRenderer& gui, int win_w, int win_h) {
    if (!s_enabled) return;
    (void)win_w;
    char line[256];
    float y = (float)win_h - 18.0f;
    const float x = 8.0f;
    snprintf(line, sizeof line, "TAS  frame %llu  %s  slot %d%s%s%s", (unsigned long long)s_frame, mode_name(s_mode),
             s_slot, s_slots[s_slot] ? "" : " (empty)", g_game_paused ? "  PAUSED" : "",
             s_fast_forward ? "  >>" : "");
    gui.draw_string(line, x, y, 1.2f, 255, 230, 120, 255);
    y -= 16.0f;
    std::string held;
    for (int i = 0; i < g_input_config.button_count() && i < 63; i++) {
        if (!((s_prev.buttons >> i) & 1)) continue;
        const TouchButton* b = g_input_config.get_button(i);
        if (!held.empty()) held += ' ';
        held += b ? b->name : std::string("?");
    }
    if ((s_prev.buttons >> kButtonEnter) & 1) held += held.empty() ? "enter" : " enter";
    if (s_prev.mouse_down) {
        snprintf(line, sizeof line, "%s%smouse %d,%d", held.c_str(), held.empty() ? "" : "  ", (int)s_prev.mouse_x,
                 (int)s_prev.mouse_y);
    } else {
        snprintf(line, sizeof line, "%s", held.empty() ? "-" : held.c_str());
    }
    gui.draw_string(line, x, y, 1.1f, 160, 220, 255, 255);
    y -= 15.0f;
    if (s_mode == Mode::Play || !s_movie.empty()) {
        if (s_movie_start)
            snprintf(line, sizeof line, "movie frames %llu-%zu", (unsigned long long)s_movie_start->frame,
                     s_movie.size());
        else
            snprintf(line, sizeof line, "movie %zu frames", s_movie.size());
        gui.draw_string(line, x, y, 1.0f, 150, 150, 170, 220);
        y -= 14.0f;
    }
}

void shutdown() {
    if (!s_enabled) return;
    if (s_mode == Mode::Record) write_movie(movie_path());
    for (auto& f : s_writers) f.wait();
    s_writers.clear();
}

// --- Host state registry --------------------------------------------------------

void register_state(const char* name, SaveFn save, LoadFn load) {
    s_hooks.push_back({name, save, load});
}

void register_reset(ResetFn fn) { s_resets.push_back(fn); }

}  // namespace tas
