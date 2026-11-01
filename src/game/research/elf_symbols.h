// =============================================================================
// elf_symbols.h — the loaded module's dynamic symbol table, as evidence
//
// WHY THIS EXISTS
//
// libswordigo.so ships with ~17 700 dynamic symbols (14 505 of them Caver::*),
// with real names, types and sizes.  That is a far better answer to "where is
// this address?" than anything we can compute, so a static site should be
// described by the symbol it falls in — not by a bare `module+0xOFFSET`.
//
// WHAT IT IS *NOT* ALLOWED TO DO
//
// It must never feed the identity hash.  A runtime address and a symbol name are
// different kinds of fact: the identity is a stable, short, comparable handle
// (g_VAR_7676), while a symbol is a long, human-meaningful annotation that can
// change shape between builds.  Putting the symbol name into the identity key
// would make the name long, unstable across builds, and — because most addresses
// sit *between* symbols rather than inside one — frequently wrong by
// construction.  So this layer is display/evidence only:
//
//   identity  :  g_VAR_7676                      (from mem_identity.h, unchanged)
//   symbol    :  Caver::GameSceneController::Update+0x18   (nearest below)
//   between   :  Caver::Update+0x4 .. Caver::Render   (the gap it sits in)
//
// Data symbols and zero-sized symbols are common, so "inside a symbol" is only
// claimed when the extent actually covers the address; otherwise we say which
// two symbols the address lies between rather than implying it belongs to one.
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swordfare::research {

// One entry of a loaded image's dynamic symbol table.
struct SymbolEntry {
    uint64_t    rva  = 0;      // st_value, module-relative
    uint64_t    size = 0;      // st_size (0 = unknown extent)
    uint8_t     type = 0;      // STT_* (2 = FUNC, 1 = OBJECT)
    std::string name;          // exactly as it appears in .dynstr
    std::string brief;         // readable form for a table column
};

// ---------------------------------------------------------------------------
// brief_symbol_name — Itanium ABI mangled name -> something a column can hold.
//
// A deliberately small transform, not a full demangler: it handles the nested
// `Caver::Foo::Bar` shape plus a single level of templates, which is what the
// engine's symbols actually look like.  Anything it cannot parse is returned
// unchanged, because a wrong name is worse than a mangled one.
// ---------------------------------------------------------------------------
std::string brief_symbol_name(const std::string& mangled);

// The coarser half — "which section is this?" — lives in elf_sections.h; a symbol
// table alone has a hole below its lowest symbol, and that hole is exactly where
// a module-scoped scan floods.

// ---------------------------------------------------------------------------
// SymbolTable — per-module symbol index, sorted for binary search.
// ---------------------------------------------------------------------------
class SymbolTable {
public:
    // Build from a loaded module's dynamic symbol table, i.e. the pointers the
    // ELF loader already holds (so_module_arm64::dynsym / dynstr / num_dynsym).
    //
    // `symtab` is the raw array and `count` its length; `is_64` selects the
    // Elf64_Sym or Elf32_Sym layout.  Zero-valued entries (imports, undefined
    // symbols) and empty names are skipped: they carry no address and would
    // otherwise sort to the front of every flood lookup.  Returns false when the
    // table is unusable, in which case every lookup stays empty rather than
    // inventing a symbol.
    bool build(const std::string& key, const void* symtab, size_t count,
               const char* strtab, size_t strtab_size, bool is_64);

    // Build from already-normalised entries (also the test entry point).
    void set(const std::string& key, std::vector<SymbolEntry> entries);

    bool   has(const std::string& key) const;
    size_t count(const std::string& key) const;
    // How many entries for this module are functions (the useful ones).
    size_t func_count(const std::string& key) const;
    const std::vector<SymbolEntry>* entries(const std::string& key) const;

    // Nearest symbol at or below `rva` — "the last dynamic symbol".
    const SymbolEntry* floor(const std::string& key, uint64_t rva) const;

    // Nearest symbol strictly above `rva` — the far side of the gap.
    const SymbolEntry* ceil(const std::string& key, uint64_t rva) const;

    // True when `s` has a known extent and `rva` falls inside it, so the address
    // can honestly be called part of that symbol.
    static bool covers(const SymbolEntry& s, uint64_t rva);

    // "Caver::GameSceneController::Update+0x18" — or "" when there is no symbol
    // at or below the address.
    std::string describe_floor(const std::string& key, uint64_t rva) const;

    // "Caver::A+0x4 .. Caver::B" — the two symbols this address sits between.
    // Falls back to one side when the address is past the last symbol or before
    // the first, and to "" when there are no symbols at all.
    std::string describe_between(const std::string& key, uint64_t rva) const;

    // One line for the UI: how much evidence this table actually carries.
    std::string summary() const;

    // --- a few truths about the shipped ABI, used by the UI hints ---

    // libswordigo.so 1.4.13 arm64 facts, measured from the image itself:
    //   17783 dynsym entries, 13213 of them functions
    //   lowest symbol rva  0x264397   (so everything below it has no symbol)
    //   .dynsym 0x2F8, .dynstr 0x8E7B8, .rela.dyn 0x197BD0, .rela.plt 0x20BC70,
    //   .rodata 0x259B00, .text 0x321990, .data 0x64F210
    static constexpr uint64_t kFirstSymbolRvaHint = 0x264397;

private:
    void sort_key(std::vector<SymbolEntry>& v) const;

    struct Module {
        std::vector<SymbolEntry> syms;   // sorted by rva
        size_t funcs = 0;
    };
    std::vector<std::pair<std::string, Module>> m_modules;
};

} // namespace swordfare::research
