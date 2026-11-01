// =============================================================================
// elf_sections.cpp — see elf_sections.h for the measured layout this exists for.
// =============================================================================

#include "game/research/elf_sections.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace swordfare::research {

namespace {

// ELF section types we care about.
constexpr uint32_t kSHT_PROGBITS = 1;
constexpr uint32_t kSHT_SYMTAB   = 2;
constexpr uint32_t kSHT_STRTAB   = 3;
constexpr uint32_t kSHT_RELA     = 4;
constexpr uint32_t kSHT_HASH     = 5;
constexpr uint32_t kSHT_DYNAMIC  = 6;
constexpr uint32_t kSHT_NOBITS   = 8;
constexpr uint32_t kSHT_REL      = 9;
constexpr uint32_t kSHT_DYNSYM   = 11;
constexpr uint32_t kSHT_GNU_HASH = 0x6ffffff6;

// Section flags.
constexpr uint64_t kSHF_WRITE     = 0x1;
constexpr uint64_t kSHF_ALLOC     = 0x2;
constexpr uint64_t kSHF_EXECINSTR = 0x4;

bool name_starts(const std::string& n, const char* p) {
    return n.rfind(p, 0) == 0;
}

} // namespace

SectionKind section_kind_of(uint32_t sh_type, uint64_t sh_flags, const std::string& name) {
    switch (sh_type) {
        case kSHT_DYNSYM:
        case kSHT_SYMTAB:
            return SectionKind::SymbolTable;
        case kSHT_STRTAB:
            // Which string table this is cannot be told from the type, so the
            // name is the only evidence available — and it is decisive here.
            if (name_starts(name, ".dynstr")) return SectionKind::StringTable;
            if (name_starts(name, ".strtab")) return SectionKind::StringTable;
            if (name.empty())                 return SectionKind::StringTable;
            return SectionKind::Other;
        case kSHT_RELA:
        case kSHT_REL:
            return SectionKind::Relocations;
        case kSHT_HASH:
        case kSHT_GNU_HASH:
            return SectionKind::Hash;
        case kSHT_DYNAMIC:
            return SectionKind::Dynamic;
        case kSHT_NOBITS:
            return SectionKind::Bss;
        case kSHT_PROGBITS:
        default:
            break;
    }
    if (sh_flags & kSHF_EXECINSTR) return SectionKind::Code;
    if (sh_flags & kSHF_WRITE)     return SectionKind::WritableData;
    if (sh_flags & kSHF_ALLOC)     return SectionKind::ReadOnlyData;
    // Non-allocated PROGBITS is debug/metadata, not part of the running image.
    return SectionKind::Other;
}

const char* section_kind_name(SectionKind k) {
    switch (k) {
        case SectionKind::Code:         return "code";
        case SectionKind::ReadOnlyData: return "read-only data";
        case SectionKind::WritableData: return "writable data";
        case SectionKind::Bss:          return "bss";
        case SectionKind::SymbolTable:  return "symbol table";
        case SectionKind::StringTable:  return "string table";
        case SectionKind::Relocations:  return "relocations";
        case SectionKind::Hash:         return "hash table";
        case SectionKind::Dynamic:      return "dynamic";
        case SectionKind::Other:        return "other";
        case SectionKind::Unknown:
        default:                        return "unknown";
    }
}

bool section_kind_is_metadata(SectionKind k) {
    switch (k) {
        case SectionKind::SymbolTable:
        case SectionKind::StringTable:
        case SectionKind::Relocations:
        case SectionKind::Hash:
        case SectionKind::Dynamic:
            return true;
        default:
            return false;
    }
}

void SectionMap::set(const std::string& key, std::vector<SectionEntry> sections) {
    Module m;
    std::stable_sort(sections.begin(), sections.end(),
                     [](const SectionEntry& a, const SectionEntry& b) {
                         if (a.addr != b.addr) return a.addr < b.addr;
                         return a.size > b.size;   // prefer the wider one at a tie
                     });
    for (const SectionEntry& s : sections)
        if (section_kind_is_metadata(section_kind_of(s.type, s.flags, s.name))) ++m.metadata;
    m.secs = std::move(sections);
    for (auto& kv : m_modules) {
        if (kv.first == key) { kv.second = std::move(m); return; }
    }
    m_modules.emplace_back(key, std::move(m));
}

