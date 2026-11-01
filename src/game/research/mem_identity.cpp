// =============================================================================
// mem_identity.cpp — Deterministic memory identity for the live inspector
// See mem_identity.h for the contract.
// =============================================================================

#include "game/research/mem_identity.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace swordfare::research {

namespace {

// FNV-1a 64 — dependency-free, stable forever, and good enough for identity.
// (This is an identifier, not a security primitive; what matters is that it
// never changes and is reproducible on every machine and every boot.)
uint64_t fnv1a64(const std::string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= static_cast<uint64_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

// splitmix64 finalizer — used to spread a session seed across tag values.
uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

std::string hex64(uint64_t v) {
    char b[32];
    std::snprintf(b, sizeof(b), "%llX", static_cast<unsigned long long>(v));
    return b;
}

// Tags and VAR handles are 4-digit handles so they stay scannable by eye.
uint32_t make_handle(uint64_t h) { return static_cast<uint32_t>(h % 10000ull); }

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else           { cur.push_back(c); }
    }
    out.push_back(cur);
    return out;
}

std::string sanitize_field(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// enums
// ---------------------------------------------------------------------------
const char* mem_kind_name(MemKind k) {
    switch (k) {
        case MemKind::Global:       return "global";
        case MemKind::StackLocal:   return "stack";
        case MemKind::StructField:  return "field";
        case MemKind::HeapInstance: return "heap";
        case MemKind::VtablePtr:    return "vtable";
        case MemKind::String:       return "string";
        case MemKind::ArrayElement: return "element";
        case MemKind::Unknown:      break;
    }
    return "unknown";
}

const char* provenance_name(Provenance p) {
    switch (p) {
        case Provenance::Observed:   return "OBSERVED";
        case Provenance::Inferred:   return "SUSPECTED";
        case Provenance::Correlated: return "CORRELATED";
        case Provenance::Recovered:  return "RECOVERED";
        case Provenance::Confirmed:  return "CONFIRMED";
        case Provenance::Unknown:    break;
    }
    return "UNKNOWN";
}

// Short UI markers, kept visually distinct so "inferred" can never be mistaken
// for "recovered" at a glance:  ?  observed   ~  suspected   =  correlated
// OK recovered   *  confirmed
const char* provenance_badge(Provenance p) {
    switch (p) {
        case Provenance::Observed:   return "?";
        case Provenance::Inferred:   return "~";
        case Provenance::Correlated: return "=";
        case Provenance::Recovered:  return "OK";
        case Provenance::Confirmed:  return "*";
        case Provenance::Unknown:    break;
    }
    return "??";
}

bool provenance_is_proven(Provenance p) {
    return p == Provenance::Recovered || p == Provenance::Confirmed;
}

const char* InstanceTag::source_name() const {
    switch (source) {
        case TagSource::Provenance: return "provenance";
        case TagSource::Session:    return "session";
        case TagSource::None:       break;
    }
    return "none";
}

std::string InstanceTag::suffix_str() const {
    if (source == TagSource::None || value == 0) return {};
    char b[16];
    std::snprintf(b, sizeof(b), "%u", value);
    return b;
}

// ---------------------------------------------------------------------------
// StaticRef
// ---------------------------------------------------------------------------
std::string StaticRef::canonical() const {
    // NOTE: recovered_name is deliberately NOT part of the key.  Discovering (or
    // renaming) something must never change its identity.
    std::string s;
    s.reserve(96);
    s += build_id.empty() ? "unknown-build" : build_id;
    s += '|';
    s += module.empty() ? "unknown-module" : module;
    s += '|';
    s += mem_kind_name(kind);
    s += '|';
    s += struct_name;
    s += '|';
    s += hex64(container_rva);
    s += '|';
    s += hex64(offset);
    s += '|';
    s += type_name;
    return s;
}

// ---------------------------------------------------------------------------
// IdentityResolver
// ---------------------------------------------------------------------------
uint32_t IdentityResolver::short_handle(uint64_t var_id) { return make_handle(var_id); }

Identity IdentityResolver::resolve(const StaticRef& ref) {
    Identity id;
    id.canonical = ref.canonical();
    id.var_id    = fnv1a64(id.canonical);
    id.provenance = ref.provenance;
    id.deterministic = true;

    const uint32_t h = short_handle(id.var_id);
    char buf[96];

    // A recovered/confirmed offset uses its recovered name as the base name.
    if (provenance_is_proven(ref.provenance) && !ref.recovered_name.empty()) {
        id.base_name = ref.recovered_name;
        return id;
    }

    switch (ref.kind) {
        case MemKind::StackLocal:
            std::snprintf(buf, sizeof(buf), "sub_%08llX_VAR_%04u",
                          static_cast<unsigned long long>(ref.container_rva), h);
            id.base_name = buf;
            break;
        case MemKind::Global:
            std::snprintf(buf, sizeof(buf), "g_VAR_%04u", h);
            id.base_name = buf;
            break;
        case MemKind::HeapInstance:
            if (!ref.struct_name.empty()) {
                std::snprintf(buf, sizeof(buf), "%s_VAR_%04u",
                              ref.struct_name.c_str(), h);
                id.base_name = buf;
            } else {
                std::snprintf(buf, sizeof(buf), "VAR_%04u", h);
                id.base_name = buf;
            }
            break;
        case MemKind::VtablePtr:
            std::snprintf(buf, sizeof(buf), "vtbl_VAR_%04u", h);
            id.base_name = buf;
            break;
        case MemKind::String:
            std::snprintf(buf, sizeof(buf), "str_VAR_%04u", h);
            id.base_name = buf;
            break;
        case MemKind::ArrayElement:
            std::snprintf(buf, sizeof(buf), "arr_VAR_%04u", h);
            id.base_name = buf;
            break;
        case MemKind::StructField:
        case MemKind::Unknown:
        default:
            std::snprintf(buf, sizeof(buf), "VAR_%04u", h);
            id.base_name = buf;
            break;
    }
    return id;
}

std::string IdentityResolver::instance_name(const std::string& base_name,
                                            const InstanceTag& tag) {
    const std::string s = tag.suffix_str();
    if (s.empty()) return base_name;
    return base_name + "_" + s;
}

// ---------------------------------------------------------------------------
// Tier C naming — a slot with no static anchor
//
// This is the *only* place a name is allowed to depend on a runtime address, and
// it is allowed for exactly one reason: for a stack/heap/JIT slot there is no
// static thing for the name to be derived from, so claiming boot-to-boot
// continuity would be a lie.  The name is therefore:
//
//   * prefixed ("heap_") so it can never be mistaken for a static site,
//   * derived from the address, so it is at least stable *within* a session
//     (and reproducible when the guest allocator lays out the same way again),
//   * never stored as if it were a deterministic identity.
//
// The moment anything static is established for the slot (a module RVA, a
// recovered struct field, an object relationship) the slot moves up to tier A
// or B and gets a real, permanent name.
// ---------------------------------------------------------------------------
std::string session_scoped_site_name(uint64_t runtime_va) {
    char b[48];
    std::snprintf(b, sizeof(b), "heap_VAR_%04u",
                  IdentityResolver::stable_tag_value(std::to_string(runtime_va)) % 10000u);
    return b;
}

uint32_t IdentityResolver::stable_tag_value(const std::string& provenance_key) {
    if (provenance_key.empty()) return 0;
    return static_cast<uint32_t>(fnv1a64("tag|" + provenance_key) % 9000ull) + 1000u;
}

// ---------------------------------------------------------------------------
// LiveMemoryIndex
// ---------------------------------------------------------------------------
InstanceTag LiveMemoryIndex::tag_for_locked(uint64_t runtime_va,
                                            const std::string& provenance_key) {
    InstanceTag tag;
    if (!provenance_key.empty()) {
        tag.value  = IdentityResolver::stable_tag_value(provenance_key);
        tag.source = TagSource::Provenance;
        return tag;
    }

    auto it = m_session_tag_by_va.find(runtime_va);
    if (it != m_session_tag_by_va.end()) {
        tag.value  = it->second;
        tag.source = TagSource::Session;
        return tag;
    }

    // No stable anchor: derive a per-session tag that is stable for this
    // session (so the researcher can keep working) but explicitly labelled as
    // session-scoped, because it will differ on the next boot.
    uint32_t v = static_cast<uint32_t>(splitmix64(m_session_seed ^ runtime_va) % 9000ull) + 1000u;
    for (int probe = 0; probe < 4096; ++probe) {
        bool taken = false;
        for (const auto& kv : m_session_tag_by_va)
            if (kv.second == v) { taken = true; break; }
        if (!taken) break;
        v = (v == 9999u) ? 1000u : v + 1u;
    }
    m_session_tag_by_va[runtime_va] = v;
    ++m_next_tag;

    tag.value  = v;
    tag.source = TagSource::Session;
    return tag;
}

std::string LiveMemoryIndex::bind(uint64_t runtime_va, const StaticRef& ref,
                                  uint64_t frame, const std::string& provenance_key) {
    std::lock_guard<std::mutex> lk(m_mu);

    Identity id = IdentityResolver::resolve(ref);
    InstanceTag tag = tag_for_locked(runtime_va, provenance_key);

    auto it = m_by_va.find(runtime_va);
    if (it != m_by_va.end() && it->second.identity.var_id == id.var_id) {
        it->second.tag             = tag;
        it->second.provenance_key  = provenance_key;
        it->second.display_name    = IdentityResolver::instance_name(id.base_name, tag);
        it->second.last_seen_frame = frame;
        it->second.stale           = false;
        ++m_rebinds;
        return it->second.display_name;
    }

    LiveMemoryEntry e;
    e.runtime_va        = runtime_va;
    e.identity          = id;
    e.tag               = tag;
    e.provenance_key    = provenance_key;
    e.display_name      = IdentityResolver::instance_name(id.base_name, tag);
    e.first_seen_frame  = frame;
    e.last_seen_frame   = frame;
    e.stale             = false;
    m_by_va[runtime_va] = e;
    ++m_binds;
    return e.display_name;
}

void LiveMemoryIndex::touch(uint64_t runtime_va, uint64_t frame) {
    std::lock_guard<std::mutex> lk(m_mu);
    auto it = m_by_va.find(runtime_va);
    if (it == m_by_va.end()) return;
    it->second.last_seen_frame = frame;
    it->second.stale           = false;
}

size_t LiveMemoryIndex::age_out(uint64_t frame, uint64_t max_age_frames) {
    std::lock_guard<std::mutex> lk(m_mu);
    size_t newly = 0;
    for (auto& kv : m_by_va) {
        if (kv.second.stale) continue;
        if (frame > kv.second.last_seen_frame &&
            (frame - kv.second.last_seen_frame) > max_age_frames) {
            kv.second.stale = true;
            ++newly;
        }
    }
    return newly;
}

size_t LiveMemoryIndex::prune_stale() {
    std::lock_guard<std::mutex> lk(m_mu);
    size_t removed = 0;
    for (auto it = m_by_va.begin(); it != m_by_va.end();) {
        if (it->second.stale) { it = m_by_va.erase(it); ++removed; }
        else                  { ++it; }
    }
    return removed;
}

const LiveMemoryEntry* LiveMemoryIndex::find_by_va(uint64_t runtime_va) const {
    std::lock_guard<std::mutex> lk(m_mu);
    auto it = m_by_va.find(runtime_va);
    return it == m_by_va.end() ? nullptr : &it->second;
}

std::vector<LiveMemoryEntry> LiveMemoryIndex::all_instances_of(uint64_t var_id) const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::vector<LiveMemoryEntry> out;
    for (const auto& kv : m_by_va)
        if (kv.second.identity.var_id == var_id) out.push_back(kv.second);
    std::sort(out.begin(), out.end(), [](const LiveMemoryEntry& a, const LiveMemoryEntry& b) {
        return a.runtime_va < b.runtime_va;
    });
    return out;
}

