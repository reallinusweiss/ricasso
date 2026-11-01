// =============================================================================
// mem_watchpoint.h — Guest memory watchpoint system
//
// A lightweight, lock-free watchpoint list that sits between the Dynarmic
// MemoryWrite callbacks and the ImGui research tab.
//
// Design constraints (from the plan):
//   • Hot path: MemoryWrite{8,16,32,64} are called millions of times per sec.
//     Watchpoint checks must be O(W) where W is the number of active
//     watchpoints (≤64 is the hard cap).
//   • The watch list is read on the emulator thread; entries are
//     added/removed from the ImGui thread.  We use a std::mutex + a small
//     fixed array — contention is rare and brief.
//   • Hit records are written into a lock-free ring buffer so the render
//     thread can drain them without blocking the emulator thread.
//   • No dynamic allocation in the hot path.
// =============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <array>
#include <functional>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
inline constexpr int  kMaxWatchpoints   = 64;
inline constexpr int  kHitRingCapacity  = 4096;  // must be power-of-two
inline constexpr int  kMaxHitRecords    = 256;    // drain this many per frame

// ---------------------------------------------------------------------------
// WatchpointEntry — one watched address range
// ---------------------------------------------------------------------------
struct WatchpointEntry {
    uint64_t    va_start   = 0;
    uint64_t    va_end     = 0;      // exclusive; va_end == va_start+size
    uint32_t    id         = 0;      // unique ID (monotonically assigned)
    bool        active     = false;
    bool        watch_write= true;
    bool        watch_read = false;
    char        label[48]  = {};     // human-readable label
    char        struct_ctx[48] = {}; // optional: "GameSceneController+0x90"
};

// ---------------------------------------------------------------------------
// WatchHit — one recorded access
// ---------------------------------------------------------------------------
struct WatchHit {
    uint64_t    va         = 0;      // accessed guest VA
    uint64_t    value      = 0;      // value written (0 for reads)
    uint64_t    guest_pc   = 0;      // PC at time of access (best-effort; see plan)
    uint32_t    wp_id      = 0;      // which watchpoint triggered
    uint32_t    access_size= 0;      // bytes: 1/2/4/8
    bool        is_write   = true;
    uint64_t    seq        = 0;      // monotonic sequence number
};

// ---------------------------------------------------------------------------
// MemWatchpointManager — singleton
// ---------------------------------------------------------------------------
class MemWatchpointManager {
public:
    static MemWatchpointManager& instance();

    // ── Render/config thread ──────────────────────────────────────────────

    // Add a watchpoint.  Returns the assigned ID, or 0 if the list is full.
    uint32_t add(uint64_t va_start, uint32_t size,
                 const char* label,
                 bool watch_write = true, bool watch_read = false);

    // Remove a watchpoint by ID.
    void remove(uint32_t id);

    // List all current watchpoints (snapshot for the UI).
    std::vector<WatchpointEntry> list() const;

    // Drain hit records from the ring buffer into dst (up to max_count).
    // Returns number of records written.  Call once per ImGui frame.
    int drain_hits(WatchHit* dst, int max_count);

    // Clear all stored hits (ring buffer reset).
    void clear_hits();

    // Total number of hits observed since last clear_hits().
    uint64_t total_hits() const { return m_total_hits.load(std::memory_order_relaxed); }

    // ── Emulator thread (hot path) ────────────────────────────────────────

    // Called from MemoryWrite{8,16,32,64} callbacks.
    // va = guest VA, value = written value, size = access size in bytes.
    // pc = current JIT PC (may be block-start, not instruction-exact).
    void on_write(uint64_t va, uint64_t value, uint32_t size, uint64_t pc = 0);
    void on_read (uint64_t va, uint32_t size,  uint64_t pc = 0);

    // Quick inline check: returns true if ANY watchpoint could match this VA.
    // Used to gate the more expensive on_write() call.
    bool may_match(uint64_t va) const {
        uint64_t lo = m_range_lo.load(std::memory_order_relaxed);
        uint64_t hi = m_range_hi.load(std::memory_order_relaxed);
        return (m_count.load(std::memory_order_relaxed) > 0) &&
               (va >= lo && va < hi);
    }

private:
    MemWatchpointManager() = default;
    ~MemWatchpointManager() = default;

    void push_hit(const WatchHit& h);

    mutable std::mutex m_mu;

    // Watch list (accessed from both threads under m_mu)
    std::array<WatchpointEntry, kMaxWatchpoints> m_watchpoints{};
    std::atomic<int>      m_count{0};  // number of active entries (cache)
    std::atomic<uint64_t> m_range_lo{UINT64_MAX};  // min va_start across all
    std::atomic<uint64_t> m_range_hi{0};            // max va_end across all
    uint32_t m_next_id = 1;

    // Hit ring buffer (lock-free SPSC: emulator thread writes, render reads)
    std::array<WatchHit, kHitRingCapacity> m_ring{};
    std::atomic<uint32_t> m_ring_write{0};
    std::atomic<uint32_t> m_ring_read{0};
    std::atomic<uint64_t> m_total_hits{0};
    std::atomic<uint64_t> m_seq{0};
};

// ---------------------------------------------------------------------------
// Inline hot-path helpers called from Dynarmic callback
// ---------------------------------------------------------------------------
// These are the minimal wrappers the emulator callbacks should call.
// They compile to nearly nothing when no watchpoints are active.
// ---------------------------------------------------------------------------

inline void wp_on_write(uint64_t va, uint64_t value, uint32_t size, uint64_t pc = 0) {
    auto& m = MemWatchpointManager::instance();
    if (m.may_match(va)) m.on_write(va, value, size, pc);
}

inline void wp_on_read(uint64_t va, uint32_t size, uint64_t pc = 0) {
    auto& m = MemWatchpointManager::instance();
    if (m.may_match(va)) m.on_read(va, size, pc);
}

} // namespace swordfare::research
