// =============================================================================
// unclaimed_explorer.cpp
// =============================================================================

#include "game/research/unclaimed_explorer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace swordfare::research {

namespace {

constexpr uint64_t kMinValidAddr = 0x1000;

size_t width_of(ValueType t) {
    switch (t) {
        case ValueType::Byte:   return 1;
        case ValueType::Word:   return 2;
        case ValueType::Dword:  return 4;
        case ValueType::Qword:  return 8;
        case ValueType::Float:  return 4;
        case ValueType::Double: return 8;
        default:                return 0;
    }
}

bool is_float_type(ValueType t) {
    return t == ValueType::Float || t == ValueType::Double;
}

size_t next_pow2(size_t v) {
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

std::string hex(uint64_t v) {
    char b[32];
    std::snprintf(b, sizeof(b), "0x%llX", static_cast<unsigned long long>(v));
    return b;
}

// The single place that knows how raw bytes become a number for a given type.
MemNumber decode_bits(ValueType type, uint64_t bits) {
    MemNumber n;
    switch (type) {
        case ValueType::Float: {
            float f = 0.0f;
            const uint32_t u = static_cast<uint32_t>(bits);
            std::memcpy(&f, &u, 4);
            n.is_float = true;
            n.f = f;
            break;
        }
        case ValueType::Double: {
            double d = 0.0;
            std::memcpy(&d, &bits, 8);
            n.is_float = true;
            n.f = d;
            break;
        }
        case ValueType::Byte:  n.i = static_cast<int8_t>(static_cast<uint8_t>(bits)); break;
        case ValueType::Word:  n.i = static_cast<int16_t>(static_cast<uint16_t>(bits)); break;
        case ValueType::Qword: n.i = static_cast<int64_t>(bits); break;
        case ValueType::Dword:
        default:               n.i = static_cast<int32_t>(static_cast<uint32_t>(bits)); break;
    }
    return n;
}

// A 4-byte value is "pointer like" when it could plausibly BE an address.
// Addresses are cheap to find and boring to research, so they get penalised —
// penalised, never hidden.
//
// The bar is deliberately high: PAGE alignment, not merely "divisible by 16".
// A loose alignment test flags ordinary integers that happen to be round
// numbers, which would demote exactly the kind of value we are looking for.
bool looks_like_pointer(uint64_t v, uint64_t mem_size) {
    if (v < 0x10000 || v >= mem_size) return false;
    return (v & 0xFFF) == 0;
}

} // namespace

std::string describe_slot_value(ValueType type, const MemNumber& n) {
    char b[160];
    if (is_float_type(type)) {
        std::snprintf(b, sizeof(b), "%g", n.as_double());
        return b;
    }
    const int64_t iv = n.i;
    if (type == ValueType::Dword) {
        const uint32_t u = static_cast<uint32_t>(iv);
        float f = 0.0f;
        std::memcpy(&f, &u, 4);
        // Only show the float reading when it is a sane number; otherwise it is
        // noise that misleads more than it helps.
        if (std::isfinite(f) && std::fabs(f) > 1e-6f && std::fabs(f) < 1e12f) {
            std::snprintf(b, sizeof(b), "%lld  (0x%llX, %g as float)",
                          static_cast<long long>(iv),
                          static_cast<unsigned long long>(u), static_cast<double>(f));
            return b;
        }
        std::snprintf(b, sizeof(b), "%lld  (0x%llX)", static_cast<long long>(iv),
                      static_cast<unsigned long long>(u));
        return b;
    }
    std::snprintf(b, sizeof(b), "%lld  (0x%llX)", static_cast<long long>(iv),
                  static_cast<unsigned long long>(iv));
    return b;
}

// ---------------------------------------------------------------------------
void UnclaimedExplorer::configure(const ExploreConfig& c) {
    m_cfg = c;
    if (m_cfg.cache_slots) m_cfg.cache_slots = next_pow2(m_cfg.cache_slots);
    m_cfg.cache_slots = std::max<size_t>(m_cfg.cache_slots, 64);
    if (m_cfg.alignment == 0) {
        const size_t w = width_of(m_cfg.type);
        m_cfg.alignment = w ? w : 4;
    }
    // Reshaping invalidates the sample cache: stale baselines would read as
    // "changed" on the very next pass.
    m_cache.assign(m_cfg.cache_slots, CacheEntry{});
    m_cursor = m_cfg.region_begin;
    m_region_started = false;
}

void UnclaimedExplorer::set_region_from(uint64_t begin, uint64_t end) {
    if (m_cfg.region_begin == begin && m_cfg.region_end == end) return;
    m_cfg.region_begin = begin;
    m_cfg.region_end   = end;
    m_cursor = begin;
    m_region_started = false;
}

void UnclaimedExplorer::set_claimed(std::vector<std::pair<uint64_t, uint64_t>> ranges) {
    for (auto& r : ranges)
        if (r.second <= r.first) r.second = r.first + 1;
    std::sort(ranges.begin(), ranges.end());

    // Merge overlaps so a lookup is one binary search.
    std::vector<std::pair<uint64_t, uint64_t>> merged;
    merged.reserve(ranges.size());
    for (const auto& r : ranges) {
        if (!merged.empty() && r.first <= merged.back().second)
            merged.back().second = std::max(merged.back().second, r.second);
        else
            merged.push_back(r);
    }
    m_claimed.swap(merged);
}

bool UnclaimedExplorer::is_claimed(uint64_t addr) const {
    if (m_claimed.empty()) return false;
    auto it = std::upper_bound(
        m_claimed.begin(), m_claimed.end(), addr,
        [](uint64_t a, const std::pair<uint64_t, uint64_t>& r) { return a < r.first; });
    if (it == m_claimed.begin()) return false;
    --it;
    return addr >= it->first && addr < it->second;
}

SlotTrack* UnclaimedExplorer::find_track(uint64_t address) {
    auto it = m_index.find(address);
    if (it == m_index.end()) return nullptr;
    return &m_tracks[it->second];
}

// ---------------------------------------------------------------------------
UnclaimedExplorer::PassResult UnclaimedExplorer::pass(const uint8_t* mem, uint64_t mem_size,
                                                      uint64_t frame,
                                                      bool scene_signal,
                                                      bool rebuild_signal) {
    PassResult r;
    if (!mem || mem_size == 0) return r;
    if (m_cache.empty()) m_cache.assign(m_cfg.cache_slots, CacheEntry{});

    const size_t w = width_of(m_cfg.type);
    if (!w) return r;
    const size_t step = m_cfg.alignment ? m_cfg.alignment : w;

    uint64_t begin = m_cfg.region_begin;
    uint64_t end   = m_cfg.region_end ? m_cfg.region_end : mem_size;
    end = std::min<uint64_t>(end, mem_size);
    if (begin < kMinValidAddr) begin = kMinValidAddr;
    if (end <= begin + w) return r;
    r.region_valid = true;

    if (!m_region_started) {
        m_cursor = begin;
        m_region_started = true;
    } else if (m_cursor < begin || m_cursor >= end) {
        // A cursor that has run past the end wraps here.  The loop below cannot
        // see this case (the cursor lands exactly on `end`), so reporting it only
        // inside the loop undercounted one wrap per cycle.
        m_cursor = begin;
        ++m_wraps;
        r.wrapped = true;
    }
    r.cursor_before = m_cursor;

    const uint64_t mask = m_cfg.cache_slots - 1;

    for (size_t i = 0; i < m_cfg.slots_per_pass; ++i) {
        if (m_cursor + w > end) {
            m_cursor = begin;
            ++m_wraps;
            r.wrapped = true;
            break;
        }
        const uint64_t addr = m_cursor;
        m_cursor += step;
        ++r.slots_examined;
        ++m_examined;

        if (is_claimed(addr)) { ++r.slots_skipped_claimed; continue; }

        uint64_t bits = 0;
        std::memcpy(&bits, mem + addr, w);
        if (is_float_type(m_cfg.type) && !std::isfinite(decode_bits(m_cfg.type, bits).as_double()))
            continue;                       // Inf / NaN is not a lead

        CacheEntry& c = m_cache[addr & mask];
        if (!c.valid || c.tag != addr) {
            c.valid = true;                 // first visit: establish the baseline
            c.tag   = addr;
            c.bits  = bits;
            continue;
        }
        if (c.bits == bits) continue;       // unchanged since the last visit

        const uint64_t prev_bits = c.bits;
        c.bits = bits;                      // refresh so the next visit compares fresh
        ++r.slots_changed;

        SlotTrack* t = find_track(addr);
        if (!t) {
            if (m_tracks.size() >= m_cfg.max_tracks) { r.track_cap_hit = true; continue; }
            SlotTrack fresh;
            fresh.address = addr;
            fresh.type    = m_cfg.type;
            // Seed a baseline from the pre-change value so the very first
            // observation has something to compare against.
            fresh.first = fresh.min = fresh.max = fresh.last = decode_bits(m_cfg.type, prev_bits);
            fresh.samples = 1;
            fresh.first_frame = frame;
            fresh.seen_zero    = (fresh.last.as_double() == 0.0);
            fresh.seen_nonzero = !fresh.seen_zero;
            fresh.recent[0] = fresh.last.as_double();
            fresh.recent_count = 1;
            fresh.seen_values[0] = fresh.last.as_double();
            fresh.seen_count = 1;
            m_tracks.push_back(fresh);
            m_index[addr] = static_cast<uint32_t>(m_tracks.size() - 1);
            ++m_promotions;
            ++r.promotions;
            t = &m_tracks.back();
        }

        const MemNumber cur = decode_bits(m_cfg.type, bits);
        ++t->changes;
        if (scene_signal)   ++t->passes_scene;
        if (rebuild_signal) ++t->passes_rebuild;

        const double v = cur.as_double();

        // Recent history, most recent first.
        for (size_t k = std::min<size_t>(t->recent_count, SlotTrack::kRecent - 1); k > 0; --k)
            t->recent[k] = t->recent[k - 1];
        t->recent[0] = v;
        if (t->recent_count < SlotTrack::kRecent) ++t->recent_count;

        // Distinct values over the tracking lifetime, saturating.
        if (!t->seen_saturated) {
            bool known = false;
            for (size_t k = 0; k < t->seen_count; ++k)
                if (t->seen_values[k] == v) { known = true; break; }
            if (!known) {
                if (t->seen_count < SlotTrack::kSeen) t->seen_values[t->seen_count++] = v;
                else                                  t->seen_saturated = true;
            }
        }

        // Longest strictly monotone run over the recent window.
        if (t->recent_count >= 3) {
            bool asc = true, desc = true;
            for (size_t k = 1; k < t->recent_count; ++k) {
                if (!(t->recent[k - 1] > t->recent[k])) asc = false;
                if (!(t->recent[k - 1] < t->recent[k])) desc = false;
            }
            if (asc || desc)
                t->monotone = std::max<uint32_t>(t->monotone,
                                                 static_cast<uint32_t>(t->recent_count));
        }

        t->last = cur;
        t->last_frame = frame;
        ++t->samples;

        if (v == 0.0) t->seen_zero = true; else t->seen_nonzero = true;
        if (!t->last.is_float || t->min.is_float) {
            if (v < t->min.as_double()) t->min = cur;
            if (v > t->max.as_double()) t->max = cur;
        }

        if (m_cfg.type == ValueType::Dword)
            t->pointer_like = looks_like_pointer(static_cast<uint32_t>(bits), mem_size);
    }

    ++m_passes;
    r.cursor_after = m_cursor;
    return r;
}

// ---------------------------------------------------------------------------
// rank
// ---------------------------------------------------------------------------
std::vector<Lead> UnclaimedExplorer::rank(const ModuleMap& modules,
                                          const std::string& build_id,
                                          size_t max_leads) const {
    std::vector<Lead> leads;
    leads.reserve(std::min<size_t>(m_tracks.size(), 4096));

    const size_t limit = max_leads ? max_leads : m_cfg.max_leads;

    for (const auto& t : m_tracks) {
        if (t.samples < m_cfg.min_samples) continue;

        Lead l;
        l.address  = t.address;
        l.type     = t.type;
        l.samples  = t.samples;
        l.changes  = t.changes;
        l.distinct = t.distinct();
        l.monotone = t.monotone;
        l.value_text = describe_slot_value(t.type, t.last);
        l.evidence = Provenance::Observed;

        std::string mod;
        uint64_t    rva = 0;
        StaticRef ref;
        ref.build_id  = build_id;
        ref.type_name = value_type_name(t.type);
        ref.provenance = Provenance::Observed;
        if (modules.rva_of(t.address, &mod, &rva)) {
            // Static home: the identity is deterministic and survives a reboot.
            l.mapped = true;
            l.module = mod;
            l.rva    = rva;
            ref.module        = mod;
            ref.kind          = MemKind::Global;
            ref.container_rva = rva;
            ref.offset        = 0;
        } else {
            // No static home.  There is no honest way to claim this is the same
            // thing after a reboot, so it is named as a session-scoped
            // observation and the UI marks it as such.
            ref.kind   = MemKind::Unknown;
            ref.offset = t.address;
        }
        l.identity     = IdentityResolver::resolve(ref);
        l.display_name = l.identity.base_name;

        // ── Score, with the reasoning kept alongside it ──────────────────
        double score = 0.0;
        std::string why;

        // Every tracked slot has changed at least once by construction (a slot
        // is only ever promoted BECAUSE it moved), so there is no
        // "never changed" case to score here.  Memory that never moves is what
        // the exact-value scanner is for.
        {
            score = 1.0;
            why = "changed " + std::to_string(t.changes) + "x over " +
                  std::to_string(t.samples) + " sample(s)";

            const double scene_assoc =
                static_cast<double>(t.passes_scene) / static_cast<double>(t.changes);
            score += 2.0 * scene_assoc;
            if (t.passes_scene)
                why += "; " + std::to_string(t.passes_scene) +
                       " of those in a pass carrying a scene change (" +
                       std::to_string(static_cast<int>(scene_assoc * 100.0)) +
                       "% association, not causation)";

            const double rebuild_assoc =
                static_cast<double>(t.passes_rebuild) / static_cast<double>(t.changes);
            score += 1.5 * rebuild_assoc;
            if (t.passes_rebuild)
                why += "; " + std::to_string(t.passes_rebuild) +
                       " coincided with an object rebuild";

            const uint32_t d = t.distinct();
            if (d >= 2 && d <= 6) {
                score += 0.5;
                why += "; bounded value set (" + std::to_string(d) + " states)";
            } else if (d == 99) {
                score -= 1.0;
                why += "; near-unbounded churn (timer or noise)";
            }
            if (t.monotone >= 3) {
                score += 0.5;
                why += "; monotone run of " + std::to_string(t.monotone) + " (counter-like)";
            }
            if (t.seen_nonzero && !t.seen_zero) {
                score += 0.4;
                why += "; never zero (always-live value)";
            } else if (t.seen_zero && t.seen_nonzero) {
                score += 0.2;
                why += "; toggles zero/non-zero (spawn or despawn flag?)";
            }
            if (t.pointer_like) {
                score -= 0.6;
                why += "; looks like a pointer (address, not data)";
            }
        }

        // Evidence tier: an association is a lead, never a proof, so this stops
        // at SUSPECTED and never reaches RECOVERED.
        if (t.changes >= 4 && (t.passes_scene > 0 || t.passes_rebuild > 0))
            l.evidence = Provenance::Inferred;

        l.score          = score;
        l.scene_changes  = t.passes_scene;
        l.rebuild_changes = t.passes_rebuild;
        l.why            = why;
        leads.push_back(std::move(l));
    }

    // Cluster adjacent leads: a run of nearby moving slots is far more likely to
    // be one object than any single slot is, so it is boosted and reported with
    // its span.
    std::sort(leads.begin(), leads.end(),
              [](const Lead& a, const Lead& b) { return a.address < b.address; });
    for (size_t i = 0; i < leads.size();) {
        size_t j = i + 1;
        while (j < leads.size() &&
               leads[j].address - leads[j - 1].address <= m_cfg.cluster_gap)
            ++j;
        if (j - i > 1) {
            const uint64_t span = leads[j - 1].address - leads[i].address + width_of(leads[i].type);
            const double bonus = std::min(1.0, 0.15 * static_cast<double>(j - i));
            for (size_t k = i; k < j; ++k) {
                leads[k].cluster_slots = static_cast<uint32_t>(j - i);
                leads[k].cluster_span  = span;
                leads[k].score += bonus;
                leads[k].why += "; part of a " + std::to_string(j - i) +
                                "-slot cluster spanning " + std::to_string(span) +
                                " bytes (probable struct)";
            }
        }
        i = j;
    }

    std::sort(leads.begin(), leads.end(), [](const Lead& a, const Lead& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.address < b.address;
    });
    if (leads.size() > limit) leads.resize(limit);
    return leads;
}

void UnclaimedExplorer::reset() {
    m_cache.assign(m_cfg.cache_slots, CacheEntry{});
    m_tracks.clear();
    m_index.clear();
    m_cursor = m_cfg.region_begin;
    m_region_started = false;
    m_passes = 0;
    m_promotions = 0;
    m_examined = 0;
    m_wraps = 0;
}

std::string UnclaimedExplorer::summary() const {
    char b[288];
    std::snprintf(b, sizeof(b),
                  "region %s..%s  |  %llu pass(es), %llu slot(s) examined, %zu tracked "
                  "(cap %zu), %llu promoted, %llu wrap(s), %zu claimed range(s)",
                  hex(m_cfg.region_begin).c_str(), hex(m_cfg.region_end).c_str(),
                  static_cast<unsigned long long>(m_passes),
                  static_cast<unsigned long long>(m_examined),
                  m_tracks.size(), m_cfg.max_tracks,
                  static_cast<unsigned long long>(m_promotions),
                  static_cast<unsigned long long>(m_wraps), m_claimed.size());
    return b;
}

} // namespace swordfare::research
