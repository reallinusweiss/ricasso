// =============================================================================
// mem_scanner.cpp — scan engine over the flat guest buffer
// =============================================================================

#include "game/research/mem_scanner.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace swordfare::research {

namespace {

constexpr uint64_t kMinValidAddr = 0x1000;   // skip the null page / ELF header

bool in_range(uint64_t addr, size_t width, uint64_t mem_size) {
    return addr >= kMinValidAddr && width > 0 &&
           addr <= mem_size && width <= mem_size &&
           (addr + width) <= mem_size;
}

unsigned effective_threads(unsigned requested) {
    if (requested) return requested;
    unsigned hw = std::thread::hardware_concurrency();
    return hw ? hw : 1u;
}

double tol_for(ValueType t, double requested) {
    if (requested > 0.0) return requested;
    if (t == ValueType::Float)  return 1e-5;
    if (t == ValueType::Double) return 1e-9;
    return 0.0;
}

bool num_eq(const MemNumber& a, const MemNumber& b, double tol) {
    if (a.is_float || b.is_float) {
        const double d = std::fabs(a.as_double() - b.as_double());
        if (tol > 0.0) return d <= tol;
        return d <= 1e-9 * (1.0 + std::fabs(a.as_double()));
    }
    return a.i == b.i;
}

bool num_lt(const MemNumber& a, const MemNumber& b) {
    if (a.is_float || b.is_float) return a.as_double() < b.as_double();
    return a.i < b.i;
}

MemNumber num_sub(const MemNumber& a, const MemNumber& b) {
    if (a.is_float || b.is_float)
        return MemNumber::from_float(a.as_double() - b.as_double());
    return MemNumber::from_int(a.i - b.i);
}

// Does `cur` satisfy `scan` relative to `prev` and the target value?
bool matches(ScanType scan, const MemNumber& cur, const MemNumber& prev,
             const ScanValue& v, double tol) {
    switch (scan) {
        case ScanType::Exact:       return num_eq(cur, v.num, tol);
        case ScanType::NotEqual:    return !num_eq(cur, v.num, tol);
        case ScanType::Increased:   return num_lt(prev, cur);
        case ScanType::Decreased:   return num_lt(cur, prev);
        case ScanType::Changed:     return !num_eq(cur, prev, tol);
        case ScanType::Unchanged:   return num_eq(cur, prev, tol);
        case ScanType::IncreasedBy: return num_eq(num_sub(cur, prev), v.num, tol);
        case ScanType::DecreasedBy: return num_eq(num_sub(prev, cur), v.num, tol);
        case ScanType::BiggerThan:  return num_lt(v.num, cur);
        case ScanType::SmallerThan: return num_lt(cur, v.num);
        case ScanType::Between:
            return !num_lt(cur, v.num) && !num_lt(v.num2, cur);
        case ScanType::UnknownInitial:
            return true;   // no filter: only meaningful as a seeding scan
    }
    return false;
}

// A first scan can only seed from these; the rest need a previous value.
bool scan_seeds(ScanType s) {
    switch (s) {
        case ScanType::Exact:
        case ScanType::NotEqual:
        case ScanType::BiggerThan:
        case ScanType::SmallerThan:
        case ScanType::Between:
        case ScanType::UnknownInitial:
            return true;
        default:
            return false;
    }
}

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

std::vector<ValueType> scan_all_types() {
    return {ValueType::Byte, ValueType::Word, ValueType::Dword,
            ValueType::Qword, ValueType::Float, ValueType::Double};
}

} // namespace

// ---------------------------------------------------------------------------
// value types
// ---------------------------------------------------------------------------
const char* value_type_name(ValueType t) {
    switch (t) {
        case ValueType::Byte:   return "Byte";
        case ValueType::Word:   return "2 Bytes";
        case ValueType::Dword:  return "4 Bytes";
        case ValueType::Qword:  return "8 Bytes";
        case ValueType::Float:  return "Float";
        case ValueType::Double: return "Double";
        case ValueType::Str:    return "String";
        case ValueType::AoB:    return "Array of Bytes";
        case ValueType::All:    return "All";
    }
    return "?";
}

size_t value_type_size(ValueType t) { return width_of(t); }

bool value_type_is_float(ValueType t) {
    return t == ValueType::Float || t == ValueType::Double;
}

bool value_type_is_numeric(ValueType t) {
    return width_of(t) != 0;
}

