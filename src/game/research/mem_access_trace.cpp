// =============================================================================
// mem_access_trace.cpp
// =============================================================================

#include "game/research/mem_access_trace.h"

#include <algorithm>
#include <cstdio>
#include <unordered_map>

namespace swordfare::research {

namespace {

std::string hex(uint64_t v) {
    char b[32];
    std::snprintf(b, sizeof(b), "0x%llX", static_cast<unsigned long long>(v));
    return b;
}

std::string hex_pad(uint64_t v, int digits) {
    char b[32];
    std::snprintf(b, sizeof(b), "0x%0*llX", digits, static_cast<unsigned long long>(v));
    return b;
}

uint32_t bits(uint32_t insn, int lo, int hi) {
    return (insn >> lo) & ((hi - lo >= 31) ? 0xFFFFFFFFu : ((1u << (hi - lo + 1)) - 1u));
}

int64_t sign_extend(uint64_t v, int bits_count) {
    const uint64_t sign = 1ull << (bits_count - 1);
    if (v & sign) return static_cast<int64_t>(v | (~0ull << bits_count));
    return static_cast<int64_t>(v);
}

// dest register fields by name, for the mnemonic text
const char* gp_reg(uint8_t r) {
    static const char* kNames[32] = {
        "x0","x1","x2","x3","x4","x5","x6","x7",
        "x8","x9","x10","x11","x12","x13","x14","x15",
        "x16","x17","x18","x19","x20","x21","x22","x23",
        "x24","x25","x26","x27","x28","x29","x30","sp/xzr"
    };
    return kNames[r & 31];
}

} // namespace

const char* access_kind_name(AccessKind k) {
    switch (k) {
        case AccessKind::Read:  return "READ";
        case AccessKind::Write: return "WRITE";
        case AccessKind::ReadWrite: return "RMW";
    }
    return "?";
}

uint64_t code_site_id(const std::string& module, uint64_t pc_rva) {
    // FNV-1a 64 over the static code site.  Same build + same instruction ⇒
    // same id on every boot; the identity of a *code* site is as static as the
    // identity of a data site.
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= (v >> (i * 8)) & 0xFF;
            h *= 1099511628211ull;
        }
    };
    for (unsigned char c : module) { h ^= c; h *= 1099511628211ull; }
    mix(pc_rva);
    return h;
}

