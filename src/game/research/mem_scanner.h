// =============================================================================
// mem_scanner.h — Cheat Engine / GameGuardian-class scan engine over the guest
//                 address space.
//
// This is *not* a cross-process memory scanner.  Swordfare owns the guest, so
// `g_guest_memory` is one flat host buffer and a "first scan" is a single
// linear pass over it — no VirtualQuery region walk, no ReadProcessMemory, no
// page faults.  Everything here is plain pointer arithmetic on that buffer.
//
// Results never carry identity.  A scan hit is (runtime address, previous
// value); the caller resolves identity through mem_identity.h before display,
// so what a researcher sees is "GameState+0x90 (currentHealth)" or
// "VAR_6262_2727", not a raw address.
//
// Design notes
//  * Two-phase protocol, like CE: first_scan() seeds the candidate set,
//    next_scan() narrows it.  Every candidate keeps its previous value so
//    Increased / Decreased / Changed / Unchanged / *By are all pure filters
//    over the same set — no re-scan of the buffer.
//  * Statistics are recorded per candidate so "unchanged" and "increased by"
//    work across an arbitrary number of frames.
//  * Float comparisons use a tolerance because exact float equality across
//    emulated arithmetic is not meaningful.
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// Value types — the CE set, minus 32-bit-only aliases.
// ---------------------------------------------------------------------------
enum class ValueType : uint8_t {
    Byte = 0,   // uint8
    Word,       // uint16
    Dword,      // uint32
    Qword,      // uint64
    Float,      // float32
    Double,     // float64
    Str,        // ASCII/UTF-8 byte string
    AoB,        // array of bytes, with optional wildcards
    All,        // scan every numeric width (first scan only)
};

const char* value_type_name(ValueType t);
size_t      value_type_size(ValueType t);     // 0 = variable width (Str/AoB/All)
bool        value_type_is_float(ValueType t);
bool        value_type_is_numeric(ValueType t);

// ---------------------------------------------------------------------------
// Input domain — which base the researcher is typing in.
//
// This is deliberately separate from ValueType.  "I am editing this Dword" and
// "I am typing in hex" are different questions, and conflating them is what
// produced an edit box that displayed "32 (0x20)" (a rendering) and then failed
// to parse it back.  The domain decides how text is READ; the representation
// decides how it is SHOWN.  Adapted from libmemscan (LGPL-3.0) `value.zig`
// UserValue, where the same split is made explicit via MatchFlags.
// ---------------------------------------------------------------------------
enum class ValueDomain : uint8_t {
    Decimal = 0,   // "32"        — sign allowed
    Hex,           // "20"/"0x20" — read as unsigned, reinterpreted per width
    Auto,          // "0x.."/"0b.."/"0o.." prefixes honoured, else decimal
};

const char* value_domain_name(ValueDomain d);

// ---------------------------------------------------------------------------
// A number in whichever domain its type lives in.  Integer types stay exact
// (int64) so 64-bit values are never lossily round-tripped through a double.
// ---------------------------------------------------------------------------
struct MemNumber {
    bool    is_float = false;
    int64_t i        = 0;
    double  f        = 0.0;

    static MemNumber from_int(int64_t v)          { MemNumber n; n.i = v; return n; }
    static MemNumber from_float(double v)         { MemNumber n; n.is_float = true; n.f = v; return n; }
    double  as_double() const                     { return is_float ? f : static_cast<double>(i); }
};

// ---------------------------------------------------------------------------
// LiteralParse — one parse, many possible widths.
//
// libmemscan's central idea, and the fix for the edit bug: user text is NOT
// immediately forced into one type.  "255" is a valid u8, u16, u32, u64, s16,
// s32 and s64, but not s8.  Committing to a width at parse time throws that
// information away, leaving the caller able to say only "does not fit Dword"
// without saying what WOULD fit.  So a parse records a bitmask of every numeric
// width the text is representable in (`fits`), which drives both the live hex
// counterpart in the edit box and a real explanation when a write is refused.
// ---------------------------------------------------------------------------
struct LiteralParse {
    bool        ok    = false;
    std::string error;                  // human-readable, empty when ok
    MemNumber   num;                    // value as parsed
    bool        negative    = false;    // text carried a sign
    bool        hexadecimal = false;    // parsed with hex semantics
    uint16_t    fits        = 0;        // bitmask over numeric ValueType values
    ValueType   narrowest   = ValueType::Qword;  // smallest numeric type it fits