const char* scan_type_name(ScanType t) {
    switch (t) {
        case ScanType::Exact:          return "Exact Value";
        case ScanType::UnknownInitial: return "Unknown Initial Value";
        case ScanType::Increased:      return "Increased Value";
        case ScanType::IncreasedBy:    return "Increased By";
        case ScanType::Decreased:      return "Decreased Value";
        case ScanType::DecreasedBy:    return "Decreased By";
        case ScanType::Changed:        return "Changed Value";
        case ScanType::Unchanged:      return "Unchanged Value";
        case ScanType::BiggerThan:     return "Bigger Than";
        case ScanType::SmallerThan:    return "Smaller Than";
        case ScanType::Between:        return "Value Between";
        case ScanType::NotEqual:       return "Not Equal To";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// scan-combination validity (libmemscan scanroutines.validateCombo)
// ---------------------------------------------------------------------------
bool scan_type_needs_previous(ScanType t) {
    switch (t) {
        case ScanType::Exact:
        case ScanType::UnknownInitial:
        case ScanType::BiggerThan:
        case ScanType::SmallerThan:
        case ScanType::Between:
        case ScanType::NotEqual:
            return false;   // decidable from the buffer alone
        default:
            return true;    // Increased / Decreased / Changed / Unchanged / *By
    }
}

bool scan_type_needs_second_value(ScanType t) {
    return t == ScanType::Between;
}

bool scan_type_supported(ScanType t, ValueType type, bool first_scan) {
    // A variable-length target has no meaningful ordering or arithmetic, so it
    // supports equality only — same restriction libmemscan applies to BYTEARRAY
    // and STRING.
    if (type == ValueType::Str || type == ValueType::AoB)
        return t == ScanType::Exact;

    // "All" is a seeding mode: it asks the scanner to try every numeric width.
    // A narrowed candidate already knows its width, so the mode is meaningless
    // on a second pass and would silently mean "whatever this one already is".
    if (type == ValueType::All && !first_scan) return false;

    if (first_scan) return scan_seeds(t);
    return true;
}

const char* scan_type_unsupported_reason(ScanType t, ValueType type, bool first_scan) {
    if (type == ValueType::Str || type == ValueType::AoB) {
        if (t != ScanType::Exact)
            return "text and byte patterns can only be matched for equality \u2014 "
                   "'increased' and 'changed' have no meaning for variable-length data";
        return nullptr;
    }
    if (type == ValueType::All && !first_scan)
        return "'All' only applies to a first scan: a narrowed hit already has a "
               "known width, so there is nothing left to try";
    if (first_scan && !scan_seeds(t))
        return "this filter compares against a previous value, and a first scan has "
               "none \u2014 run a first scan (Exact or Unknown Initial), then narrow it";
    return nullptr;
}

// ---------------------------------------------------------------------------
// typed accessors
// ---------------------------------------------------------------------------
bool mem_read_bytes(const uint8_t* mem, uint64_t mem_size, uint64_t addr,
                    size_t width, std::vector<uint8_t>* out) {
    if (!mem || !out || !in_range(addr, width, mem_size)) return false;
    out->assign(mem + addr, mem + addr + width);
    return true;
}

bool mem_read_number(const uint8_t* mem, uint64_t mem_size, uint64_t addr,
                     ValueType type, MemNumber* out) {
    if (!mem || !out) return false;
    const size_t w = width_of(type);
    if (!in_range(addr, w, mem_size)) return false;

    const uint8_t* p = mem + addr;
    switch (type) {
        case ValueType::Byte:   *out = MemNumber::from_int(*p); break;
        case ValueType::Word: {
            uint16_t v;  std::memcpy(&v, p, 2); *out = MemNumber::from_int(v); break;
        }
        case ValueType::Dword: {
            uint32_t v;  std::memcpy(&v, p, 4); *out = MemNumber::from_int(v); break;
        }
        case ValueType::Qword: {
            uint64_t v;  std::memcpy(&v, p, 8);
            *out = MemNumber::from_int(static_cast<int64_t>(v)); break;
        }
        case ValueType::Float: {
            float v;     std::memcpy(&v, p, 4);
            if (std::isnan(v) || std::isinf(v)) return false;
            *out = MemNumber::from_float(v); break;
        }
        case ValueType::Double: {
            double v;    std::memcpy(&v, p, 8);
            if (std::isnan(v) || std::isinf(v)) return false;
            *out = MemNumber::from_float(v); break;
        }
        default:
            return false;
    }
    return true;
}

bool mem_write_bytes(uint8_t* mem, uint64_t mem_size, uint64_t addr,
                     const std::vector<uint8_t>& bytes, std::string* err) {
    if (!mem) { if (err) *err = "no guest memory"; return false; }
    if (bytes.empty()) { if (err) *err = "empty write"; return false; }
    if (!in_range(addr, bytes.size(), mem_size)) {
        if (err) *err = "write out of bounds";
        return false;
    }
    std::memcpy(mem + addr, bytes.data(), bytes.size());
    return true;
}

std::vector<uint8_t> mem_encode_number(const MemNumber& n, ValueType type) {
    std::vector<uint8_t> out;
    const size_t w = width_of(type);
    out.resize(w ? w : 4, 0);
    switch (type) {
        case ValueType::Byte:   { uint8_t  v = static_cast<uint8_t>(n.as_double());  std::memcpy(out.data(), &v, 1); break; }
        case ValueType::Word:   { uint16_t v = static_cast<uint16_t>(n.as_double()); std::memcpy(out.data(), &v, 2); break; }
        case ValueType::Dword:  { uint32_t v = static_cast<uint32_t>(n.as_double()); std::memcpy(out.data(), &v, 4); break; }
        case ValueType::Qword:  { uint64_t v = static_cast<uint64_t>(n.as_double()); std::memcpy(out.data(), &v, 8); break; }
        case ValueType::Float:  { float  v = static_cast<float>(n.as_double());      std::memcpy(out.data(), &v, 4); break; }
        case ValueType::Double: { double v = n.as_double();                          std::memcpy(out.data(), &v, 8); break; }
        default: break;
    }
    return out;
}

// ---------------------------------------------------------------------------
// text parsing / formatting
// ---------------------------------------------------------------------------
const char* value_domain_name(ValueDomain d) {
    switch (d) {
        case ValueDomain::Decimal: return "decimal";
        case ValueDomain::Hex:     return "hex";
        case ValueDomain::Auto:    return "auto";
    }
    return "?";
}

namespace {

std::string trim_ws(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return {};
    const size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

// Strip a rendering suffix.  The results table shows integers as "32 (0x20)"
// because that is the most useful thing to *read*; the same string is what an
// operator will paste back into an edit box, so accepting it is the difference
// between a working write and a silent no-op.  Only a trailing " (0x...)" is
// removed, and only when it is well-formed, so genuinely malformed text still
// fails loudly instead of being quietly truncated.
std::string strip_render_suffix(const std::string& t) {
    const size_t open = t.rfind(" (0x");
    if (open == std::string::npos || t.empty() || t.back() != ')') return t;
    for (size_t i = open + 4; i + 1 < t.size(); ++i) {
        const char c = t[i];
        const bool is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                            (c >= 'A' && c <= 'F');
        if (!is_hex) return t;
    }
    if (t.size() < open + 5) return t;          // " (0x)" has no digits
    return trim_ws(t.substr(0, open));
}

// One wide-unsigned accumulator so widths are never lossily routed through a
// double.  `magnitude` is an unsigned magnitude plus a sign, which is what the
// per-width fit test below actually needs.
struct ScannedLiteral {
    bool     ok        = false;
    bool     negative  = false;
    uint64_t magnitude = 0;
    bool     is_float  = false;
    double   f         = 0.0;
    bool     hexadecimal = false;
};

ScannedLiteral scan_literal_text(const std::string& text, ValueDomain domain) {
    ScannedLiteral r;
    const std::string t = trim_ws(strip_render_suffix(text));
    if (t.empty()) return r;

    char* end = nullptr;
    errno = 0;

    // A leading sign is only meaningful for decimal/auto reading.  In an
    // explicit hex field, "-1" would be ambiguous with a huge unsigned value, so
    // it is refused rather than guessed at.
    const bool neg = (t[0] == '-') || (t[0] == '+');
    if (neg && domain == ValueDomain::Hex) return r;

    bool hex = (domain == ValueDomain::Hex);
    if (domain == ValueDomain::Auto && t.size() > 2 && t[0] == '0' &&
        (t[1] == 'x' || t[1] == 'X'))
        hex = true;

    if (hex) {
        std::string digits = t;
        if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
            digits = digits.substr(2);
        if (digits.empty()) return r;
        errno = 0;
        const unsigned long long u = std::strtoull(digits.c_str(), &end, 16);
        if (!end || *end != '\0' || errno == ERANGE) return r;
        r.ok = true;
        r.magnitude = static_cast<uint64_t>(u);
        r.hexadecimal = true;
        return r;
    }

    // Binary/octal prefixes are honoured in Auto mode, because a researcher
    // reading a disassembly writes "0b1010" and "0o17" as readily as "0x1F".
    if (domain == ValueDomain::Auto && t.size() > 2 && t[0] == '0' &&
        (t[1] == 'b' || t[1] == 'B' || t[1] == 'o' || t[1] == 'O')) {
        const int base = (t[1] == 'b' || t[1] == 'B') ? 2 : 8;
        const std::string digits = t.substr(2);
        if (digits.empty()) return r;
        errno = 0;
        const unsigned long long u = std::strtoull(digits.c_str(), &end, base);
        if (!end || *end != '\0' || errno == ERANGE) return r;
        r.ok = true;
        r.magnitude = static_cast<uint64_t>(u);
        return r;
    }

    // Decimal.  A fractional part or exponent means this is a float literal and
    // must NOT be silently truncated to an integer: writing 12 when the operator
    // typed 12.5 is a wrong answer stated confidently.  (This is a deliberate
    // divergence from libmemscan, whose parseFloatAndBackfillInts does backfill
    // integer candidates — correct for a *scan*, unsafe for a *write*.)
    const bool looks_float = t.find('.') != std::string::npos ||
                             t.find('e') != std::string::npos ||
                             t.find('E') != std::string::npos;
    if (looks_float) {
        const double d = std::strtod(t.c_str(), &end);
        if (!end || *end != '\0' || errno == ERANGE) return r;
        r.ok = true;
        r.is_float = true;
        r.f = d;
        return r;
    }

    const long long s = std::strtoll(t.c_str(), &end, 10);
    if (!end || *end != '\0' || errno == ERANGE) return r;
    r.ok = true;
    r.negative = (s < 0);
    r.magnitude = r.negative ? static_cast<uint64_t>(-(s + 1)) + 1ull
                             : static_cast<uint64_t>(s);
    return r;
}

// Which numeric widths can hold this exact value, losslessly.
//
// A negative value is allowed in an unsigned field as two's complement when it
// fits the same width's signed range, because that is what actually happens on
// the wire: -1 stored into a byte is 0xFF and reading it back as unsigned gives
// 255.  Refusing it would make "write -1 to freeze this flag" impossible.
void fill_fit_mask(uint16_t* fits, ValueType* narrowest,
                   bool negative, uint64_t magnitude) {
    struct Width { ValueType type; unsigned bits; };
    const Width kWidths[] = {
        { ValueType::Byte,  8  },
        { ValueType::Word,  16 },
        { ValueType::Dword, 32 },
        { ValueType::Qword, 64 },
    };

    // Largest magnitude this width can hold, treating the field as unsigned when
    // the value is non-negative and as two's complement when it is negative.
    auto capacity = [](unsigned bits, bool negative) -> uint64_t {
        const unsigned used = negative ? bits - 1 : bits;
        return used >= 64 ? 0xFFFFFFFFFFFFFFFFull : ((1ull << used) - 1ull);
    };

    uint16_t mask = 0;
    for (const Width& w : kWidths) {
        if (magnitude > capacity(w.bits, negative)) continue;
        mask |= static_cast<uint16_t>(1u << static_cast<unsigned>(w.type));
        if (!(mask & (mask - 1))) *narrowest = w.type;   // first (smallest) set bit
    }
    *fits = mask;
}

} // namespace

std::string LiteralParse::fits_desc() const {
    if (!fits) return "no numeric width";
    std::string s;
    for (ValueType t : { ValueType::Byte, ValueType::Word, ValueType::Dword, ValueType::Qword }) {
        if (!fits_type(t)) continue;
        if (!s.empty()) s += ", ";
        s += value_type_name(t);
    }
    return s.empty() ? std::string("no numeric width") : s;
}

LiteralParse parse_literal(const std::string& text, ValueDomain domain) {
    LiteralParse out;
    const ScannedLiteral s = scan_literal_text(text, domain);
    if (!s.ok) {
        out.error = trim_ws(strip_render_suffix(text)).empty()
                        ? std::string("type a value")
                        : ("not a number: '" + trim_ws(strip_render_suffix(text)) + "'");
        return out;
    }

    out.ok          = true;
    out.negative    = s.negative;
    out.hexadecimal = s.hexadecimal;

    if (s.is_float) {
        out.num = MemNumber::from_float(s.f);
        out.fits = static_cast<uint16_t>((1u << static_cast<unsigned>(ValueType::Float)) |
                                         (1u << static_cast<unsigned>(ValueType::Double)));
        out.narrowest = ValueType::Float;
        return out;
    }

    // Reinterpret the magnitude at full width so a hex literal like
    // 0xFFFFFFFFFFFFFFFF becomes the legitimate all-ones value, not an overflow.
    const uint64_t bits = s.negative ? (~s.magnitude + 1ull) : s.magnitude;
    out.num = MemNumber::from_int(static_cast<int64_t>(bits));
    fill_fit_mask(&out.fits, &out.narrowest, s.negative, s.magnitude);
    return out;
}

std::string literal_counterpart(const std::string& text, ValueDomain domain, ValueType type) {
    const LiteralParse p = parse_literal(text, domain);
    if (!p.ok) return {};
    char buf[64];
    if (value_type_is_float(type)) {
        if (domain == ValueDomain::Hex) return {};      // no hex form for a float
        std::snprintf(buf, sizeof(buf), "%g", p.num.as_double());
        return buf;
    }
    const size_t w = width_of(type);
    const uint64_t u = w && w < 8
        ? (static_cast<uint64_t>(p.num.i) & ((1ull << (w * 8)) - 1ull))
        : static_cast<uint64_t>(p.num.i);
    if (domain == ValueDomain::Hex) {
        // Typing in hex, so the counterpart is the plain decimal reading.
        std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(u));
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(u));
    return buf;
}

// ---------------------------------------------------------------------------
// AoB / Str text is parsed separately from numbers: an AoB pattern is a *shape*
// with wildcards, not a value, so it has no counterpart rendering and no fit
// mask.
// ---------------------------------------------------------------------------
static bool parse_aob_text(const std::string& trimmed, ScanValue* out) {
    out->bytes.clear();
    out->mask.clear();
    auto hexv = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    size_t i = 0;
    while (i < trimmed.size()) {
        while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == ',')) ++i;
        if (i >= trimmed.size()) break;
        if (trimmed[i] == '?') {                    // wildcard, one or two chars
            out->bytes.push_back(0);
            out->mask.push_back(0);
            ++i;
            if (i < trimmed.size() && trimmed[i] == '?') ++i;
            continue;
        }
        if (i + 1 >= trimmed.size()) return false;
        const int hi = hexv(trimmed[i]), lo = hexv(trimmed[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out->bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
        out->mask.push_back(0xFF);
        i += 2;
    }
    return !out->bytes.empty();
}

bool MemScanner::parse_typed_text_ex(ValueType type, const std::string& text,
                                     ValueDomain domain, ScanValue* out,
                                     std::string* error) {
    if (!out) return false;
    const std::string t = trim_ws(text);

    if (type == ValueType::Str) {
        if (t.empty()) { if (error) *error = "type some text"; return false; }
        out->text = text;
        return true;
    }
    if (type == ValueType::AoB) {
        if (!parse_aob_text(t, out)) {
            if (error) *error = "not a byte pattern (try: 4F ?? A0)";
            return false;
        }
        return true;
    }

    const LiteralParse p = parse_literal(t, domain);
    if (!p.ok) {
        if (error) *error = p.error;
        return false;
    }
    if (type == ValueType::All) {
        // "All" is a scan-only pseudo-type: it means "every numeric width", so
        // the literal must fit at least one of them.
        if (!p.fits) {
            if (error) *error = "value is not representable in any numeric width";
            return false;
        }
        out->num = p.num;
        return true;
    }
    if (value_type_is_float(type)) {
        // "100" is a float literal here even though it parsed as an integer, and
        // refusing it would be absurd.  What must NOT happen is the reverse:
        // 12.5 quietly becoming the integer 12, which is why the integer branch
        // below range-checks against the width instead of truncating.
        if (!p.fits_as(type)) {
            if (error) {
                *error = std::string("'") + trim_ws(text) + "' is not a number for " +
                         value_type_name(type);
            }
            return false;
        }
        out->num = MemNumber::from_float(p.num.as_double());
        return true;
    }
    if (!p.fits_type(type)) {
        if (error) {
            // Name what WOULD have worked: "does not fit Byte" is only half an
            // answer, and half an answer is what made the old edit box useless.
            *error = std::string("'") + trim_ws(text) + "' does not fit " +
                     value_type_name(type) + " (fits " + p.fits_desc() + ")";
        }
        return false;
    }
    out->num = p.num;
    return true;
}

bool MemScanner::parse_typed_text(ValueType type, const std::string& text, ScanValue* out) {
    return parse_typed_text_ex(type, text, ValueDomain::Auto, out, nullptr);
}

std::string MemScanner::format_editable(const uint8_t* mem, uint64_t mem_size,
                                        uint64_t address, ValueType type,
                                        ValueDomain domain) {
    if (type == ValueType::Str) {
        if (!mem || address >= mem_size) return {};
        std::string s;
        for (uint64_t i = address; i < mem_size; ++i) {
            const unsigned char c = mem[i];
            if (c == 0) break;
            s.push_back(static_cast<char>(c));
        }
        return s;
    }
    if (type == ValueType::AoB) {
        std::vector<uint8_t> bytes;
        if (!mem_read_bytes(mem, mem_size, address, 8, &bytes)) return {};
        char buf[64];
        std::string s;
        for (size_t i = 0; i < bytes.size(); ++i) {
            std::snprintf(buf, sizeof(buf), i ? " %02X" : "%02X", bytes[i]);
            s += buf;
        }
        return s;
    }

    MemNumber n;
    if (!mem_read_number(mem, mem_size, address, type, &n)) return {};

    char buf[64];
    if (value_type_is_float(type)) {
        std::snprintf(buf, sizeof(buf), "%g", n.as_double());
        return buf;
    }
    const size_t w = width_of(type);
    const uint64_t u = w && w < 8
        ? (static_cast<uint64_t>(n.i) & ((1ull << (w * 8)) - 1ull))
        : static_cast<uint64_t>(n.i);
    if (domain == ValueDomain::Hex) {
        // Masked to the field width, so a Byte whose bit 7 is set seeds "0x8C"
        // rather than a sign-extended "0xFFFFFFFFFFFFFF8C".
        std::snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(u));
        return buf;
    }
    // Signed decimal, matching format_value()'s rendering so the edit box and the
    // read-only cell never disagree about what the value is.
    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n.i));
    return buf;
}

