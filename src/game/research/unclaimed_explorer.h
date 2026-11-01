// =============================================================================
// unclaimed_explorer.h — turn raw unclaimed memory into a ranked list of leads.
//
// The other panels start from something already identified.  This one starts
// from what is left over: every slot in the guest address space that is NOT
// covered by a live object, a seeded catalog field, or the address list.  It
// watches those slots move and ranks them by how their movement relates to the
// scene, so "what should I look at next" is a sorted list rather than a guess.
//
// Two constraints shape the whole design.
//
// 1. THE ADDRESS SPACE IS HUGE.  g_guest_memory is ~3.5 GB and stays mapped for
//    the whole session, so there is no per-slot bookkeeping over the region.
//    Instead there is a small DIRECT-MAPPED CHANGE CACHE (keyed by address
//    tag).  A pass walks a bounded slice and compares each slot against the
//    cache entry it maps to; only slots that actually MOVED are promoted into
//    the tracked set.  Static code and untouched tables therefore cost nothing,
//    and the tracked set is capped.  Memory is O(tracked + cache), not
//    O(region).  Per-track state is fixed size (no per-slot allocation).
//
// 2. THE PASS MUST NEVER BLOCK A FRAME.  Work per pass is bounded by
//    `slots_per_pass` and the region cursor is remembered, so the region is
//    swept over many frames while every individual pass stays cheap.
//
// HONESTY ABOUT THE SCENE CORRELATION.  A change is detected between two
// VISITS to a slot, not at the exact frame it happened, so what is recorded is
// an *association*: "in the N passes where this slot changed, M also carried a
// scene signal".  That is a lead, not a proof, so both numbers are reported and
// the evidence tier never exceeds SUSPECTED.
// =============================================================================
#pragma once

#include "game/research/live_object_map.h"
#include "game/research/mem_identity.h"
#include "game/research/mem_scanner.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// ExploreConfig
// ---------------------------------------------------------------------------
struct ExploreConfig {
    uint64_t region_begin  = 0;      // 0 = unset; the caller supplies a region
    uint64_t region_end    = 0;      // exclusive
    ValueType type         = ValueType::Dword;
    size_t   alignment     = 0;      // 0 = natural (type width)

    size_t   slots_per_pass = 8192;      // frame budget
    size_t   cache_slots    = 1u << 18;  // direct-mapped change cache (rounded to pow2)
    size_t   max_tracks     = 200000;    // hard cap on promoted slots

    uint64_t min_samples    = 2;     // before a slot is ranked at all
    size_t   cluster_gap    = 24;    // adjacency that suggests one struct
    size_t   max_leads      = 300;
};

// ---------------------------------------------------------------------------
// SlotTrack — everything known about one slot that has moved at least once.
// Fixed size: 200k of these must not mean 200k heap allocations.
// ---------------------------------------------------------------------------
struct SlotTrack {
    static constexpr size_t kRecent = 6;   // recent values, most recent first
    static constexpr size_t kSeen   = 8;   // distinct-value set, saturating

    uint64_t  address = 0;
    ValueType type    = ValueType::Dword;

    uint32_t  samples        = 0;
    uint32_t  changes        = 0;
    uint32_t  passes_scene   = 0;   // changes seen in a pass with a scene signal
    uint32_t  passes_rebuild = 0;   // changes seen in a pass with an object rebuild
    uint32_t  monotone       = 0;   // longest strictly monotone run (bounded by kRecent)

    bool      seen_zero    = false;
    bool      seen_nonzero = false;
    bool      pointer_like = false;

    MemNumber first, last, min, max;
    uint64_t  first_frame = 0;
    uint64_t  last_frame  = 0;

    double    recent[kRecent] = {0};
    uint8_t   recent_count    = 0;

    // Distinct values seen, over the whole tracking lifetime rather than over
    // the history window.  Saturates at kSeen, which is all the ranking needs:
    // "bounded" and "unbounded churn" are the only two conclusions drawn.
    double    seen_values[kSeen] = {0};
    uint8_t   seen_count = 0;
    bool      seen_saturated = false;

    uint32_t distinct() const { return seen_saturated ? 99u : seen_count; }
};

