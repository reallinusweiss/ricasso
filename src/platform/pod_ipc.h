// ============================================================================
// pod_ipc.h — Ruby GG ⇄ Swordfare "engine pod" IPC
//
// A mini Swordigo emulator session runs as a child process ("pod") spawned by
// Ruby GG. It renders into a hidden SDL/GL window at low resolution and pumps
// each finished frame into a shared-memory triple buffer. Ruby GG displays the
// newest frame in a dock widget (zoomable, like a video player) and sends input
// / pause / mute / quit commands back over the pod's stdin.
//
//   ┌───────────────────────────┐        shm: 3×RGBA frame slots        ┌─────────────┐
//   │ swordfare --pod-preview   │ ─────────────────────────────────────▶ │ Ruby GG     │
//   │   (hidden GL window)      │ ◀───────────────────────────────────── │ emulator    │
//   └───────────────────────────┘   stdin: length-prefixed PodMsg stream │ dock widget │
//
// Frame transport = lock-free multi-producer safe triple buffer with
// per-slot generation counters (child is the only writer, ruby the only
// reader). Control transport = a simple length-prefixed binary stream on the
// child's stdin (only meaningful when the pod is spawned with a pipe).
//
// == Why this design (see docs/emulation_and_arm64/ruby_inline_emulator.md) ==
//  * The 8335-line swordfare host is a monolithic SDL app. Running it as an
//    isolated child reuses the ENTIRE boot + Dynarmic + SRE13 pipeline
//    unchanged and guarantees a guest crash can never take Ruby GG down.
//  * glReadPixels from the hidden window's back buffer right before Swap is
//    the single capture point that includes every rendering pass (FBO, portal,
//    post-FX) but excludes host ImGui overlays (disabled in pod mode).
//  * QImage blit in the dock is trivially zoomable and works identically on
//    X11/Wayland/Windows — no SDL-window-embedding or GL-context-sharing
//    fragility, no native-window re-parenting problems.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace pod {

// ---------------------------------------------------------------------------
// Shared-memory frame ring
// ---------------------------------------------------------------------------
constexpr uint32_t kShmMagic   = 0x31505753u;      // "SWP1"
// v2: ShmHeader carries per-slot slot_pub[] publish-sequence markers so the
// consumer can pick the TRUE newest frame by publish order (v1's gen-ack
// lag heuristic silently picked 1-2-frame-old frames when the producer ran
// ahead, causing "previous frame" ghosting in the Ruby viewer).
constexpr uint32_t kShmVersion = 2u;
constexpr uint32_t kFrameSlots = 3u;               // triple buffer

// Pod lifecycle / session states written into the shm header by the child.
enum PodState : uint32_t {
    kStateBooting = 1,
    kStateRunning = 2,
    kStatePaused  = 3,   // loop frozen by ruby (pause button)
    kStateExiting = 4,
    kStateCrashed = 5,   // child is gone / never started cleanly
};

struct FrameLayout {
    uint32_t width  = 0;   // pixels
    uint32_t height = 0;
    uint32_t stride = 0;   // width * 4
    uint32_t bytes  = 0;   // stride * height
};

// Producer (child) / consumer (ruby) handle over one shm object.
class FrameRing {
public:
    ~FrameRing();

    FrameRing(const FrameRing&) = delete;
    FrameRing& operator=(const FrameRing&) = delete;

    // ---- Producer side (swordfare pod) ------------------------------------
    // Creates (shm_open + ftruncate) or reuses an existing shm of the exact
    // frame size. Returns nullptr on failure.
    static FrameRing* create(const std::string& name, uint32_t w, uint32_t h);

    // Publish one RGBA8 frame (top-left origin, stride == w*4). Returns false
    // if the consumer dropped out of memory (shm gone).
    bool publish(const uint8_t* rgba, uint32_t w, uint32_t h);

    // ---- Consumer side (ruby) ---------------------------------------------
    // Attaches to an existing shm. Returns nullptr if the pod isn't up yet.
    static FrameRing* open(const std::string& name);

    // Copy the newest available frame into dst (capacity w*h*4). Returns the
    // frame sequence number copied, or 0 when no new frame has arrived since
    // the previous call (caller should keep showing the last frame).
    uint64_t grab(uint8_t* dst);

