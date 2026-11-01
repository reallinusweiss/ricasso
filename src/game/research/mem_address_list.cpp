// =============================================================================
// mem_address_list.cpp
// =============================================================================

#include "game/research/mem_address_list.h"

#include <algorithm>
#include <cstdio>

namespace swordfare::research {

namespace {

size_t width_for(ValueType t, const std::string& text) {
    const size_t w = value_type_size(t);
    if (w) return w;
    if (t == ValueType::Str) return text.size() + 1;
    return 0;   // AoB: determined by the parsed pattern
}

std::string spill_error(const AddressEntry& e, size_t width) {
    char b[192];
    std::snprintf(b, sizeof(b),
                  "refused: %zu-byte write at +0x%llX would spill past the end of "
                  "'%s' (field ends at +0x%llX)",
                  width, static_cast<unsigned long long>(e.runtime_va),
                  e.label().c_str(),
                  static_cast<unsigned long long>(e.field_bound_end));
    return b;
}

} // namespace

// ---------------------------------------------------------------------------
// add / remove
// ---------------------------------------------------------------------------
uint64_t AddressList::add(const StaticRef& ref, uint64_t runtime_va, ValueType type,
                          const std::string& group, uint64_t field_bound_end,
                          const InstanceTag& tag) {
    std::lock_guard<std::mutex> lk(m_mu);

    AddressEntry e;
    e.id              = m_next_id++;
    e.runtime_va      = runtime_va;
    e.type            = type;
    e.identity        = IdentityResolver::resolve(ref);
    e.tag             = tag;
    e.display_name    = IdentityResolver::instance_name(e.identity.base_name, tag);
    e.kind            = ref.kind;
    e.provenance      = ref.provenance;
    e.group           = group;
    e.field_bound_end = field_bound_end;
    m_entries.push_back(std::move(e));
    return m_entries.back().id;
}

bool AddressList::remove(uint64_t id) {
    std::lock_guard<std::mutex> lk(m_mu);
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (it->id == id) { m_entries.erase(it); return true; }
    }
    return false;
}

void AddressList::clear() {
    std::lock_guard<std::mutex> lk(m_mu);
    m_entries.clear();
}

AddressEntry* AddressList::find_locked(uint64_t id) {
    for (auto& e : m_entries) if (e.id == id) return &e;
    return nullptr;
}

const AddressEntry* AddressList::find_locked(uint64_t id) const {
    for (const auto& e : m_entries) if (e.id == id) return &e;
    return nullptr;
}

const AddressEntry* AddressList::find(uint64_t id) const {
    std::lock_guard<std::mutex> lk(m_mu);
    return find_locked(id);
}

size_t AddressList::size() const {
    std::lock_guard<std::mutex> lk(m_mu);
    return m_entries.size();
}

// ---------------------------------------------------------------------------
// identity / movement
// ---------------------------------------------------------------------------
bool AddressList::rebind(uint64_t id, uint64_t new_runtime_va) {
    std::lock_guard<std::mutex> lk(m_mu);
    AddressEntry* e = find_locked(id);
    if (!e) return false;
    // Deliberately the ONLY thing that changes: identity, label, notes, tag and
    // provenance all survive the move.
    e->runtime_va = new_runtime_va;
    e->freeze_conflicts = 0;
    return true;
}

bool AddressList::set_label(uint64_t id, const std::string& label) {
    std::lock_guard<std::mutex> lk(m_mu);
    AddressEntry* e = find_locked(id);
    if (!e) return false;
    e->user_label = label;
    return true;
}

bool AddressList::set_notes(uint64_t id, const std::string& notes) {
    std::lock_guard<std::mutex> lk(m_mu);
    AddressEntry* e = find_locked(id);
    if (!e) return false;
    e->user_notes = notes;
    return true;
}

bool AddressList::set_provenance(uint64_t id, Provenance p) {
    std::lock_guard<std::mutex> lk(m_mu);
    AddressEntry* e = find_locked(id);
    if (!e) return false;
    if (static_cast<unsigned>(p) <= static_cast<unsigned>(e->provenance)) return false;
    e->provenance = p;          // forward-only, mirroring IdentityStore
    return true;
}