    bool fits_type(ValueType t) const {
        const unsigned bit = static_cast<unsigned>(t);
        return bit < 16 && (fits & (1u << bit)) != 0;
    }

    // fits_type() plus the float case: any successfully-parsed literal is a
    // valid float, because "100" for a float field is how everyone writes 100.0.
    // The fit MASK stays strictly about integer widths, so it can never claim a
    // value is exactly representable when it is not.
    bool fits_as(ValueType t) const {
        return ok && (fits_type(t) || value_type_is_float(t));
    }

    // "2 Bytes, 4 Bytes, 8 Bytes" — names what WOULD have worked, in the same
    // vocabulary as the type selector, so a refusal is actionable.
    std::string fits_desc() const;
};

// Parse researcher text in a given domain WITHOUT committing to a width.
// Deliberately tolerant of a trailing rendering suffix: a value copied straight
// out of the results table ("32 (0x20)") parses as 32, because that string is
// exactly what the table shows and the operator's instinct is to reuse it.
LiteralParse parse_literal(const std::string& text, ValueDomain domain = ValueDomain::Auto);

// The live counterpart shown beside an edit box: type "32" in decimal and this
// returns "0x20"; type "20" in hex and it returns "32".  Empty when the text
// does not parse, so the UI can show an error instead of a plausible wrong
// answer.
std::string literal_counterpart(const std::string& text, ValueDomain domain, ValueType type);

// ---------------------------------------------------------------------------
// Scan types — the CE set.
// ---------------------------------------------------------------------------
enum class ScanType : uint8_t {
    Exact = 0,      // == target
    UnknownInitial, // seed every aligned slot (no comparison)
    Increased,      // > previous
    IncreasedBy,    // == previous + delta
    Decreased,      // < previous
    DecreasedBy,    // == previous - delta
    Changed,        // != previous
    Unchanged,      // == previous
    BiggerThan,     // > target
    SmallerThan,    // < target
    Between,        // target <= v <= target2
    NotEqual,       // != target
};

const char* scan_type_name(ScanType t);

// ---------------------------------------------------------------------------
// Scan-combination validity
//
// Ported from libmemscan's `scanroutines.validateCombo()` (LGPL-3.0), which
// rejects a (data type, filter, value) triple that cannot mean anything BEFORE
// running it.  The failure it prevents is specific and bad: pick "Increased
// Value" on a first scan and there is no previous value to compare against, so
// the honest answers are "refuse" or "seed everything instead and say so" —
// never "seed everything instead and stay quiet", which is what a scanner that
// only falls back internally ends up doing.
// ---------------------------------------------------------------------------

// True when this filter can be evaluated against a previous value, i.e. it is
// meaningful only as a narrowing pass.
bool scan_type_needs_previous(ScanType t);

// True when this filter takes an upper bound as well as a target.
bool scan_type_needs_second_value(ScanType t);

// Can this (filter, type, pass) actually do something?  `first_scan` means the
// candidate set is being seeded, so there is no previous value yet.
bool scan_type_supported(ScanType t, ValueType type, bool first_scan);

// Human reason for a refusal, or nullptr when it is supported.
const char* scan_type_unsupported_reason(ScanType t, ValueType type, bool first_scan);

// ---------------------------------------------------------------------------
// The value the researcher is looking for.
// ---------------------------------------------------------------------------
struct ScanValue {
    MemNumber num;                       // primary target (Exact/Bigger/Smaller/Between low/delta)
    MemNumber num2;                      // Between: high bound
    std::string text;                    // Str target
    bool        case_sensitive = false;  // Str
    std::vector<uint8_t> bytes;          // AoB pattern
    std::vector<uint8_t> mask;           // AoB mask, 0 = wildcard (empty = all exact)
    double      tolerance = 0.0;         // numeric slack (floats); 0 = type default
};