// ---------------------------------------------------------------------------
// AArch64 decoder
// ---------------------------------------------------------------------------
DecodedAccess decode_arm64_access(uint32_t insn) {
    DecodedAccess d;

    // ── Load/store register, unsigned immediate ────────────────────────────
    //   size(2) 111 0 01 opc(2) imm12 Rn Rt
    if ((insn & 0x3B000000u) == 0x39000000u) {
        const uint32_t size = bits(insn, 30, 31);
        const uint32_t opc  = bits(insn, 22, 23);
        const uint64_t imm  = bits(insn, 10, 21) << size;
        d.base_reg = static_cast<uint8_t>(bits(insn, 5, 9));
        d.data_reg = static_cast<uint8_t>(bits(insn, 0, 4));
        d.offset   = imm;
        d.pair     = false;

        d.width = 1ull << size;

        // opc is not simply "load when odd": for size<3 the values 2 and 3 are
        // the narrowing signed loads (LDRSB/LDRSH/LDRSW), and for size==3,
        // opc==2 is PRFM (a prefetch hint, not a data access) and opc==3 is
        // unallocated.  Getting this wrong classifies a load as a store.
        bool load  = false;
        const char* mnem = nullptr;
        if (size == 0) {
            load = (opc != 0);
            mnem = (opc == 0) ? "STRB" : (opc == 1 ? "LDRB" : (opc == 2 ? "LDRSB(64)" : "LDRSB"));
        } else if (size == 1) {
            load = (opc != 0);
            mnem = (opc == 0) ? "STRH" : (opc == 1 ? "LDRH" : (opc == 2 ? "LDRSH(64)" : "LDRSH"));
        } else if (size == 2) {
            if (opc == 3) return DecodedAccess{};          // unallocated
            load = (opc != 0);
            mnem = (opc == 0) ? "STR" : (opc == 1 ? "LDR" : "LDRSW(64)");
        } else {
            if (opc >= 2) return DecodedAccess{};          // PRFM / unallocated
            load = (opc != 0);
            mnem = load ? "LDR" : "STR";
        }
        d.is_load  = load;
        d.is_store = !load;
        d.mnemonic = mnem;

        // Present as "STR W1, [X0,#0x90]" — the shape a researcher recognises.
        const bool dest_is64 = (size == 3) || (opc == 2);
        const char* dr = dest_is64 ? gp_reg(d.data_reg) : [&] {
            static thread_local char b[8];
            std::snprintf(b, sizeof(b), "w%u", d.data_reg);
            return b;
        }();
        d.mnemonic += std::string(" ") + dr + ", [" + gp_reg(d.base_reg) + ",#" + hex(imm) + "]";
        d.ok = true;
        return d;
    }

    // ── Load/store register, unscaled / pre / post index ───────────────────
    //   111 0 00 0 0 opc(2) 0 imm9 idx(2) Rn Rt
    if ((insn & 0x3B200000u) == 0x38000000u) {
        const uint32_t size = bits(insn, 30, 31);
        const uint32_t opc  = bits(insn, 22, 23);
        const uint32_t idx  = bits(insn, 10, 11);
        if (idx == 2) return DecodedAccess{};      // 10 = unprivileged: ignore
        const int64_t imm = sign_extend(bits(insn, 12, 20), 9);
        d.base_reg = static_cast<uint8_t>(bits(insn, 5, 9));
        d.data_reg = static_cast<uint8_t>(bits(insn, 0, 4));
        d.offset   = static_cast<uint64_t>(imm);
        d.pre_index  = (idx == 3);
        d.post_index = (idx == 1);
        d.width    = 1ull << size;
        if (size == 3 && opc >= 2) return DecodedAccess{};   // PRFM / unallocated
        const bool load = (opc != 0);
        d.is_load = load;
        d.is_store = !load;
        const char* base = "STR";
        if (load) {
            if (size == 0)      base = (opc == 2 || opc == 3) ? "LDRSB" : "LDRB";
            else if (size == 1) base = (opc == 2 || opc == 3) ? "LDRSH" : "LDRH";
            else if (size == 2) base = (opc == 2) ? "LDRSW" : "LDR";
            else                base = "LDR";
        }
        d.mnemonic = std::string(base) +
                     (idx == 0 ? "(unscaled)" : (idx == 1 ? "(post)" : "(pre)")) +
                     " [" + gp_reg(d.base_reg) + ",#" + hex(static_cast<uint64_t>(imm)) + "]";
        d.ok = true;
        return d;
    }

    // ── Load/store pair ────────────────────────────────────────────────────
    //   opc(2) 101 0 0 0 0 L imm7 Rt2 Rn Rt
    if ((insn & 0x3A000000u) == 0x28000000u) {
        const uint32_t opc = bits(insn, 30, 31);
        const uint32_t idx = bits(insn, 23, 24);
        const bool     L   = bits(insn, 22, 22) != 0;
        const int64_t  imm = sign_extend(bits(insn, 15, 21), 7);
        const bool is64 = (opc == 2);
        d.base_reg = static_cast<uint8_t>(bits(insn, 5, 9));
        d.data_reg = static_cast<uint8_t>(bits(insn, 0, 4));
        d.pair     = true;
        d.offset   = static_cast<uint64_t>(imm) << (is64 ? 3 : 2);
        d.width    = (is64 ? 8 : 4) * 2;
        d.pre_index  = (idx == 3);
        d.post_index = (idx == 1);
        if (idx == 2 && !L) return DecodedAccess{};   // STGP: ignore
        d.is_load  = L;
        d.is_store = !L;
        d.mnemonic = std::string(L ? "LDP" : "STP") +
                     (is64 ? "" : "(32)") + " [" + gp_reg(d.base_reg) + ",#" +
                     hex(static_cast<uint64_t>(imm) << (is64 ? 3 : 2)) + "]";
        d.ok = true;
        return d;
    }

    return DecodedAccess{};
}