// ---------------------------------------------------------------------------
// Lead — a ranked, explained candidate for research.
// ---------------------------------------------------------------------------
struct Lead {
    uint64_t    address = 0;
    ValueType   type    = ValueType::Dword;

    // Static half of the identity (mapped == false means no static home at all,
    // in which case the name is session-scoped and says so).
    bool        mapped = false;
    std::string module;
    uint64_t    rva    = 0;
    Identity    identity;
    InstanceTag tag;
    std::string display_name;

    uint32_t    cluster_slots = 1;
    uint64_t    cluster_span  = 0;   // bytes

    double      score    = 0.0;
    Provenance  evidence = Provenance::Observed;

    std::string value_text;   // current value, with a float reading when useful
    std::string why;          // the evidence, in words

    // Raw counters so the ranking can be audited rather than trusted.
    uint32_t    samples = 0, changes = 0, scene_changes = 0, rebuild_changes = 0;
    uint32_t    distinct = 0, monotone = 0;

    bool is_cluster() const { return cluster_slots > 1; }
};

// ---------------------------------------------------------------------------
// UnclaimedExplorer
// ---------------------------------------------------------------------------
class UnclaimedExplorer {
public:
    void configure(const ExploreConfig& c);
    const ExploreConfig& config() const { return m_cfg; }

    // Ranges already identified.  Anything inside is skipped entirely, so
    // "unclaimed" always means exactly that.
    void set_claimed(std::vector<std::pair<uint64_t, uint64_t>> ranges);
    size_t claimed_range_count() const { return m_claimed.size(); }
    bool   is_claimed(uint64_t addr) const;

    void set_region_from(uint64_t begin, uint64_t end);

    struct PassResult {
        size_t   slots_examined = 0;
        size_t   slots_changed  = 0;
        size_t   slots_skipped_claimed = 0;
        size_t   promotions     = 0;
        uint64_t cursor_before  = 0;
        uint64_t cursor_after   = 0;
        bool     wrapped        = false;
        bool     region_valid   = false;
        bool     track_cap_hit  = false;
    };

    // One bounded pass.  `scene_signal` / `rebuild_signal` describe the window
    // this pass covers; they are recorded as an association, not a cause.
    PassResult pass(const uint8_t* mem, uint64_t mem_size, uint64_t frame,
                    bool scene_signal, bool rebuild_signal);

    // Rank the tracked slots.  `modules` / `build_id` give mapped leads their
    // deterministic name; unmapped leads are explicitly session-scoped.
    std::vector<Lead> rank(const ModuleMap& modules, const std::string& build_id,
                           size_t max_leads = 0) const;

    void reset();

    size_t   tracked() const { return m_tracks.size(); }
    uint64_t passes() const { return m_passes; }
    uint64_t promotions() const { return m_promotions; }
    uint64_t slots_examined_total() const { return m_examined; }
    uint64_t wraps() const { return m_wraps; }
    uint64_t region_begin() const { return m_cfg.region_begin; }
    uint64_t region_end() const { return m_cfg.region_end; }

    std::string summary() const;

private:
    struct CacheEntry {
        uint64_t tag   = 0;      // address; only meaningful when valid
        uint64_t bits  = 0;
        bool     valid = false;
    };

    SlotTrack* find_track(uint64_t address);

    ExploreConfig                        m_cfg;
    std::vector<std::pair<uint64_t, uint64_t>> m_claimed;   // sorted, disjoint
    std::vector<CacheEntry>              m_cache;
    std::vector<SlotTrack>               m_tracks;
    std::unordered_map<uint64_t, uint32_t> m_index;
    uint64_t                             m_cursor = 0;
    uint64_t                             m_passes = 0;
    uint64_t                             m_promotions = 0;
    uint64_t                             m_examined = 0;
    uint64_t                             m_wraps = 0;
    bool                                 m_region_started = false;
};

// Render a tracked value the way a researcher wants to read it: decimal, hex,
// and — for a 4-byte slot — the float reading, because a value that looks like
// garbage as an integer is very often a perfectly sensible float.
std::string describe_slot_value(ValueType type, const MemNumber& n);

} // namespace swordfare::research
