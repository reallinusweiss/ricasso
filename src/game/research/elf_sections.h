// =============================================================================
// elf_sections.h — the loaded image's sections, as the coarse answer to
//                  "where is this address?"
//
// WHY THIS IS NEEDED ON TOP OF THE SYMBOL TABLE
//
// The symbol table has a hole at the bottom.  Measured on the shipped
// libswordigo.so 1.4.13 arm64:
//
//     .dynsym      rva 0x002F8   size 0x68328
//     .dynstr      rva 0x08E7B8  size 0x109414
//     .rela.dyn    rva 0x197BD0  size 0x740A0
//     .rela.plt    rva 0x20BC70  size 0x1AC88
//     .rodata      rva 0x259B00  size 0x1A0B4
//     .text        rva 0x321990  size 0x2E1278
//     lowest symbol rva  0x264397
//
// So *nothing* below 0x264397 has a symbol — the whole metadata block.  And that
// is where a naively-scoped scan lands: a real exact scan of the module range for
// a common 4-byte value returned 12 hits, of which 8 sit inside .dynsym and
// .rela.plt, i.e. hits on the emulator's own bookkeeping rather than on game
// state.  Reporting those as "no symbol below" would be technically true and
// practically useless; reporting them as ".dynsym+0x30D60" tells a researcher
// instantly to ignore them.
//
// So: symbol first, section second, and metadata sections are also excludable.
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace swordfare::research {

// One section of a loaded image.
struct SectionEntry {
    uint64_t    addr  = 0;      // module-relative start
    uint64_t    size  = 0;
    uint32_t    type  = 0;      // SHT_*
    uint64_t    flags = 0;      // SHF_*
    std::string name;           // "" when no section-name table was kept
};

enum class SectionKind : uint8_t {
    Unknown = 0,
    Code,           // executable
    ReadOnlyData,   // .rodata, .data.rel.ro, .eh_frame, ...
    WritableData,   // .data, .got
    Bss,            // zero-filled, no file image
    SymbolTable,    // .dynsym / .symtab
    StringTable,    // .dynstr / .strtab
    Relocations,    // .rela.dyn / .rela.plt
    Hash,           // .hash / .gnu.hash
    Dynamic,        // .dynamic
    Other,
};

// Classify using the ELF type/flags, falling back to the name only where the
// type does not carry the distinction (which string table is this?).
SectionKind section_kind_of(uint32_t sh_type, uint64_t sh_flags, const std::string& name);
const char* section_kind_name(SectionKind k);

// True for the image's own bookkeeping.  A scanner should almost never be
// looking here, and a researcher should be told when it is.
bool section_kind_is_metadata(SectionKind k);

class SectionMap {
public:
    // `shdrs` is the loader's section-header array (so_module_arm64::shdr).
    // `names` is an optional copy of the section-name string table; when it is
    // absent the sections are still usable, they are just described by kind.
    // `is_64` selects the Elf64_Shdr / Elf32_Shdr layout.  Returns false when
    // nothing usable was handed over.
    bool build(const std::string& key, const void* shdrs, size_t count,
               const char* names, size_t names_size, bool is_64);

    // Direct form, also the test entry point.
    void set(const std::string& key, std::vector<SectionEntry> sections);

    bool   has(const std::string& key) const;
    size_t count(const std::string& key) const;
    size_t metadata_count(const std::string& key) const;

    // Section containing `rva`, or nullptr.  Zero-size sections never match:
    // claiming membership for a section with no extent would be a lie.
    const SectionEntry* find(const std::string& key, uint64_t rva) const;
    SectionKind         kind_at(const std::string& key, uint64_t rva) const;

    // True when the address is in the image's own bookkeeping.
    bool is_metadata(const std::string& key, uint64_t rva) const {
        return section_kind_is_metadata(kind_at(key, rva));
    }

    // ".dynsym+0x30D60", or "code+0x1038" when the image kept no section names,
    // or "" when nothing covers the address.
    std::string describe(const std::string& key, uint64_t rva) const;

    // [begin, end) of every metadata section — the ranges a scan can skip.
    std::vector<std::pair<uint64_t, uint64_t>> metadata_ranges(const std::string& key) const;

    // The end of the highest section, i.e. how much of the image is described.
    uint64_t covered_end(const std::string& key) const;

    std::string summary() const;

private:
    struct Module {
        std::vector<SectionEntry> secs;   // sorted by addr
        size_t metadata = 0;
    };
    std::vector<std::pair<std::string, Module>> m_modules;
};

// Is `rva` inside any [begin, end) range?  Used for the scan-skip filter.
bool rva_in_ranges(const std::vector<std::pair<uint64_t, uint64_t>>& ranges, uint64_t rva);

} // namespace swordfare::research
