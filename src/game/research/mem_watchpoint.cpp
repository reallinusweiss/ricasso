// =============================================================================
// mem_watchpoint.cpp — MemWatchpointManager implementation
// =============================================================================

#include "game/research/mem_watchpoint.h"
#include <cstring>
#include <algorithm>
#include <limits>

namespace swordfare::research {

MemWatchpointManager& MemWatchpointManager::instance() {
    static MemWatchpointManager s;
    return s;
}

// ---------------------------------------------------------------------------
// add / remove
// ---------------------------------------------------------------------------
uint32_t MemWatchpointManager::add(uint64_t va_start, uint32_t size,
                                    const char* label,
                                    bool watch_write, bool watch_read) {
    std::lock_guard<std::mutex> lg(m_mu);
    // Find a free slot
    int slot = -1;
    for (int i = 0; i < kMaxWatchpoints; ++i) {
        if (!m_watchpoints[i].active) { slot = i; break; }
    }
    if (slot < 0) return 0;   // full

    auto& wp = m_watchpoints[slot];
    wp = WatchpointEntry{};
    wp.va_start    = va_start;
    wp.va_end      = va_start + size;
    wp.id          = m_next_id++;
    wp.active      = true;
    wp.watch_write = watch_write;
    wp.watch_read  = watch_read;
    if (label) {
        strncpy(wp.label, label, sizeof(wp.label) - 1);
    }

    // Recompute global range
    uint64_t lo = std::numeric_limits<uint64_t>::max();
    uint64_t hi = 0;
    int cnt = 0;
    for (auto& e : m_watchpoints) {
        if (!e.active) continue;
        if (e.va_start < lo) lo = e.va_start;
        if (e.va_end   > hi) hi = e.va_end;
        ++cnt;
    }
    m_range_lo.store(lo, std::memory_order_relaxed);
    m_range_hi.store(hi, std::memory_order_relaxed);
    m_count.store(cnt,   std::memory_order_relaxed);

    return wp.id;
}

void MemWatchpointManager::remove(uint32_t id) {
    std::lock_guard<std::mutex> lg(m_mu);
    for (auto& wp : m_watchpoints) {
        if (wp.active && wp.id == id) {
            wp.active = false;
            break;
        }
    }
    uint64_t lo = std::numeric_limits<uint64_t>::max();
    uint64_t hi = 0;
    int cnt = 0;
    for (auto& e : m_watchpoints) {
        if (!e.active) continue;
        if (e.va_start < lo) lo = e.va_start;
        if (e.va_end   > hi) hi = e.va_end;
        ++cnt;
    }
    m_range_lo.store(cnt ? lo : UINT64_MAX, std::memory_order_relaxed);
    m_range_hi.store(cnt ? hi : 0,          std::memory_order_relaxed);
    m_count.store(cnt, std::memory_order_relaxed);
}

std::vector<WatchpointEntry> MemWatchpointManager::list() const {
    std::lock_guard<std::mutex> lg(m_mu);
    std::vector<WatchpointEntry> out;
    for (auto& wp : m_watchpoints)
        if (wp.active) out.push_back(wp);
    return out;
}

// ---------------------------------------------------------------------------
// push_hit — called from emulator thread; lock-free ring buffer write
// ---------------------------------------------------------------------------
void MemWatchpointManager::push_hit(const WatchHit& h) {
    m_total_hits.fetch_add(1, std::memory_order_relaxed);
    uint32_t w = m_ring_write.load(std::memory_order_relaxed);
    uint32_t r = m_ring_read.load(std::memory_order_acquire);
    uint32_t next_w = (w + 1) & (kHitRingCapacity - 1);
    if (next_w == r) return;   // full, drop oldest implicitly (ring)
    m_ring[w] = h;
    m_ring_write.store(next_w, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// on_write — called from Dynarmic MemoryWrite* callbacks (emulator thread)
// ---------------------------------------------------------------------------
void MemWatchpointManager::on_write(uint64_t va, uint64_t value,
                                    uint32_t size, uint64_t pc) {
    // We do NOT take m_mu here to keep the emulator thread hot-path lock-free.
    // We read the watchpoint array without a lock; worst case: we see a stale
    // entry or miss one entry during an add/remove.  For a research tool this
    // is acceptable — false negatives are rare and benign.
    uint64_t seq = m_seq.fetch_add(1, std::memory_order_relaxed);
    for (int i = 0; i < kMaxWatchpoints; ++i) {
        const auto& wp = m_watchpoints[i];
        if (!wp.active || !wp.watch_write) continue;
        if (va >= wp.va_start && va < wp.va_end) {
            WatchHit h;
            h.va          = va;
            h.value       = value;
            h.guest_pc    = pc;
            h.wp_id       = wp.id;
            h.access_size = size;
            h.is_write    = true;
            h.seq         = seq;
            push_hit(h);
        }
    }
}

// ---------------------------------------------------------------------------
// on_read
// ---------------------------------------------------------------------------
void MemWatchpointManager::on_read(uint64_t va, uint32_t size, uint64_t pc) {
    uint64_t seq = m_seq.fetch_add(1, std::memory_order_relaxed);
    for (int i = 0; i < kMaxWatchpoints; ++i) {
        const auto& wp = m_watchpoints[i];
        if (!wp.active || !wp.watch_read) continue;
        if (va >= wp.va_start && va < wp.va_end) {
            WatchHit h;
            h.va          = va;
            h.value       = 0;
            h.guest_pc    = pc;
            h.wp_id       = wp.id;
            h.access_size = size;
            h.is_write    = false;
            h.seq         = seq;
            push_hit(h);
        }
    }
}

// ---------------------------------------------------------------------------
// drain_hits — called from render/ImGui thread
// ---------------------------------------------------------------------------
int MemWatchpointManager::drain_hits(WatchHit* dst, int max_count) {
    int n = 0;
    while (n < max_count) {
        uint32_t r = m_ring_read.load(std::memory_order_relaxed);
        uint32_t w = m_ring_write.load(std::memory_order_acquire);
        if (r == w) break;
        dst[n++] = m_ring[r];
        m_ring_read.store((r + 1) & (kHitRingCapacity - 1),
                          std::memory_order_release);
    }
    return n;
}

void MemWatchpointManager::clear_hits() {
    m_ring_read.store(m_ring_write.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
    m_total_hits.store(0, std::memory_order_relaxed);
}

} // namespace swordfare::research