// ---------------------------------------------------------------------------
// values
// ---------------------------------------------------------------------------
std::string AddressList::read_value(uint64_t id, const uint8_t* mem, uint64_t mem_size) const {
    std::lock_guard<std::mutex> lk(m_mu);
    const AddressEntry* e = find_locked(id);
    if (!e) return "<no row>";
    // A row set to hex also READS as hex.  Otherwise the base would flip back the
    // moment the operator stopped editing, which is the sort of small dishonesty
    // that makes a tool feel unreliable.
    if (e->domain == ValueDomain::Hex && value_type_is_numeric(e->type) &&
        !value_type_is_float(e->type)) {
        MemNumber n;
        if (mem_read_number(mem, mem_size, e->runtime_va, e->type, &n)) {
            const size_t w = value_type_size(e->type);
            const uint64_t u = w && w < 8
                ? (static_cast<uint64_t>(n.i) & ((1ull << (w * 8)) - 1ull))
                : static_cast<uint64_t>(n.i);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "0x%llX",
                          static_cast<unsigned long long>(u));
            return buf;
        }
    }
    return MemScanner::format_value(mem, mem_size, e->runtime_va, e->type);
}

std::string AddressList::read_editable(uint64_t id, const uint8_t* mem,
                                       uint64_t mem_size, ValueDomain domain) const {
    std::lock_guard<std::mutex> lk(m_mu);
    const AddressEntry* e = find_locked(id);
    if (!e) return {};
    // `domain` is authoritative: a hex/decimal toggle in the UI re-seeds the box
    // without mutating the row, and passing e.domain reads the row's own choice.
    return MemScanner::format_editable(mem, mem_size, e->runtime_va, e->type, domain);
}

bool AddressList::set_domain(uint64_t id, ValueDomain domain) {
    std::lock_guard<std::mutex> lk(m_mu);
    AddressEntry* e = find_locked(id);
    if (!e) return false;
    e->domain = domain;
    return true;
}

// ---------------------------------------------------------------------------
// One definition of "would this write be accepted", used by both the live
// validator and the write itself.  Two separate implementations is how a green
// commit button ends up next to a refused write.
// ---------------------------------------------------------------------------
bool AddressList::check_write_locked(const AddressEntry& e, const std::string& text,
                                     ValueDomain domain, size_t* width,
                                     std::string* err) const {
    size_t w = 0;
    if (e.type == ValueType::AoB) {
        ScanValue v;
        if (!MemScanner::parse_typed_text_ex(ValueType::AoB, text, domain, &v, err))
            return false;
        w = v.bytes.size();
    } else if (e.type == ValueType::Str) {
        w = text.size() + 1;                 // the writer appends a NUL
    } else {
        ScanValue v;
        if (!MemScanner::parse_typed_text_ex(e.type, text, domain, &v, err))
            return false;
        w = width_for(e.type, text);
    }
    if (w == 0) {
        if (err) *err = "unknown width for this type";
        return false;
    }
    if (e.field_bound_end != 0 && e.runtime_va + w > e.field_bound_end) {
        if (err) *err = spill_error(e, w);
        return false;
    }
    if (width) *width = w;
    return true;
}

bool AddressList::validate_value(uint64_t id, const std::string& text,
                                 ValueDomain domain, std::string* err) const {
    std::lock_guard<std::mutex> lk(m_mu);
    const AddressEntry* e = find_locked(id);
    if (!e) { if (err) *err = "no such row"; return false; }
    return check_write_locked(*e, text, domain, nullptr, err);
}

bool AddressList::set_value(uint64_t id, const std::string& text,
                            uint8_t* mem, uint64_t mem_size, std::string* err,
                            ValueDomain domain) {
    if (!mem) { if (err) *err = "no guest memory"; return false; }

    std::lock_guard<std::mutex> lk(m_mu);
    AddressEntry* e = find_locked(id);
    if (!e) { if (err) *err = "no such row"; return false; }

    if (!check_write_locked(*e, text, domain, nullptr, err)) {
        ++m_refused;
        return false;
    }
    if (!mem_write_typed(mem, mem_size, e->runtime_va, e->type, text, err, domain)) {
        ++m_refused;
        return false;
    }
    ++m_writes;
    return true;
}