bool SectionMap::build(const std::string& key, const void* shdrs, size_t count,
                       const char* names, size_t names_size, bool is_64) {
    if (!shdrs || count == 0) return false;

    const uint8_t* p = static_cast<const uint8_t*>(shdrs);
    const size_t step = is_64 ? 64 : 40;   // sizeof(Elf64_Shdr) / sizeof(Elf32_Shdr)

    std::vector<SectionEntry> out;
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* sh = p + i * step;
        uint32_t name_off = 0, type = 0;
        uint64_t flags = 0, addr = 0, size = 0;
        std::memcpy(&name_off, sh + 0, 4);
        std::memcpy(&type,     sh + 4, 4);
        if (is_64) {
            // Elf64_Shdr: name(4) type(4) flags(8) addr(8) offset(8) size(8) ...
            std::memcpy(&flags, sh + 8,  8);
            std::memcpy(&addr,  sh + 16, 8);
            std::memcpy(&size,  sh + 32, 8);
        } else {
            // Elf32_Shdr: name(4) type(4) flags(4) addr(4) offset(4) size(4) ...
            uint32_t f = 0, a = 0, s = 0;
            std::memcpy(&f, sh + 8,  4);
            std::memcpy(&a, sh + 12, 4);
            std::memcpy(&s, sh + 20, 4);
            flags = f; addr = a; size = s;
        }

        SectionEntry e;
        e.addr  = addr;
        e.size  = size;
        e.type  = type;
        e.flags = flags;
        if (names && names_size && name_off < names_size) {
            const char* nm  = names + name_off;
            const size_t rem = names_size - name_off;
            e.name.assign(nm, strnlen(nm, rem));
        }
        out.push_back(std::move(e));
    }
    if (out.empty()) return false;
    set(key, std::move(out));
    return true;
}

bool SectionMap::has(const std::string& key) const { return count(key) > 0; }

size_t SectionMap::count(const std::string& key) const {
    for (const auto& kv : m_modules)
        if (kv.first == key) return kv.second.secs.size();
    return 0;
}

size_t SectionMap::metadata_count(const std::string& key) const {
    for (const auto& kv : m_modules)
        if (kv.first == key) return kv.second.metadata;
    return 0;
}

const SectionEntry* SectionMap::find(const std::string& key, uint64_t rva) const {
    for (const auto& kv : m_modules) {
        if (kv.first != key) continue;
        // Sections are sorted by address and do not overlap, so the last one
        // starting at or below `rva` is the only candidate.
        const SectionEntry* best = nullptr;
        for (const SectionEntry& s : kv.second.secs) {
            if (s.addr > rva) break;
            if (s.size == 0) continue;      // no extent: cannot contain anything
            if (rva < s.addr + s.size) best = &s;
        }
        return best;
    }
    return nullptr;
}

SectionKind SectionMap::kind_at(const std::string& key, uint64_t rva) const {
    const SectionEntry* s = find(key, rva);
    if (!s) return SectionKind::Unknown;
    return section_kind_of(s->type, s->flags, s->name);
}

std::string SectionMap::describe(const std::string& key, uint64_t rva) const {
    const SectionEntry* s = find(key, rva);
    if (!s) return std::string();
    char b[96];
    std::snprintf(b, sizeof(b), "+0x%llX", static_cast<unsigned long long>(rva - s->addr));
    const std::string label =
        s->name.empty() ? std::string(section_kind_name(section_kind_of(s->type, s->flags, s->name)))
                        : s->name;
    return label + b;
}

std::vector<std::pair<uint64_t, uint64_t>> SectionMap::metadata_ranges(const std::string& key) const {
    std::vector<std::pair<uint64_t, uint64_t>> out;
    for (const auto& kv : m_modules) {
        if (kv.first != key) continue;
        for (const SectionEntry& s : kv.second.secs) {
            if (s.size == 0) continue;
            if (!section_kind_is_metadata(section_kind_of(s.type, s.flags, s.name))) continue;
            out.emplace_back(s.addr, s.addr + s.size);
        }
    }
    return out;
}

uint64_t SectionMap::covered_end(const std::string& key) const {
    uint64_t end = 0;
    for (const auto& kv : m_modules) {
        if (kv.first != key) continue;
        for (const SectionEntry& s : kv.second.secs)
            end = std::max(end, s.addr + s.size);
    }
    return end;
}

std::string SectionMap::summary() const {
    if (m_modules.empty()) return "no sections loaded";
    std::string s;
    for (const auto& kv : m_modules) {
        char b[160];
        std::snprintf(b, sizeof(b), "%s: %zu section(s), %zu metadata, ends at 0x%llX",
                      kv.first.c_str(), kv.second.secs.size(), kv.second.metadata,
                      static_cast<unsigned long long>(covered_end(kv.first)));
        if (!s.empty()) s += "  |  ";
        s += b;
    }
    return s;
}

bool rva_in_ranges(const std::vector<std::pair<uint64_t, uint64_t>>& ranges, uint64_t rva) {
    for (const auto& r : ranges)
        if (rva >= r.first && rva < r.second) return true;
    return false;
}

} // namespace swordfare::research