struct ScanOptions {
    ValueType type        = ValueType::Dword;
    ScanType  scan        = ScanType::Exact;
    uint64_t  range_begin = 0;
    uint64_t  range_end   = 0;        // exclusive; 0 = whole buffer
    size_t    alignment   = 0;        // 0 = natural (type width); else 1/2/4/8
    size_t    max_results  = 0;       // 0 = unlimited
    unsigned  threads      = 0;       // 0 = hardware concurrency
    bool      string_aligned = false; // Str/AoB: require alignment too
};

struct ScanStats {
    uint64_t    bytes_scanned = 0;    // size of the address range that was covered
    size_t      compared      = 0;    // positions actually evaluated
    size_t      candidates    = 0;
    bool        truncated     = false;   // stopped at max_results
    double      elapsed_ms    = 0.0;

    // What the caller asked for versus what could actually run.  A first scan
    // cannot evaluate "Increased" (nothing to compare against), and silently
    // seeding every slot instead is how a filter turns into a no-op that looks
    // like a result.  The UI reads this and says which filter was dropped.
    ScanType    requested       = ScanType::Exact;
    ScanType    applied         = ScanType::Exact;
    bool        filter_ignored  = false;
};

// A surviving address, plus the value it had when it was last looked at.
struct ScanCandidate {
    uint64_t  address   = 0;
    ValueType type      = ValueType::Dword;
    MemNumber previous;                  // value at the previous scan
    double    first_seen_value = 0.0;    // for the observations log
};

// ---------------------------------------------------------------------------
// ScanPass — a saved shot.
//
// A real investigation is a *sequence*: search 100 coins, go gain 50, search 150,
// then filter again after the next change.  The valuable result is rarely the
// final single hit — it is being able to walk back to the shot where the list
// was still short enough to reason about, or to branch off it with a different
// filter.  So every shot can keep the exact candidate set it produced, and any
// earlier shot can be restored and re-filtered from.
//
// A pass stores runtime addresses, so restoring one is meaningful only within
// the session that produced it — which is what the UI says about it, rather than
// implying the set is a permanent finding.
// ---------------------------------------------------------------------------
struct ScanPass {
    size_t      shot    = 0;             // 1-based shot number
    ScanType    scan    = ScanType::Exact;
    ValueType   type    = ValueType::Dword;
    std::string target;                  // the value that was searched for, as typed
    std::string label;                   // display label (scan name when unset)
    size_t      results = 0;             // how many survived this shot
    size_t      from    = 0;             // how many the previous shot had
    bool        has_set = false;         // false when the set was too large to keep
    std::vector<ScanCandidate> candidates;  // the set as it stood after this shot
};

class MemScanner {
public:
    // Seed the candidate set.  For UnknownInitial every aligned slot becomes a
    // candidate and only the previous value is recorded.
    size_t first_scan(const uint8_t* mem, uint64_t mem_size,
                      const ScanValue& value, const ScanOptions& opts,
                      ScanStats* stats = nullptr);

    // Narrow the existing candidate set in place.
    size_t next_scan(const uint8_t* mem, uint64_t mem_size,
                     const ScanValue& value, const ScanOptions& opts,
                     ScanStats* stats = nullptr);

    // Re-read every candidate and refresh its previous value without filtering
    // (used between frames so a later compare has fresh "previous" data).
    void snapshot_values(const uint8_t* mem, uint64_t mem_size, const ScanOptions& opts);

    const std::vector<ScanCandidate>& candidates() const { return m_candidates; }
    size_t candidate_count() const { return m_candidates.size(); }
    void   clear();

    // Replace the candidate set wholesale.  Used by post-filters that reject hits
    // on criteria the scan loop does not know about (e.g. "this hit is inside the
    // image's own symbol table"); doing it here keeps the scan itself a single
    // branch-free pass over the buffer.
    void   replace_candidates(std::vector<ScanCandidate> c) { m_candidates = std::move(c); }
    void   reserve(size_t n) { m_candidates.reserve(n); }