bool AddressList::set_frozen(uint64_t id, bool on, const uint8_t* mem, uint64_t mem_size,
                             const std::string& text, std::string* err) {
    std::lock_guard<std::mutex> lk(m_mu);
    AddressEntry* e = find_locked(id);
    if (!e) { if (err) *err = "no such row"; return false; }

    if (!on) {
        e->frozen = false;
        e->frozen_bytes.clear();
        return true;
    }

    std::vector<uint8_t> payload;
    if (!text.empty()) {
        if (e->type == ValueType::AoB) {
            ScanValue v;
            if (!MemScanner::parse_typed_text(ValueType::AoB, text, &v)) {
                if (err) *err = "not a byte pattern";
                return false;
            }
            payload = v.bytes;
        } else {
            ScanValue v;
            if (!MemScanner::parse_typed_text(e->type, text, &v)) {
                if (err) *err = "value does not fit " + std::string(value_type_name(e->type));
                return false;
            }
            payload = mem_encode_number(v.num, e->type);
        }
    } else {
        const size_t w = value_type_size(e->type);
        if (!w) { if (err) *err = "freeze needs an explicit value for this type"; return false; }
        if (!mem || !mem_read_bytes(mem, mem_size, e->runtime_va, w, &payload)) {
            if (err) *err = "cannot read the current value";
            return false;
        }
    }

    if (e->field_bound_end != 0 && e->runtime_va + payload.size() > e->field_bound_end) {
        if (err) *err = spill_error(*e, payload.size());
        return false;
    }

    e->frozen_bytes = std::move(payload);
    e->frozen       = true;
    return true;
}

size_t AddressList::apply_freeze(uint8_t* mem, uint64_t mem_size) {
    if (!mem) return 0;
    std::lock_guard<std::mutex> lk(m_mu);
    size_t written = 0;
    for (auto& e : m_entries) {
        if (!e.frozen || e.frozen_bytes.empty()) continue;
        const size_t w = e.frozen_bytes.size();
        if (e.field_bound_end != 0 && e.runtime_va + w > e.field_bound_end) continue;
        if (!mem_write_bytes(mem, mem_size, e.runtime_va, e.frozen_bytes, nullptr)) continue;
        ++e.freeze_writes;
        ++written;
    }
    // A frozen slot the guest keeps changing is worth reporting rather than
    // silently fighting about: count rows whose value differs from the payload.
    for (auto& e : m_entries) {
        if (!e.frozen || e.frozen_bytes.empty()) continue;
        std::vector<uint8_t> cur;
        if (mem_read_bytes(mem, mem_size, e.runtime_va, e.frozen_bytes.size(), &cur) &&
            cur != e.frozen_bytes)
            ++e.freeze_conflicts;
    }
    return written;
}

// ---------------------------------------------------------------------------
// query / categorisation
// ---------------------------------------------------------------------------
std::vector<AddressEntry> AddressList::snapshot(const AddressFilter& f) const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::vector<AddressEntry> out;
    for (const auto& e : m_entries) {
        if (f.unresolved_only && e.is_known())            continue;
        if (f.frozen_only && !e.frozen)                   continue;
        if (f.min_provenance != Provenance::Unknown &&
            static_cast<unsigned>(e.provenance) < static_cast<unsigned>(f.min_provenance))
            continue;
        if (f.kind != MemKind::Unknown && e.kind != f.kind) continue;
        if (f.type != ValueType::All && e.type != f.type)  continue;
        if (f.has_group && e.group != f.group)             continue;
        out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](const AddressEntry& a, const AddressEntry& b) {
        if (a.identity.var_id != b.identity.var_id)
            return a.identity.var_id < b.identity.var_id;
        return a.runtime_va < b.runtime_va;
    });
    return out;
}

std::vector<std::string> AddressList::groups() const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::vector<std::string> gs;
    for (const auto& e : m_entries) {
        if (e.group.empty()) continue;
        if (std::find(gs.begin(), gs.end(), e.group) == gs.end()) gs.push_back(e.group);
    }
    std::sort(gs.begin(), gs.end());
    return gs;
}

std::string AddressList::summary() const {
    std::lock_guard<std::mutex> lk(m_mu);
    size_t known = 0, frozen = 0;
    for (const auto& e : m_entries) {
        if (e.is_known()) ++known;
        if (e.frozen) ++frozen;
    }
    char b[192];
    std::snprintf(b, sizeof(b),
                  "rows %zu | resolved %zu | unresolved %zu | frozen %zu | "
                  "writes %llu | refused %llu",
                  m_entries.size(), known, m_entries.size() - known, frozen,
                  static_cast<unsigned long long>(m_writes),
                  static_cast<unsigned long long>(m_refused));
    return b;
}

} // namespace swordfare::research