size_t LiveMemoryIndex::instance_count_of(uint64_t var_id) const {
    std::lock_guard<std::mutex> lk(m_mu);
    size_t n = 0;
    for (const auto& kv : m_by_va)
        if (kv.second.identity.var_id == var_id) ++n;
    return n;
}

std::vector<LiveMemoryEntry> LiveMemoryIndex::snapshot() const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::vector<LiveMemoryEntry> out;
    out.reserve(m_by_va.size());
    for (const auto& kv : m_by_va) out.push_back(kv.second);
    std::sort(out.begin(), out.end(), [](const LiveMemoryEntry& a, const LiveMemoryEntry& b) {
        if (a.identity.var_id != b.identity.var_id) return a.identity.var_id < b.identity.var_id;
        return a.runtime_va < b.runtime_va;
    });
    return out;
}

size_t LiveMemoryIndex::size() const {
    std::lock_guard<std::mutex> lk(m_mu);
    return m_by_va.size();
}

void LiveMemoryIndex::reset_session(uint64_t session_seed) {
    std::lock_guard<std::mutex> lk(m_mu);
    m_session_tag_by_va.clear();
    m_session_seed = session_seed;
    m_next_tag     = 0;
}

uint64_t LiveMemoryIndex::session_seed() const {
    std::lock_guard<std::mutex> lk(m_mu);
    return m_session_seed;
}

