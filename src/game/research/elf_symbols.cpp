// =============================================================================
// elf_symbols.cpp — see elf_symbols.h for why this layer exists and, more
// importantly, for what it is forbidden to do (never touch the identity hash).
// =============================================================================

#include "game/research/elf_symbols.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// brief_symbol_name
// ---------------------------------------------------------------------------
namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

// Read "<len><chars>" at `i`; returns the identifier and advances past it.
bool read_len_ident(const std::string& s, size_t* i, std::string* out) {
    size_t j = *i;
    while (j < s.size() && is_digit(s[j])) ++j;
    if (j == *i) return false;                 // no digits: not a length
    if (j >= s.size()) return false;
    size_t len = 0;
    for (size_t k = *i; k < j; ++k) len = len * 10 + static_cast<size_t>(s[k] - '0');
    if (len == 0 || j + len > s.size()) return false;
    out->assign(s, j, len);
    *i = j + len;
    return true;
}

// Index of the 'E' matching the 'I' at `open`, or npos.
size_t matching_close(const std::string& s, size_t open) {
    int depth = 0;
    for (size_t i = open; i < s.size(); ++i) {
        if (s[i] == 'I') ++depth;
        else if (s[i] == 'E') {
            if (--depth == 0) return i;
        }
    }
    return std::string::npos;
}

// Forward declaration: parse the body of a nested-name / template-argument run.
bool brief_parse(const std::string& s, size_t* i, std::string* out);

// One component of a nested name at `i`.  Handles a substitution back-reference
// (St/Ss/...), a nested-name (N...E) and a template instantiation.
bool brief_component(const std::string& s, size_t* i, std::string* out) {
    if (*i >= s.size()) return false;

    // Substitutions we can render without a full substitution table.
    if (s.compare(*i, 3, "St9") == 0) { *out = "std"; *i += 3; return true; }
    if (s.compare(*i, 3, "St6") == 0) { *out = "std"; *i += 3; return true; }
    if (s.compare(*i, 2, "St") == 0)  { *out = "std"; *i += 2; return true; }

    std::string ident;
    if (!read_len_ident(s, i, &ident)) return false;
    *out = ident;

    // A template argument list belongs to this component.
    if (*i < s.size() && s[*i] == 'I') {
        const size_t close = matching_close(s, *i);
        if (close == std::string::npos) return true;      // unparsable: keep the name
        size_t k = *i + 1;
        std::string args;
        while (k < close) {
            std::string one;
            const size_t before = k;
            if (!brief_parse(s, &k, &one)) { k = before + 1; continue; }
            if (!args.empty()) args += ", ";
            args += one;
            // Argument runs are separated by a leading 'J'.
            if (k < close && s[k] == 'J') { ++k; continue; }
        }
        if (!args.empty()) *out = ident + "<" + args + ">";
        *i = close + 1;
    }
    return true;
}

// Parse one type/name starting at `i`.
bool brief_parse(const std::string& s, size_t* i, std::string* out) {
    if (*i >= s.size()) return false;
    const char c = s[*i];

    if (c == 'N') {                       // nested name: N<component>+E
        ++(*i);
        std::string acc;
        while (*i < s.size() && s[*i] != 'E') {
            std::string part;
            if (!brief_component(s, i, &part)) return false;
            acc += acc.empty() ? part : ("::" + part);
        }
        if (*i < s.size() && s[*i] == 'E') ++(*i);
        *out = acc;
        return !acc.empty();
    }

    if (is_digit(c)) return brief_component(s, i, out);

    // Pointer / reference / qualifier wrappers around a name.
    if (c == 'P' || c == 'R' || c == 'O' || c == 'K') {
        ++(*i);
        std::string inner;
        if (!brief_parse(s, i, &inner)) return false;
        *out = inner;
        if (c == 'P') *out += "*";
        if (c == 'R') *out += "&";
        return true;
    }

    // Builtin type codes: only what a symbol's *name* ever needs.
    const char* builtin = nullptr;
    switch (c) {
        case 'v': *out = "void";   ++(*i); return true;
        case 'b': *out = "bool";   ++(*i); return true;
        case 'c': *out = "char";   ++(*i); return true;
        case 'i': *out = "int";    ++(*i); return true;
        case 'j': *out = "uint";   ++(*i); return true;
        case 'l': *out = "long";   ++(*i); return true;
        case 'm': *out = "ulong";  ++(*i); return true;
        case 'f': *out = "float";  ++(*i); return true;
        case 'd': *out = "double"; ++(*i); return true;
        case 'x': *out = "int64";  ++(*i); return true;
        case 'y': *out = "uint64"; ++(*i); return true;
        default: (void)builtin; return false;
    }
}

} // namespace