std::string MemScanner::format_value(const uint8_t* mem, uint64_t mem_size,
                                     uint64_t address, ValueType type) {
    char buf[128];
    if (type == ValueType::Str) {
        if (!mem || address >= mem_size) return "<oob>";
        std::string s;
        for (uint64_t i = address; i < mem_size && s.size() < 24; ++i) {
            const unsigned char c = mem[i];
            if (c == 0) break;
            if (c < 0x20 || c > 0x7e) { s += '.'; continue; }
            s.push_back(static_cast<char>(c));
        }
        return "\"" + s + "\"";
    }
    if (type == ValueType::AoB) {
        std::vector<uint8_t> bytes;
        if (!mem_read_bytes(mem, mem_size, address, 8, &bytes)) return "<oob>";
        std::string s;
        for (size_t i = 0; i < bytes.size(); ++i) {
            std::snprintf(buf, sizeof(buf), i ? " %02X" : "%02X", bytes[i]);
            s += buf;
        }
        return s;
    }
    MemNumber n;
    if (!mem_read_number(mem, mem_size, address, type, &n)) return "<oob>";
    if (value_type_is_float(type)) {
        std::snprintf(buf, sizeof(buf), "%g", n.as_double());
        return buf;
    }
    const uint64_t u = static_cast<uint64_t>(n.i);
    std::snprintf(buf, sizeof(buf), "%lld (0x%llX)", static_cast<long long>(n.i),
                  static_cast<unsigned long long>(u));
    return buf;
}