    // ---- Shared state ------------------------------------------------------
    void   set_state(uint32_t s);
    uint32_t state() const;
    void   set_fps(uint32_t fps);
    uint32_t fps() const;
    FrameLayout layout() const;
    uint64_t last_seq() const { return m_last_seq; }
    bool valid() const { return m_map != nullptr; }
    const std::string& name() const { return m_name; }

private:
    FrameRing() = default;
    bool map_common(const std::string& name, uint32_t w, uint32_t h, bool create);
    void unmap();

    std::string m_name;
    void*  m_map = nullptr;   // mapped segment
    size_t m_size = 0;
    // Header pointer (inside m_map), see impl for layout.
    struct Impl;
    Impl* m = nullptr;
    uint64_t m_last_seq = 0;
    bool m_creator = false;

private:
    // Byte offset of slot `slot` payload inside the mapping.
    uint8_t* slot_base(int slot) const;
};

// Sanitized name helper (converts any non [A-Za-z0-9_.] to '_' and prefixes).
std::string shm_sanitize(const std::string& raw);

// Removes a shared-memory object by name (no-op on platforms without named
// POSIX shm). Used by ruby after the pod process ends to release /dev/shm.
void shm_unlink_name(const std::string& name);

// ---------------------------------------------------------------------------
// Control messages (ruby → pod over pod stdin)
// ---------------------------------------------------------------------------
enum PodMsgKind : uint32_t {
    kMsgPause    = 1,  // a = 1 pause, 0 resume
    kMsgMute     = 2,  // toggle host music mute
    kMsgQuit     = 3,  // clean session exit
    kMsgKey      = 4,  // key event: key=SDL keycode, scancode, mods, a=down(1)/up(0), x=repeat
    kMsgMouseBtn = 5,  // a=button, mods=1 down/0 up, f[0..1]=normalized position
    kMsgMouseMove= 6,  // f[0..1]=normalized position
    kMsgWheel    = 7,  // a = y steps
    kMsgTouch    = 8,  // a=action 1/2/4, mods=finger id, f[0..3]=nx,ny,dx,dy
    kMsgText     = 9,  // UTF-8 text (from Qt text-input events)
    kMsgVolume   = 10, // a = 0..100 master volume (0 = mute) — phase 2 nicety
    kMsgShiftScene = 11, // Scene Shifter dispatch (Ruby "▶ Run scene"):
                          //   target = stem of <name>.scene (no extension),
                          //   spawn  = "start" or custom, a = 1 normal / 2 forced
};

struct PodMsg {
    uint32_t kind   = 0;
    uint32_t a      = 0;   // generic int arg (paused flag / button / shift mode)
    int32_t  x      = 0;
    int32_t  y      = 0;
    int32_t  x2     = 0;
    int32_t  y2     = 0;
    uint32_t key    = 0;   // SDL_Keycode
    uint32_t scancode = 0;
    uint32_t mods   = 0;   // SDL_Keymod / mouse down flag / finger id
    float    f[4]   = {0, 0, 0, 0};
    char     target[128] = {0};  // kMsgShiftScene: scene stem
    char     spawn[64] = {0};    // kMsgShiftScene: spawn point ("start")
    char     text[32] = {0};     // kMsgText: UTF-8 text
};
static_assert(sizeof(PodMsg) <= 512, "PodMsg must stay within one fixed frame");

// Writer (ruby): push one message into a caller-owned byte buffer stream.
// Returns bytes appended. The stream format is simply:
//     [u32 little-endian length][length payload bytes]
class MsgStream {
public:
    explicit MsgStream(std::string* out) : m_out(out) {}
    void push(const PodMsg& m);
private:
    std::string* m_out;
};

// Reader (pod control thread): pulls all complete messages from a byte chunk.
// Returns true if any message was extracted. Messages are dispatched to cb.
using MsgHandler = void (*)(const PodMsg& msg, void* user);

// Consumes `len` bytes from a raw stream chunk, dispatching complete messages
// to `cb` and buffering partial ones in `carry` (pass the SAME carry between
// calls). Returns bytes consumed. Declaration only — impl in pod_ipc.cpp.
size_t pod_dispatch_stream(const uint8_t* data, size_t len,
                           std::string* carry, MsgHandler cb, void* user);

} // namespace pod