std::string brief_symbol_name(const std::string& mangled) {
    if (mangled.empty()) return mangled;
    if (mangled.rfind("_Z", 0) != 0) return mangled;   // C name or already readable

    std::string s    = mangled.substr(2);
    std::string lead;

    // Special-name prefixes the runtime itself generates.
    if (s.compare(0, 2, "TV") == 0) { lead = "vtable for ";   s = s.substr(2); }
    else if (s.compare(0, 2, "TI") == 0) { lead = "typeinfo for "; s = s.substr(2); }
    else if (s.compare(0, 2, "TS") == 0) { lead = "name of ";     s = s.substr(2); }
    else if (s.compare(0, 2, "TT") == 0) { lead = "VTT for ";     s = s.substr(2); }
    else if (s.compare(0, 2, "GV") == 0) { lead = "guard for ";   s = s.substr(2); }

    size_t i = 0;
    std::string name;
    if (!brief_parse(s, &i, &name) || name.empty()) return mangled;

    // Constructor / destructor encodings appear as a final component code.
    if (i + 1 < s.size() && (s[i] == 'C' || s[i] == 'D') && is_digit(s[i + 1])) {
        name += (s[i] == 'C') ? "::<ctor>" : "::<dtor>";
    }
    return lead + name;
}

// ---------------------------------------------------------------------------
// SymbolTable
// ---------------------------------------------------------------------------
namespace {

// Read one symbol out of whichever ELF layout the image uses.  Field offsets are
// hard-coded on purpose: this file must not depend on the loader's ELF structs,
// so it stays testable with a synthetic array.
struct RawSym {
    uint32_t name_off = 0;
    uint64_t value    = 0;
    uint64_t size     = 0;
    uint8_t  type     = 0;
};

bool read_raw_sym(const uint8_t* p, bool is_64, RawSym* out) {
    uint32_t info = 0;
    if (is_64) {
        // Elf64_Sym: name(4) info(1) other(1) shndx(2) value(8) size(8)
        std::memcpy(&out->name_off, p + 0, 4);
        std::memcpy(&info,        p + 4, 1);
        std::memcpy(&out->value,  p + 8, 8);
        std::memcpy(&out->size,   p + 16, 8);
    } else {
        // Elf32_Sym: name(4) value(4) size(4) info(1) other(1) shndx(2)
        std::memcpy(&out->name_off, p + 0, 4);
        uint32_t v = 0, sz = 0;
        std::memcpy(&v,           p + 4, 4);
        std::memcpy(&sz,          p + 8, 4);
        std::memcpy(&info,        p + 12, 1);
        out->value = v;
        out->size  = sz;
    }
    out->type = static_cast<uint8_t>(info & 0x0F);
    return true;
}

} // namespace

void SymbolTable::set(const std::string& key, std::vector<SymbolEntry> entries) {
    Module m;
    sort_key(entries);
    for (const SymbolEntry& e : entries) {
        if (e.type == 2 /*STT_FUNC*/) ++m.funcs;
    }
    m.syms = std::move(entries);
    for (auto& kv : m_modules) {
        if (kv.first == key) { kv.second = std::move(m); return; }
    }
    m_modules.emplace_back(key, std::move(m));
}

bool SymbolTable::build(const std::string& key, const void* symtab, size_t count,
                        const char* strtab, size_t strtab_size, bool is_64) {
    if (!symtab || !strtab || count == 0 || strtab_size == 0) return false;

    const uint8_t* p    = static_cast<const uint8_t*>(symtab);
    const size_t   step = is_64 ? 24 : 16;

    std::vector<SymbolEntry> out;
    out.reserve(count / 2);
    for (size_t i = 0; i < count; ++i) {
        RawSym r;
        read_raw_sym(p + i * step, is_64, &r);
        // st_value == 0 is an import or an undefined symbol: it has no address,
        // so it must not become the "nearest symbol below" for everything.
        if (r.value == 0) continue;
        if (r.name_off == 0 || r.name_off >= strtab_size) continue;
        const char* nm = strtab + r.name_off;
        const size_t max = strtab_size - r.name_off;
        const size_t len = strnlen(nm, max);
        if (len == 0) continue;
        SymbolEntry e;
        e.rva  = r.value;
        e.size = r.size;
        e.type = r.type;
        e.name.assign(nm, len);
        e.brief = brief_symbol_name(e.name);
        out.push_back(std::move(e));
    }
    if (out.empty()) return false;
    set(key, std::move(out));
    return true;
}