// ---------------------------------------------------------------------------
// MemScanner
// ---------------------------------------------------------------------------
void MemScanner::clear() {
    m_candidates.clear();
    m_passes.clear();
    m_next_shot = 1;
}

// ---------------------------------------------------------------------------
// Saved shots
// ---------------------------------------------------------------------------
// NOTE: a new first_scan() deliberately does NOT discard the saved shots.  The
// GameGuardian-style workflow is "save this search, go change the value in the
// game, search again" — the earlier shots are the whole point, so they are kept
// until the researcher explicitly resets or drops one.
size_t MemScanner::save_pass(ScanType scan, ValueType type, const std::string& target,
                             const std::string& label) {
    ScanPass p;
    p.shot    = m_next_shot++;
    p.scan    = scan;
    p.type    = type;
    p.target  = target;
    p.label   = label.empty() ? std::string(scan_type_name(scan)) : label;
    p.results = m_candidates.size();
    p.from    = m_passes.empty() ? m_candidates.size() : m_passes.back().results;
    if (m_candidates.size() <= kMaxSavedCandidates) {
        p.candidates = m_candidates;
        p.has_set    = true;
    }
    m_passes.push_back(std::move(p));
    return m_passes.back().shot;
}

bool MemScanner::restore_pass(size_t shot) {
    for (const ScanPass& p : m_passes) {
        if (p.shot != shot) continue;
        if (!p.has_set) return false;   // too large to have been kept
        m_candidates = p.candidates;
        return true;
    }
    return false;
}