std::string LiveMemoryIndex::summary() const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::unordered_map<uint64_t, size_t> per_identity;
    for (const auto& kv : m_by_va) ++per_identity[kv.second.identity.var_id % 10000ull];
    size_t stale = 0;
    for (const auto& kv : m_by_va) if (kv.second.stale) ++stale;

    char b[256];
    std::snprintf(b, sizeof(b),
                  "live %zu | identities %zu | binds %llu | rebinds %llu | stale %zu | seed 0x%llX",
                  m_by_va.size(), per_identity.size(),
                  static_cast<unsigned long long>(m_binds),
                  static_cast<unsigned long long>(m_rebinds),
                  stale, static_cast<unsigned long long>(m_session_seed));
    return b;
}

// ---------------------------------------------------------------------------
// IdentityStore — persistence
// ---------------------------------------------------------------------------
bool IdentityStore::load(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_mu);
    std::ifstream in(path);
    if (!in) return false;
    m_records.clear();

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        auto f = split_tabs(line);
        if (f.size() < 5) continue;              // tolerate truncated lines
        IdentityRecord r;
        r.var_id       = std::strtoull(f[0].c_str(), nullptr, 16);
        r.provenance   = static_cast<Provenance>(std::strtoul(f[1].c_str(), nullptr, 10));
        r.first_seen_boot = std::strtoull(f[2].c_str(), nullptr, 10);
        r.base_name    = f[3];
        r.canonical    = f[4];
        r.user_name    = f.size() > 5 ? f[5] : std::string();
        if (r.var_id) m_records[r.var_id] = std::move(r);
    }
    return true;
}