    // ── Saved shots ─────────────────────────────────────────────────────────
    // Record the current candidate set as a restore point.  Returns the shot
    // number assigned (1-based, monotonically increasing across the session).
    size_t save_pass(ScanType scan, ValueType type, const std::string& target,
                     const std::string& label = "");

    const std::vector<ScanPass>& passes() const { return m_passes; }

    // Restore a saved shot as the live candidate set, so it can be re-filtered
    // with a different scan type.  false when the shot does not exist or was too
    // large to keep (`has_set == false`).
    bool restore_pass(size_t shot);
    bool drop_pass(size_t shot);
    void clear_passes() { m_passes.clear(); }

    // Bound how many shots are retained.  The first shot is the widest set and
    // the one a researcher branches from most often, so it is never evicted; the
    // second-oldest goes instead.  A shot that was too large to keep costs no
    // memory, so those are evicted before anything restorable.
    void trim_passes(size_t max_passes, bool keep_first = true);

    // A pass keeps its full candidate set so it can be restored.  A first scan
    // with no result cap can produce millions of candidates, so a pass above this
    // size keeps only its metadata; the UI reports that instead of silently
    // offering an unrestorable shot.
    static constexpr size_t kMaxSavedCandidates = 200000;

    // Current formatted value at a candidate (for the results table).
    static std::string format_value(const uint8_t* mem, uint64_t mem_size,
                                    uint64_t address, ValueType type);

    // Parse a researcher-typed value ("123", "-4.5", "0x1F", "hello").
    // Returns false when the text does not fit the type.
    static bool parse_typed_text(ValueType type, const std::string& text, ScanValue* out);

    // Same, but reports *why* it failed and which widths would have worked.
    // `domain` forces hex/decimal reading; Auto honours 0x/0b/0o prefixes.
    static bool parse_typed_text_ex(ValueType type, const std::string& text,
                                   ValueDomain domain, ScanValue* out,
                                   std::string* error);

    // The bare, editable form of a value: no " (0x..)" suffix, no quotes.  An
    // edit box must be seeded with something that parses back to itself, which
    // is exactly what format_value() is not (it is a *rendering*).
    static std::string format_editable(const uint8_t* mem, uint64_t mem_size,
                                       uint64_t address, ValueType type,
                                       ValueDomain domain = ValueDomain::Decimal);

private:
    std::vector<ScanCandidate> m_candidates;
    std::vector<ScanPass>      m_passes;
    size_t                     m_next_shot = 1;
};

// ---------------------------------------------------------------------------
// Typed accessors — the single place that knows how a ValueType maps to bytes.
// Every read is bounds-checked against (mem, mem_size); every write goes
// through the same validation so nothing can walk off the buffer.
// ---------------------------------------------------------------------------
bool mem_read_number(const uint8_t* mem, uint64_t mem_size, uint64_t addr,
                     ValueType type, MemNumber* out);

bool mem_read_bytes(const uint8_t* mem, uint64_t mem_size, uint64_t addr,
                    size_t width, std::vector<uint8_t>* out);

// Raw byte write with bounds checking only (no field-spill rules — that lives
// in the address list, which knows the owning struct).
bool mem_write_bytes(uint8_t* mem, uint64_t mem_size, uint64_t addr,
                     const std::vector<uint8_t>& bytes, std::string* err);

// Parse + validate + write a typed value.  Bounds-checked; `err` explains any
// rejection, including which widths the text WOULD have fitted.
//
// `domain` is how the text should be read; Auto honours 0x/0b/0o prefixes and
// otherwise reads decimal, which is what a researcher expects when the field
// and the base are chosen independently.
bool mem_write_typed(uint8_t* mem, uint64_t mem_size, uint64_t addr,
                     ValueType type, const std::string& text, std::string* err,
                     ValueDomain domain = ValueDomain::Auto);

// Serialise a number for writing frozen values.
std::vector<uint8_t> mem_encode_number(const MemNumber& n, ValueType type);

} // namespace swordfare::research