bool MemScanner::drop_pass(size_t shot) {
    for (auto it = m_passes.begin(); it != m_passes.end(); ++it) {
        if (it->shot != shot) continue;
        m_passes.erase(it);
        return true;
    }
    return false;
}

void MemScanner::trim_passes(size_t max_passes, bool keep_first) {
    if (max_passes == 0) return;
    while (m_passes.size() > max_passes) {
        // Pick the eviction victim: an unrestorable shot first (it holds only
        // metadata), then the second-oldest.  Pass 1 is never chosen while
        // `keep_first` is set, because it is the widest baseline set.
        size_t victim = SIZE_MAX;
        for (size_t i = 0; i < m_passes.size(); ++i) {
            if (!m_passes[i].has_set) { victim = i; break; }
        }
        if (victim == SIZE_MAX) {
            victim = m_passes.size() > 1 ? 1 : 0;
            if (keep_first && victim == 0 && m_passes.size() > 1) victim = 1;
        }
        m_passes.erase(m_passes.begin() + static_cast<std::ptrdiff_t>(victim));
    }
}

size_t MemScanner::first_scan(const uint8_t* mem, uint64_t mem_size,
                              const ScanValue& value, const ScanOptions& opts,
                              ScanStats* stats) {
    const auto t0 = std::chrono::steady_clock::now();

    // Record what was asked for versus what can run, before any early return, so
    // the caller always learns that its filter was dropped.  Falling back to a
    // seed silently is how a filter becomes an invisible no-op.
    if (stats) {
        *stats = ScanStats{};
        const ScanType seed = scan_seeds(opts.scan) ? opts.scan : ScanType::UnknownInitial;
        stats->requested      = opts.scan;
        stats->applied        = seed;
        stats->filter_ignored = (seed != opts.scan);
    }

    m_candidates.clear();

    if (!mem || mem_size == 0) return 0;

    const uint64_t begin = std::max<uint64_t>(opts.range_begin, kMinValidAddr);
    const uint64_t end   = (opts.range_end && opts.range_end <= mem_size)
                               ? opts.range_end : mem_size;
    if (end <= begin) return 0;

    // alignment == 0 means "natural": scan on the type's own width, which is
    // what a memory editor does by default.  An explicit 1 scans every byte
    // (and therefore also matches values that straddle a neighbouring slot).
    const size_t align = opts.alignment;
    const size_t cap   = opts.max_results;
    bool truncated     = false;
    uint64_t compared  = 0;

    // A first scan can only seed from these types; anything else has no
    // previous value to compare against, so it seeds like UnknownInitial.
    const ScanType seed_scan = scan_seeds(opts.scan) ? opts.scan : ScanType::UnknownInitial;

    std::vector<ValueType> types;
    if (opts.type == ValueType::All) types = scan_all_types();
    else                             types.push_back(opts.type);

    // ── String scan ─────────────────────────────────────────────────────────
    if (opts.type == ValueType::Str) {
        const std::string& needle = value.text;
        if (needle.empty()) return 0;
        const size_t n = needle.size();
        const size_t sstep = opts.string_aligned ? std::max<size_t>(align, 1) : 1;
        for (uint64_t a = begin; a + n <= end; a += sstep) {
            ++compared;
            bool ok = true;
            for (size_t i = 0; i < n; ++i) {
                unsigned char c = mem[a + i];
                unsigned char d = static_cast<unsigned char>(needle[i]);
                if (!value.case_sensitive) {
                    if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c + 32);
                    if (d >= 'A' && d <= 'Z') d = static_cast<unsigned char>(d + 32);
                }
                if (c != d) { ok = false; break; }
            }
            if (!ok) continue;
            ScanCandidate c;
            c.address = a;
            c.type    = ValueType::Str;
            m_candidates.push_back(c);
            if (cap && m_candidates.size() >= cap) { truncated = true; break; }
        }
        if (stats) {
            stats->bytes_scanned = end - begin;
            stats->compared      = compared;
            stats->candidates    = m_candidates.size();
            stats->truncated     = truncated;
            stats->elapsed_ms    = std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - t0).count();
        }
        return m_candidates.size();
    }

    // ── Array-of-bytes scan ─────────────────────────────────────────────────
    if (opts.type == ValueType::AoB) {
        const size_t n = value.bytes.size();
        if (n == 0) return 0;
        const size_t sstep = opts.string_aligned ? std::max<size_t>(align, 1) : 1;
        for (uint64_t a = begin; a + n <= end; a += sstep) {
            ++compared;
            bool ok = true;
            for (size_t i = 0; i < n; ++i) {
                const bool wild = !value.mask.empty() && value.mask[i] == 0;
                if (!wild && mem[a + i] != value.bytes[i]) { ok = false; break; }
            }
            if (!ok) continue;
            ScanCandidate c;
            c.address = a;
            c.type    = ValueType::AoB;
            m_candidates.push_back(c);
            if (cap && m_candidates.size() >= cap) { truncated = true; break; }
        }
        if (stats) {
            stats->bytes_scanned = end - begin;
            stats->compared      = compared;
            stats->candidates    = m_candidates.size();
            stats->truncated     = truncated;
            stats->elapsed_ms    = std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - t0).count();
        }
        return m_candidates.size();
    }

    // ── Numeric scan (parallel over the buffer) ─────────────────────────────
    const unsigned nthreads = effective_threads(opts.threads);
    for (ValueType t : types) {
        const size_t w = width_of(t);
        if (!w) continue;
        const size_t step = align ? align : w;   // 0 = natural alignment
        const double ttol = tol_for(t, value.tolerance);

        std::vector<std::vector<ScanCandidate>> locals;
        std::vector<std::thread> workers;
        std::atomic<uint64_t> compared_atomic{0};
        std::atomic<bool> stop{false};

        // Positions are split by INDEX, not by byte range: a worker's slice
        // must start on the same alignment lattice as position 0, otherwise
        // every worker after the first reads windows offset by a few bytes and
        // reports "matches" that are not aligned to the requested type at all.
        const uint64_t npos = (end >= begin + w) ? ((end - begin - w) / step + 1) : 0;
        const uint64_t per  = npos ? (npos + nthreads - 1) / nthreads : 0;
        auto position_addr = [&](uint64_t i) { return begin + i * step; };

        if (nthreads <= 1) {
            locals.resize(1);
        } else {
            locals.resize(nthreads);
            for (unsigned ti = 0; ti < nthreads; ++ti) {
                workers.emplace_back([&, ti]() {
                    const uint64_t i0  = per * ti;
                    const uint64_t i1  = std::min<uint64_t>(npos, i0 + per);
                    uint64_t local_compared = 0;
                    auto& out = locals[ti];
                    for (uint64_t i = i0; i < i1; ++i) {
                        if (stop.load(std::memory_order_relaxed)) break;
                        const uint64_t a = position_addr(i);
                        MemNumber cur;
                        if (!mem_read_number(mem, mem_size, a, t, &cur)) continue;
                        ++local_compared;
                        if (seed_scan == ScanType::UnknownInitial ||
                            matches(seed_scan, cur, cur, value, ttol)) {
                            ScanCandidate c;
                            c.address        = a;
                            c.type           = t;
                            c.previous       = cur;
                            c.first_seen_value = cur.as_double();
                            out.push_back(c);
                            if (cap && out.size() >= cap) {
                                stop.store(true, std::memory_order_relaxed);
                                break;
                            }
                        }
                    }
                    compared_atomic.fetch_add(local_compared, std::memory_order_relaxed);
                });
            }
            for (auto& th : workers) th.join();
        }

        if (nthreads <= 1) {
            uint64_t local_compared = 0;
            for (uint64_t a = begin; a + w <= end; a += step) {
                MemNumber cur;
                if (!mem_read_number(mem, mem_size, a, t, &cur)) continue;
                ++local_compared;
                if (seed_scan == ScanType::UnknownInitial ||
                    matches(seed_scan, cur, cur, value, ttol)) {
                    ScanCandidate c;
                    c.address          = a;
                    c.type             = t;
                    c.previous         = cur;
                    c.first_seen_value = cur.as_double();
                    m_candidates.push_back(c);
                    if (cap && m_candidates.size() >= cap) { truncated = true; break; }
                }
            }
            compared_atomic.store(local_compared);
        } else {
            for (auto& v : locals) {
                if (m_candidates.size() >= cap && cap) { truncated = true; break; }
                for (auto& c : v) {
                    m_candidates.push_back(c);
                    if (cap && m_candidates.size() >= cap) { truncated = true; break; }
                }
            }
        }
        compared += compared_atomic.load();
        if (truncated) break;
    }

    // Deterministic ordering: by type, then address.
    std::sort(m_candidates.begin(), m_candidates.end(),
              [](const ScanCandidate& a, const ScanCandidate& b) {
                  if (a.type != b.type) return static_cast<int>(a.type) < static_cast<int>(b.type);
                  return a.address < b.address;
              });
    if (stats) {
        stats->bytes_scanned = end - begin;
        stats->compared      = compared;
        stats->candidates    = m_candidates.size();
        stats->truncated     = truncated;
        stats->elapsed_ms    = std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - t0).count();
    }
    return m_candidates.size();
}

