// =============================================================================
// vtable_classifier.cpp — Object concrete type classification via vptr
//
// See vtable_classifier.h for the address-space contract (a live vptr is a
// relocated VA while catalog vtable keys are module-relative RVAs) and for the
// two classification strategies (RTTI first, known-vtable table second).
// =============================================================================

#include "game/research/vtable_classifier.h"
#include <cctype>
#include <cstring>
#include <cstdio>

namespace swordfare::research {

namespace {

// Reduce a mangled name to its last identifier token:
//   "N5Caver19GameSceneControllerE" -> "GameSceneController"
//   "N5Caver10SceneObjectE"         -> "SceneObject"
// Returns "" when nothing identifier-like can be extracted.
std::string last_identifier(const std::string& m) {
    std::string last;
    for (size_t i = 0; i < m.size();) {
        if (std::isdigit(static_cast<unsigned char>(m[i]))) {
            size_t j = i;
            unsigned long n = 0;
            while (j < m.size() && std::isdigit(static_cast<unsigned char>(m[j]))) {
                n = n * 10 + static_cast<unsigned long>(m[j] - '0');
                if (n > 4096) break;                 // pathological input guard
                ++j;
            }
            if (n > 0 && n < 256 && j + n <= m.size()) {
                std::string tok = m.substr(j, n);
                if (std::isalpha(static_cast<unsigned char>(tok[0])) || tok[0] == '_')
                    last = tok;
                i = j + n;
                continue;
            }
            i = j;
        } else {
            ++i;
        }
    }
    return last;
}

} // namespace

VtableClassifier& VtableClassifier::instance() {
    static VtableClassifier s_instance;
    return s_instance;
}

VtableClassifier::VtableClassifier() {
    init();
}

void VtableClassifier::init() {
    std::lock_guard<std::mutex> lock(m_mu);
    m_map.clear();
    m_probe_done    = false;
    m_probe_valid   = 0;
    m_probe_checked = 0;
    m_unmatched_rvas.clear();
    m_hits_rtti = m_hits_table = m_misses = 0;

    struct ConfirmedEntry {
        const char* name;
        uint64_t    vptr;
    };
    // Compiled-in seeds.  These are keyed in module-relative RVA space, like the
    // catalog, and they are UNVERIFIED against the binary actually being run —
    // use probe_summary() to see how many of them resolve to a real vtable in
    // guest memory.  Classification does not depend on them (RTTI is tried
    // first), so a stale seed here costs nothing but a missed fallback.
    static const ConfirmedEntry kConfirmed[] = {
        {"GameSceneController", 0x62A5E0},
        {"Scene",               0x6336A0},
        {"SceneObject",         0x6339F8},
        {"HealthComponent",     0x61E920},
        {"EntityComponent",     0x61E1D0},
        {"HeroEntityComponent", 0x61EB40},
        {"MonsterEntityComp",   0x61F050},
    };

    auto& catalog = RecoveryCatalog::instance();

    for (const auto& c : kConfirmed) {
        int cat_id = -1;
        auto st = catalog.find_struct(c.name);
        if (st.has_value()) {
            cat_id = st->id;
        } else if (std::string(c.name) == "MonsterEntityComp") {
            auto alt = catalog.find_struct("MonsterEntityComponent");
            if (alt.has_value()) cat_id = alt->id;
        }
        m_map[c.vptr] = VtableEntry{c.vptr, c.name, cat_id};
    }

    if (catalog.is_ready()) {
        auto structs = catalog.all_structs();
        for (const auto& s : structs) {
            auto vt = catalog.vtable_for_struct(s.id, true);
            // vtable_for_struct() yields vtables.vtable_rva — already the key
            // space used above, so no conversion is needed here.
            if (vt.has_value() && vt->vptr_arm64 != 0) {
                m_map[vt->vptr_arm64] = VtableEntry{vt->vptr_arm64, s.name, s.id};
            }
        }
    }

    m_ready = true;
}

bool VtableClassifier::is_ready() const {
    std::lock_guard<std::mutex> lock(m_mu);
    return m_ready;
}

// ---------------------------------------------------------------------------
// Module window
// ---------------------------------------------------------------------------
void VtableClassifier::set_module_window(uint64_t base, uint64_t end) {
    std::lock_guard<std::mutex> lock(m_mu);
    if (base && end > base) {
        m_module_base = base;
        m_module_end  = end;
        m_probe_done  = false;   // re-probe against the new window
    }
}

uint64_t VtableClassifier::module_base() const {
    std::lock_guard<std::mutex> lock(m_mu);
    return m_module_base;
}

uint64_t VtableClassifier::module_end() const {
    std::lock_guard<std::mutex> lock(m_mu);
    return m_module_end;
}

uint64_t VtableClassifier::rva_of(uint64_t va) const {
    // Catalog keys are RVAs; a live vptr is base+RVA.  Also tolerate input that
    // is already an RVA so both call styles work.
    return (va >= m_module_base && va < m_module_end) ? (va - m_module_base) : va;
}

void VtableClassifier::register_vtable(uint64_t vptr_rva, const std::string& struct_name,
                                       int catalog_id) {
    std::lock_guard<std::mutex> lock(m_mu);
    m_map[vptr_rva] = VtableEntry{vptr_rva, struct_name, catalog_id};
    m_probe_done = false;
}

// ---------------------------------------------------------------------------
// RTTI
// ---------------------------------------------------------------------------
std::string VtableClassifier::rtti_name_at(const uint8_t* mem, uint64_t mem_size,
                                           uint64_t obj_va) const {
    auto rd64 = [&](uint64_t va, uint64_t* out) {
        if (va < 0x10000 || va + 8 > mem_size || va + 8 > 0xE0000000ULL) return false;
        std::memcpy(out, mem + va, 8);
        return true;
    };

    uint64_t vptr = 0;
    if (!rd64(obj_va, &vptr)) return {};

    // Itanium ABI: an object's vptr points at the first virtual function slot,
    // and the type_info* lives in the word immediately before it.
    if (vptr < 0x10000 + 8) return {};
    uint64_t ti = 0;
    if (!rd64(vptr - 8, &ti)) return {};
    if (ti < 0x10000) return {};

    uint64_t name_ptr = 0;
    if (!rd64(ti + 8, &name_ptr)) return {};   // type_info::__type_name
    if (name_ptr < 0x10000 || name_ptr >= 0xE0000000ULL) return {};

    // Bounded, printable read — a bogus pointer almost always fails here.
    std::string raw;
    raw.reserve(64);
    for (uint64_t i = 0; i < 96; ++i) {
        const uint64_t a = name_ptr + i;
        if (a >= mem_size || a >= 0xE0000000ULL) return {};
        const unsigned char c = mem[a];
        if (c == 0) break;
        if (c < 0x20 || c > 0x7e) return {};
        raw.push_back(static_cast<char>(c));
    }
    if (raw.size() < 3) return {};

    std::string id = last_identifier(raw);
    if (!id.empty()) return id;

    // Already-unmangled class name?
    if (!(std::isalpha(static_cast<unsigned char>(raw[0])) || raw[0] == '_')) return {};
    for (char c : raw)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return {};
    return raw;
}

// ---------------------------------------------------------------------------
// Probe — one-time sanity sweep over the known vtable keys
// ---------------------------------------------------------------------------
void VtableClassifier::run_probe_locked(const uint8_t* mem, uint64_t mem_size) const {
    m_probe_done    = true;
    m_probe_valid   = 0;
    m_probe_checked = 0;
    if (!mem) return;

    for (const auto& kv : m_map) {
        ++m_probe_checked;
        for (uint64_t base : {m_module_base, uint64_t(0)}) {
            const uint64_t va = base + kv.first;
            if (va < 0x10000 || va + 8 > mem_size) continue;
            uint64_t slot0 = 0;
            std::memcpy(&slot0, mem + va, 8);
            // A real vtable's first slot is a code pointer in the module image.
            // The first page of a mapped ELF is its header, never a function
            // entry — excluding it keeps a null/zeroed slot (which relocates to
            // exactly module_base) from being scored as a valid vtable.
            if (slot0 >= m_module_base + 0x1000 && slot0 < m_module_end) {
                ++m_probe_valid;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// classify
// ---------------------------------------------------------------------------
ClassificationResult VtableClassifier::classify(uint64_t obj_va,
                                               const uint8_t* guest_memory,
                                               uint64_t mem_size) const
{
    ClassificationResult miss;
    miss.catalog_id = -1;
    miss.confidence = "UNKNOWN";

    if (!guest_memory || obj_va < 0x10000 || obj_va >= 0xE0000000 ||
        (obj_va + 8) > mem_size || (obj_va + 8) > 0xE0000000ULL || (obj_va % 8) != 0)
        return miss;

    uint64_t vptr_va = 0;
    std::memcpy(&vptr_va, guest_memory + obj_va, sizeof(uint64_t));
    miss.vptr_va = vptr_va;
    if (vptr_va < 0x10000 || vptr_va >= 0xE0000000ULL) return miss;

    std::lock_guard<std::mutex> lock(m_mu);
    if (!m_probe_done) run_probe_locked(guest_memory, mem_size);

    const uint64_t vptr_rva = rva_of(vptr_va);
    miss.vptr_rva = vptr_rva;

    auto& catalog = RecoveryCatalog::instance();

    // ── 1. RTTI (primary) ───────────────────────────────────────────────────
    // Independent of every address table, so it still works when the catalog's
    // vtable RVAs are stale for the binary being run.
    std::string rtti = rtti_name_at(guest_memory, mem_size, obj_va);
    if (!rtti.empty()) {
        ClassificationResult r;
        r.matched     = true;
        r.struct_name = rtti;
        r.vptr_va     = vptr_va;
        r.vptr_rva    = vptr_rva;
        r.confidence  = "RTTI";
        auto s = catalog.find_struct(rtti);
        if (s.has_value()) r.catalog_id = s->id;
        ++m_hits_rtti;
        return r;
    }

    // ── 2. Known-vtable table, in RVA space ─────────────────────────────────
    // The observed vptr may point at the vtable symbol or at its first function
    // slot depending on how the linker laid it out, so try the small ABI header
    // deltas rather than assuming one convention.
    for (uint64_t delta : {uint64_t(0), uint64_t(8), uint64_t(16)}) {
        if (vptr_rva < delta) continue;
        auto it = m_map.find(vptr_rva - delta);
        if (it == m_map.end()) continue;
        ClassificationResult r;
        r.matched     = true;
        r.struct_name = it->second.struct_name;
        r.vptr_va     = vptr_va;
        r.vptr_rva    = vptr_rva;
        r.catalog_id  = it->second.catalog_id;
        r.confidence  = "KNOWN";
        ++m_hits_table;
        return r;
    }

    // ── 3. Ask the catalog directly (RVA space) ─────────────────────────────
    auto cat_vt = catalog.vtable_by_vptr64(vptr_rva);
    if (!cat_vt.has_value() && vptr_rva >= 16)
        cat_vt = catalog.vtable_by_vptr64(vptr_rva - 16);
    if (cat_vt.has_value()) {
        ClassificationResult r;
        r.matched    = true;
        r.vptr_va    = vptr_va;
        r.vptr_rva   = vptr_rva;
        r.catalog_id = cat_vt->struct_id;
        r.confidence = "KNOWN";
        auto cs = catalog.find_struct_by_id(cat_vt->struct_id);
        r.struct_name = cs.has_value() ? cs->name : "Unknown";
        ++m_hits_table;
        return r;
    }

    // ── Miss — remember a bounded sample so the caller can reconcile ────────
    ++m_misses;
    if (m_unmatched_rvas.size() < 8) m_unmatched_rvas.push_back(vptr_rva);
    return miss;
}

std::vector<ClassificationResult> VtableClassifier::classify_many(
    const std::vector<uint64_t>& vas,
    const uint8_t* guest_memory, uint64_t mem_size) const
{
    std::vector<ClassificationResult> results;
    results.reserve(vas.size());
    for (uint64_t va : vas) {
        results.push_back(classify(va, guest_memory, mem_size));
    }
    return results;
}

void VtableClassifier::record_observation(uint64_t obj_va, uint64_t vptr_va,
                                          const std::string& guessed_name)
{
    char note[64];
    std::snprintf(note, sizeof(note), "vptr=0x%08llX",
                  static_cast<unsigned long long>(vptr_va));
    RecoveryCatalog::instance().record_runtime_observation(guessed_name, obj_va, note);
}

std::vector<VtableClassifier::VtableEntry> VtableClassifier::known_vtables() const {
    std::lock_guard<std::mutex> lock(m_mu);
    std::vector<VtableEntry> entries;
    entries.reserve(m_map.size());
    for (const auto& kv : m_map) {
        entries.push_back(kv.second);
    }
    return entries;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
std::string VtableClassifier::probe_summary() const {
    std::lock_guard<std::mutex> lock(m_mu);
    char buf[320];
    std::snprintf(buf, sizeof(buf),
                  "vtable keys %d/%d resolve to a real vtable (module 0x%llX-0x%llX); "
                  "hits: RTTI %llu, table %llu, misses %llu",
                  m_probe_valid, m_probe_checked,
                  static_cast<unsigned long long>(m_module_base),
                  static_cast<unsigned long long>(m_module_end),
                  static_cast<unsigned long long>(m_hits_rtti),
                  static_cast<unsigned long long>(m_hits_table),
                  static_cast<unsigned long long>(m_misses));
    std::string out = buf;
    if (!m_probe_done) {
        out += " | probe pending (needs guest memory)";
    } else if (!m_unmatched_rvas.empty()) {
        out += " | unclassified vptr RVAs:";
        for (uint64_t r : m_unmatched_rvas) {
            char t[24];
            std::snprintf(t, sizeof(t), " 0x%llX",
                          static_cast<unsigned long long>(r));
            out += t;
        }
    }
    return out;
}

void VtableClassifier::stats(uint64_t* rtti, uint64_t* table, uint64_t* unknown) const {
    std::lock_guard<std::mutex> lock(m_mu);
    if (rtti)    *rtti    = m_hits_rtti;
    if (table)   *table   = m_hits_table;
    if (unknown) *unknown = m_misses;
}

} // namespace swordfare::research