bool decode_arm64_setup(uint32_t insn, uint8_t* dest_reg, bool* is_move) {
    // ADD/SUB (immediate): sf op S 100010 sh imm12 Rn Rd
    if ((insn & 0x1F000000u) == 0x11000000u) {
        if (dest_reg) *dest_reg = static_cast<uint8_t>(bits(insn, 0, 4));
        if (is_move)  *is_move  = false;
        return true;
    }
    // MOV (register): sf op 01010 00 0 0 0 0 1 1 1 1 1 0 Rm Rn=11111 Rd  (ORR Rd, XZR, Rm)
    if ((insn & 0x7FE0FFE0u) == 0x2A0003E0u || (insn & 0x7FE0FFE0u) == 0xAA0003E0u) {
        if (dest_reg) *dest_reg = static_cast<uint8_t>(bits(insn, 0, 4));
        if (is_move)  *is_move  = true;
        return true;
    }
    // MOVZ: sf opc 100101 hw(2) imm16 Rd
    if ((insn & 0x7F800000u) == 0x52800000u || (insn & 0x7F800000u) == 0xD2800000u) {
        if (dest_reg) *dest_reg = static_cast<uint8_t>(bits(insn, 0, 4));
        if (is_move)  *is_move  = true;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// AccessEvent
// ---------------------------------------------------------------------------
std::string AccessEvent::text() const {
    std::string s = std::string(access_kind_name(kind)) + " " + hex(address) +
                    " (" + std::to_string(width) + "B) by ";
    if (module.empty()) s += hex(guest_pc);
    else                s += module + "+" + hex_pad(pc_rva, 6);
    if (!mnemonic.empty()) s += "  [" + mnemonic + "]";
    if (base_value) s += "  base=" + hex(base_value) + " +" + hex(offset);
    return s;
}

// ---------------------------------------------------------------------------
// AccessTrace
// ---------------------------------------------------------------------------
void AccessTrace::record(uint64_t guest_pc, uint64_t address, uint64_t width,
                         AccessKind kind, uint64_t base_value, uint64_t offset,
                         uint8_t base_reg, uint64_t frame,
                         const std::string& mnemonic) {
    ++m_total_events;
    if (!m_modules || !m_cfg.max_events) { ++m_dropped; return; }

    std::string module;
    uint64_t    pc_rva = 0;
    if (!m_modules->rva_of(guest_pc, &module, &pc_rva)) {
        // An address with no static home carries no identity, so it is not
        // evidence for anything.  Counted, not silently absorbed.
        ++m_unmapped;
        return;
    }

    AccessEvent e;
    e.seq        = ++m_seq;
    e.guest_pc   = guest_pc;
    e.module     = module;
    e.pc_rva     = pc_rva;
    e.kind       = kind;
    e.address    = address;
    e.width      = width;
    e.base_value = base_value;
    e.offset     = offset;
    e.base_reg   = base_reg;
    e.frame      = frame;
    e.site_id    = code_site_id(module, pc_rva);
    e.mnemonic   = mnemonic;

    m_events.push_back(std::move(e));
    while (m_events.size() > m_cfg.max_events) m_events.pop_front();
}

size_t AccessTrace::fold() {
    std::unordered_map<uint64_t, size_t> by_site;
    for (const auto& e : m_events) {
        // A read tells us the offset exists; a write tells us the game *uses*
        // it.  Reads are folded but not counted as equal evidence.
        auto it = by_site.find(e.site_id);
        if (it == by_site.end()) {
            if (m_candidates.size() >= m_cfg.max_candidates) continue;
            FieldCandidate c;
            c.site_id       = e.site_id;
            c.module        = e.module;
            c.pc_rva        = e.pc_rva;
            c.base_value    = e.base_value;
            c.offset        = e.offset;
            c.width         = e.width;
            c.kind          = e.kind;
            c.first_frame   = e.frame;
            c.sample_mnemonic = e.mnemonic;
            by_site[e.site_id] = m_candidates.size();
            m_candidates.push_back(std::move(c));
            it = by_site.find(e.site_id);
        }
        FieldCandidate& c = m_candidates[it->second];
        ++c.hits;
        c.last_frame = e.frame;
        if (e.kind == AccessKind::Write) c.kind = AccessKind::Write;
        if (e.base_value && c.sample_bases.size() < 8 &&
            std::find(c.sample_bases.begin(), c.sample_bases.end(), e.base_value) ==
                c.sample_bases.end()) {
            c.sample_bases.push_back(e.base_value);
        }
        c.distinct_bases = c.sample_bases.size();
    }

    // Promote evidence, but never past Correlated: an instruction is proof that
    // a location is touched, not proof of what the field means.
    for (auto& c : m_candidates) {
        if (c.hits >= m_cfg.hotspot_minimum * 4 && c.distinct_bases >= 3)
            c.evidence = Provenance::Correlated;
        else if (c.hits >= m_cfg.hotspot_minimum)
            c.evidence = Provenance::Inferred;      // "suspected"
        else
            c.evidence = Provenance::Observed;

        char b[192];
        std::snprintf(b, sizeof(b), "%s at %s+%s  x%llu over %zu object(s)",
                      c.kind == AccessKind::Write ? "write" : "access",
                      c.module.c_str(), hex_pad(c.pc_rva, 6).c_str(),
                      static_cast<unsigned long long>(c.hits),
                      c.distinct_bases);
        c.note = b;
    }

    std::sort(m_candidates.begin(), m_candidates.end(),
              [](const FieldCandidate& a, const FieldCandidate& b) {
                  if (a.hits != b.hits) return a.hits > b.hits;
                  return a.site_id < b.site_id;
              });
    return m_candidates.size();
}

void AccessTrace::clear() {
    m_events.clear();
    m_candidates.clear();
    m_seq = 0;
}

const FieldCandidate* AccessTrace::find_site(uint64_t site_id) const {
    for (const auto& c : m_candidates)
        if (c.site_id == site_id) return &c;
    return nullptr;
}

std::string AccessTrace::summary() const {
    char b[256];
    std::snprintf(b, sizeof(b),
                  "%llu access(es) total, %zu buffered, %llu unmapped-dropped, %zu candidate site(s)",
                  static_cast<unsigned long long>(m_total_events), m_events.size(),
                  static_cast<unsigned long long>(m_unmapped), m_candidates.size());
    return b;
}

std::string FieldCandidate::describe() const {
    return note;
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------
bool install_memory_access_hooks(AccessHookSink* sink, std::string* why_not) {
    // DELIBERATELY NOT FAKED.
    //
    // The existing emulator hooks are function-entry PC hooks, not per-
    // instruction memory-access callbacks.  Attaching the trace to those would
    // report every access as happening at the function's entry PC, which would
    // produce confidently wrong field candidates — worse than no data.
    //
    // This must be implemented against Dynarmic's live JIT context and
    // validated on a running build (throughput, and whether the callbacks
    // survive a scene transition).  Until then it reports the truth.
    (void)sink;
    if (why_not) {
        *why_not =
            "not installed: needs a per-instruction memory-access callback from Dynarmic. "
            "The existing hooks fire on function entry only, so attributing accesses to them "
            "would fabricate field offsets. Validate on a live build first.";
    }
    return false;
}

} // namespace swordfare::research