size_t MemScanner::next_scan(const uint8_t* mem, uint64_t mem_size,
                            const ScanValue& value, const ScanOptions& opts,
                            ScanStats* stats) {
    const auto t0 = std::chrono::steady_clock::now();

    // Every filter is meaningful on a narrowing pass, so filtering is never
    // dropped here; the field exists so the UI can report a first-scan drop and
    // a narrowing pass with the same code path.
    if (stats) {
        *stats = ScanStats{};
        stats->requested = opts.scan;
        stats->applied   = opts.scan;
    }

    if (!mem || m_candidates.empty()) {
        if (stats) stats->candidates = 0;
        return 0;
    }

    const bool keep_all = (opts.scan == ScanType::UnknownInitial);
    const double tol = value.tolerance;

    // Parallel filter: each chunk collects into its own vector so the merged
    // result keeps the original (address-sorted) order exactly.
    const unsigned nthreads = effective_threads(opts.threads);
    std::vector<std::vector<ScanCandidate>> locals(nthreads);
    std::vector<std::thread> workers;
    const size_t total = m_candidates.size();
    const size_t chunk = (total + nthreads - 1) / nthreads;

    auto filter_range = [&](unsigned ti) {
        const size_t lo = chunk * ti;
        const size_t hi = std::min(total, lo + chunk);
        auto& out = locals[ti];
        for (size_t i = lo; i < hi; ++i) {
            const ScanCandidate& c = m_candidates[i];
            if (c.type == ValueType::Str || c.type == ValueType::AoB) {
                // Variable-width candidates are re-validated, not compared.
                if (c.address < mem_size) out.push_back(c);
                continue;
            }
            MemNumber cur;
            if (!mem_read_number(mem, mem_size, c.address, c.type, &cur)) continue;
            if (keep_all || matches(opts.scan, cur, c.previous, value, tol_for(c.type, tol))) {
                ScanCandidate nc = c;
                nc.previous = cur;      // becomes the baseline for the next pass
                out.push_back(nc);
            }
        }
    };

    if (nthreads <= 1) {
        filter_range(0);
    } else {
        for (unsigned ti = 0; ti < nthreads; ++ti)
            workers.emplace_back(filter_range, ti);
        for (auto& th : workers) th.join();
    }

    std::vector<ScanCandidate> merged;
    merged.reserve(m_candidates.size());
    for (auto& v : locals)
        merged.insert(merged.end(), v.begin(), v.end());

    if (opts.max_results && merged.size() > opts.max_results) {
        merged.resize(opts.max_results);
        if (stats) stats->truncated = true;
    }
    m_candidates.swap(merged);

    if (stats) {
        stats->compared   = total;
        stats->candidates = m_candidates.size();
        stats->elapsed_ms = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - t0).count();
    }
    return m_candidates.size();
}

