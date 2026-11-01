// =============================================================================
// mem_access_trace.h — memory access → guest PC → static instruction → struct.
//
// A plain memory scanner can only tell you "the 4 bytes at 0x71A50090 changed".
// Swordfare owns the emulated CPU, so it can be told *which instruction* did it:
//
//     WRITE 0x71A80090  by libswordigo.so+0x2A1840
//     STR W1, [X0,#0x90]                (decoded from the guest code)
//     X0 == 0x71A80000                  (the base register at the time)
//     ⇒ 0x71A80000 is a GameSceneController, and this instruction writes its
//       field at +0x90.
//
// That inference is what promotes a field from OBSERVED to SUSPECTED — never
// straight to CONFIRMED, because an instruction is evidence, not proof.
//
// RUNTIME-VALIDATION NOTE (deliberately not assumed):
//   The function-entry PC hooks that already exist (emulator_dynarmic32/64.cpp)
//   are not per-instruction memory-access callbacks.  `AccessHook` below is the
//   exact interface the emulator must call; whether Dynarmic's current hook
//   granularity can supply it at a useful rate is UNVERIFIED and must be
//   measured on a live build before anything depends on it.
// =============================================================================
#pragma once

#include "game/research/live_object_map.h"
#include "game/research/mem_identity.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace swordfare::research {

enum class AccessKind : uint8_t { Read = 0, Write, ReadWrite };

const char* access_kind_name(AccessKind k);

// ---------------------------------------------------------------------------
// One observed memory access, already decoded to a static location.
// ---------------------------------------------------------------------------
struct AccessEvent {
    uint64_t   seq        = 0;
    uint64_t   guest_pc   = 0;   // runtime PC of the accessing instruction
    std::string module;          // module containing the PC
    uint64_t   pc_rva     = 0;   // static RVA of the instruction (identity!)
    AccessKind kind       = AccessKind::Write;
    uint64_t   address    = 0;   // runtime address touched
    uint64_t   width      = 1;
    uint64_t   base_value = 0;   // value of the base register (X0/X1/...)
    uint64_t   offset     = 0;   // decoded immediate offset in the instruction
    uint8_t    base_reg   = 0;   // 0..30, 31 = SP/XZR
    uint64_t   frame      = 0;
    // Static identity of the *code site*, deterministic across boots.
    uint64_t   site_id    = 0;
    std::string mnemonic;        // "STR", "LDRSW", "STP", ...
    std::string text() const;
};

// ---------------------------------------------------------------------------
// A decoded AArch64 load/store site (the static half of the inference).
// ---------------------------------------------------------------------------
struct DecodedAccess {
    bool     ok        = false;
    bool     is_store  = false;
    bool     is_load   = false;
    uint8_t  base_reg  = 31;   // 31 = SP/XZR
    uint8_t  data_reg  = 0;
    uint64_t offset    = 0;    // byte offset encoded in the instruction
    uint64_t width     = 0;    // bytes transferred (0 = unknown / pair / vector)
    bool     pair      = false;// STP/LDP
    bool     pre_index = false;
    bool     post_index = false;
    bool     register_offset = false;  // [Xn, Xm] — offset not an immediate
    std::string mnemonic;
};

// Decode a single 32-bit AArch64 instruction.  Only the forms that actually
// occur when a compiler touches a struct field are handled (unsigned-offset
// LDR/STR/LDRB/STRB/LDRH/STRH/LDRSW of W/X with scaled immediates, the pre/post
// index variants, STP/LDP, and the base-register setup sequence ADD/SUB/MOV
// immediate + MOVZ).  Anything else decodes as ok == false rather than a guess.
DecodedAccess decode_arm64_access(uint32_t insn);

// True when this instruction is a plausible "setup" for a base register, i.e.
// `add x0, x0, #0x90` / `mov x0, x1`.  Used by the correlator to skip noise.
bool decode_arm64_setup(uint32_t insn, uint8_t* dest_reg, bool* is_move);