bool IdentityStore::save(const std::string& path) const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << "# swordfare identity store v1\n";
    out << "# var_id(hex)\tprovenance\tfirst_seen_boot\tbase_name\tcanonical\tuser_name\n";

    // Sorted by var_id so the file is deterministic and diff-friendly.
    std::vector<const IdentityRecord*> recs;
    recs.reserve(m_records.size());
    for (const auto& kv : m_records) recs.push_back(&kv.second);
    std::sort(recs.begin(), recs.end(),
              [](const IdentityRecord* a, const IdentityRecord* b) {
                  return a->var_id < b->var_id;
              });

    for (const IdentityRecord* r : recs) {
        out << hex64(r->var_id) << '\t'
            << static_cast<unsigned>(r->provenance) << '\t'
            << r->first_seen_boot << '\t'
            << sanitize_field(r->base_name) << '\t'
            << sanitize_field(r->canonical) << '\t'
            << sanitize_field(r->user_name) << '\n';
    }
    return out.good();
}

void IdentityStore::remember(const Identity& id, uint64_t boot_index) {
    if (!id.var_id) return;
    std::lock_guard<std::mutex> lk(m_mu);
    auto it = m_records.find(id.var_id);
    if (it == m_records.end()) {
        IdentityRecord r;
        r.var_id          = id.var_id;
        r.base_name       = id.base_name;
        r.canonical       = id.canonical;
        r.provenance      = id.provenance;
        r.first_seen_boot = boot_index;
        m_records.emplace(r.var_id, std::move(r));
        return;
    }
    // Existing identity: keep the researcher's name, only ever promote
    // provenance, and refresh the base name if it improved.
    if (provenance_is_proven(id.provenance) && !provenance_is_proven(it->second.provenance))
        it->second.provenance = id.provenance;
    if (provenance_is_proven(id.provenance) && !id.base_name.empty())
        it->second.base_name = id.base_name;
    if (it->second.canonical.empty()) it->second.canonical = id.canonical;
}

bool IdentityStore::rename(uint64_t var_id, const std::string& user_name) {
    std::lock_guard<std::mutex> lk(m_mu);
    auto it = m_records.find(var_id);
    if (it == m_records.end()) return false;
    it->second.user_name = user_name;
    return true;
}

bool IdentityStore::promote(uint64_t var_id, Provenance p) {
    std::lock_guard<std::mutex> lk(m_mu);
    auto it = m_records.find(var_id);
    if (it == m_records.end()) return false;
    if (static_cast<unsigned>(p) <= static_cast<unsigned>(it->second.provenance)) return false;
    it->second.provenance = p;
    return true;
}

std::optional<IdentityRecord> IdentityStore::find(uint64_t var_id) const {
    std::lock_guard<std::mutex> lk(m_mu);
    auto it = m_records.find(var_id);
    if (it == m_records.end()) return std::nullopt;
    return it->second;
}

std::vector<IdentityRecord> IdentityStore::all() const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::vector<IdentityRecord> out;
    out.reserve(m_records.size());
    for (const auto& kv : m_records) out.push_back(kv.second);
    std::sort(out.begin(), out.end(),
              [](const IdentityRecord& a, const IdentityRecord& b) {
                  return a.var_id < b.var_id;
              });
    return out;
}

size_t IdentityStore::size() const {
    std::lock_guard<std::mutex> lk(m_mu);
    return m_records.size();
}

std::string IdentityStore::effective_base_name(uint64_t var_id) const {
    std::lock_guard<std::mutex> lk(m_mu);
    auto it = m_records.find(var_id);
    if (it == m_records.end()) return {};
    return it->second.user_name.empty() ? it->second.base_name : it->second.user_name;
}

} // namespace swordfare::research