void SymbolTable::sort_key(std::vector<SymbolEntry>& v) const {
    std::stable_sort(v.begin(), v.end(), [](const SymbolEntry& a, const SymbolEntry& b) {
        if (a.rva != b.rva) return a.rva < b.rva;
        // Two symbols can share an address (aliases).  Prefer the one with an
        // extent, then a function, then the shorter name: a column that says
        // "+0x0" is worth more than one showing an alias of length 40.
        if ((a.size != 0) != (b.size != 0)) return a.size != 0;
        if ((a.type == 2) != (b.type == 2)) return a.type == 2;
        return a.name.size() < b.name.size();
    });
}

bool SymbolTable::has(const std::string& key) const { return count(key) > 0; }

size_t SymbolTable::count(const std::string& key) const {
    for (const auto& kv : m_modules)
        if (kv.first == key) return kv.second.syms.size();
    return 0;
}

size_t SymbolTable::func_count(const std::string& key) const {
    for (const auto& kv : m_modules)
        if (kv.first == key) return kv.second.funcs;
    return 0;
}

const std::vector<SymbolEntry>* SymbolTable::entries(const std::string& key) const {
    for (const auto& kv : m_modules)
        if (kv.first == key) return &kv.second.syms;
    return nullptr;
}

bool SymbolTable::covers(const SymbolEntry& s, uint64_t rva) {
    return s.size != 0 && rva >= s.rva && rva < s.rva + s.size;
}

const SymbolEntry* SymbolTable::floor(const std::string& key, uint64_t rva) const {
    const std::vector<SymbolEntry>* v = entries(key);
    if (!v || v->empty()) return nullptr;
    // upper_bound(rva) - 1: the last symbol with rva <= the address.
    auto it = std::upper_bound(v->begin(), v->end(), rva,
                               [](uint64_t val, const SymbolEntry& e) { return val < e.rva; });
    if (it == v->begin()) {
        // The address is below the first symbol (headers, padding).  Falling
        // back to the first symbol would claim membership it does not have.
        return nullptr;
    }
    return &*(it - 1);
}

const SymbolEntry* SymbolTable::ceil(const std::string& key, uint64_t rva) const {
    const std::vector<SymbolEntry>* v = entries(key);
    if (!v || v->empty()) return nullptr;
    auto it = std::upper_bound(v->begin(), v->end(), rva,
                               [](uint64_t val, const SymbolEntry& e) { return val < e.rva; });
    return it == v->end() ? nullptr : &*it;
}

std::string SymbolTable::describe_floor(const std::string& key, uint64_t rva) const {
    const SymbolEntry* s = floor(key, rva);
    if (!s) return std::string();
    char b[64];
    std::snprintf(b, sizeof(b), "+0x%llX", static_cast<unsigned long long>(rva - s->rva));
    return s->brief + b;
}

std::string SymbolTable::describe_between(const std::string& key, uint64_t rva) const {
    const SymbolEntry* lo = floor(key, rva);
    const SymbolEntry* hi = ceil(key, rva);
    if (!lo && !hi) return std::string();
    if (lo && covers(*lo, rva)) {
        // It is genuinely inside a symbol, so say that instead of hedging.
        char b[96];
        std::snprintf(b, sizeof(b), "inside %s  (0x%llX of 0x%llX)",
                      lo->brief.c_str(), static_cast<unsigned long long>(rva - lo->rva),
                      static_cast<unsigned long long>(lo->size));
        return b;
    }
    if (lo && hi) return lo->brief + "+" + [&] {
        char b[32];
        std::snprintf(b, sizeof(b), "0x%llX", static_cast<unsigned long long>(rva - lo->rva));
        return std::string(b);
    }() + "  ..  " + hi->brief;
    if (lo) return "after " + lo->brief;
    return "before " + hi->brief;
}

std::string SymbolTable::summary() const {
    if (m_modules.empty()) return "no symbol table loaded";
    std::string s;
    for (const auto& kv : m_modules) {
        char b[160];
        std::snprintf(b, sizeof(b), "%s: %zu symbol(s), %zu function(s)",
                      kv.first.c_str(), kv.second.syms.size(), kv.second.funcs);
        if (!s.empty()) s += "  |  ";
        s += b;
    }
    return s;
}

} // namespace swordfare::research