// ---------------------------------------------------------------------------
// AccessTrace — a bounded ring buffer of observed accesses plus the inference
// that turns them into candidate struct fields.
// ---------------------------------------------------------------------------
struct FieldCandidate {
    uint64_t    site_id    = 0;    // deterministic id of the code site
    std::string module;            // module being written into
    uint64_t    pc_rva     = 0;
    uint64_t    base_value = 0;    // the object the write went into
    uint64_t    offset     = 0;    // field offset implied by the instruction
    uint64_t    width      = 0;
    AccessKind  kind       = AccessKind::Write;
    uint64_t    hits       = 0;
    uint64_t    first_frame = 0;
    uint64_t    last_frame  = 0;
    uint64_t    distinct_bases = 0;  // how many different objects used this site
    std::vector<uint64_t> sample_bases;  // bounded evidence: which objects
    std::string sample_mnemonic;         // "STR W1, [X0,#0x90]"
    // Inferred provenance for a field at (base_type, offset).  An instruction
    // is evidence, so this tops out at Suspected.
    Provenance  evidence = Provenance::Observed;
    std::string note;               // "STR W1,[X0,#0x90] ×412 over 3 objects"
    std::string describe() const;
};

struct TraceConfig {
    size_t   max_events      = 4096;   // ring capacity
    size_t   max_candidates  = 512;
    uint64_t hotspot_minimum = 4;      // hits before a site becomes a candidate
};

class AccessTrace {
public:
    void configure(const TraceConfig& c) { m_cfg = c; }
    const TraceConfig& config() const { return m_cfg; }

    void set_modules(const ModuleMap* m) { m_modules = m; }

    // Record one access.  `guest_pc` is normalised to a module RVA immediately;
    // if it does not fall inside a known module the event is counted as
    // unmapped and dropped (an address-only observation carries no identity).
    void record(uint64_t guest_pc, uint64_t address, uint64_t width,
                AccessKind kind, uint64_t base_value, uint64_t offset,
                uint8_t base_reg, uint64_t frame, const std::string& mnemonic = "");

    // Fold the buffered events into candidate fields.  Called from the UI/tick,
    // not per event, so the hot path stays a push.
    size_t fold();

    void clear();

    const std::deque<AccessEvent>& events() const { return m_events; }
    const std::vector<FieldCandidate>& candidates() const { return m_candidates; }

    uint64_t total_events() const { return m_total_events; }
    uint64_t dropped_events() const { return m_dropped; }
    uint64_t unmapped_events() const { return m_unmapped; }

    // Look up the best candidate for a (code site, offset) pair — used by the
    // UI to explain "why do you think this is a field?".
    const FieldCandidate* find_site(uint64_t site_id) const;

    std::string summary() const;

private:
    TraceConfig                m_cfg;
    const ModuleMap*           m_modules = nullptr;
    std::deque<AccessEvent>    m_events;
    std::vector<FieldCandidate> m_candidates;
    uint64_t                   m_seq = 0;
    uint64_t                   m_total_events = 0;
    uint64_t                   m_dropped = 0;
    uint64_t                   m_unmapped = 0;
};

// Deterministic identity of a code site: module + PC RVA.  Two boots of the
// same build produce the same site id, which is what lets a candidate field be
// matched back to its instruction after a restart.
uint64_t code_site_id(const std::string& module, uint64_t pc_rva);

// ---------------------------------------------------------------------------
// Hooks — the interface the emulator must call to feed the trace.  Kept as a
// plain struct so the emulator can install it without pulling in the research
// headers beyond this one.
//
// `install_memory_access_hooks()` is intentionally NOT implemented against
// Dynarmic here: it needs the live JIT context and has to be validated at
// runtime.  It returns false and explains itself until that wiring exists.
// ---------------------------------------------------------------------------
struct AccessHookSink {
    AccessTrace* trace = nullptr;
    ModuleMap*   modules = nullptr;
    uint64_t     frame = 0;
    bool         enabled = true;
};

bool install_memory_access_hooks(AccessHookSink* sink, std::string* why_not);

} // namespace swordfare::research