void MemScanner::snapshot_values(const uint8_t* mem, uint64_t mem_size,
                                 const ScanOptions& opts) {
    (void)opts;
    if (!mem) return;
    for (auto& c : m_candidates) {
        if (c.type == ValueType::Str || c.type == ValueType::AoB) continue;
        MemNumber cur;
        if (mem_read_number(mem, mem_size, c.address, c.type, &cur))
            c.previous = cur;
    }
}

bool mem_write_typed(uint8_t* mem, uint64_t mem_size, uint64_t addr,
                     ValueType type, const std::string& text, std::string* err,
                     ValueDomain domain) {
    if (!mem) { if (err) *err = "no guest memory"; return false; }

    if (type == ValueType::Str) {
        std::vector<uint8_t> bytes(text.begin(), text.end());
        bytes.push_back(0);
        return mem_write_bytes(mem, mem_size, addr, bytes, err);
    }
    if (type == ValueType::AoB) {
        ScanValue v;
        if (!MemScanner::parse_typed_text_ex(ValueType::AoB, text, domain, &v, err)) {
            if (err && err->empty()) *err = "not a byte pattern";
            return false;
        }
        return mem_write_bytes(mem, mem_size, addr, v.bytes, err);
    }

    ScanValue v;
    if (!MemScanner::parse_typed_text_ex(type, text, domain, &v, err)) {
        if (err && err->empty())
            *err = "value does not fit " + std::string(value_type_name(type));
        return false;
    }
    return mem_write_bytes(mem, mem_size, addr, mem_encode_number(v.num, type), err);
}

} // namespace swordfare::research
