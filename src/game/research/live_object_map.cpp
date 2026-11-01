// =============================================================================
// live_object_map.cpp
// =============================================================================

#include "game/research/live_object_map.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace swordfare::research {

namespace {

constexpr uint64_t kMinValidAddr = 0x1000;   // same guard as the scan engine

bool valid_range(uint64_t addr, uint64_t width, uint64_t mem_size) {
    return addr >= kMinValidAddr && width > 0 &&
           addr <= mem_size && width <= mem_size &&
           (addr + width) <= mem_size;
}

std::string hex(uint64_t v, bool prefix = true) {
    char b[32];
    std::snprintf(b, sizeof(b), prefix ? "0x%llX" : "%llX",
                  static_cast<unsigned long long>(v));
    return b;
}

} // namespace

// ---------------------------------------------------------------------------
// ModuleMap
// ---------------------------------------------------------------------------
void ModuleMap::clear() { m_modules.clear(); }

void ModuleMap::set(const ModuleExtent& m) {
    for (auto& e : m_modules) {
        if (e.name == m.name) { e = m; return; }
    }
    m_modules.push_back(m);
}

const ModuleExtent* ModuleMap::find(const std::string& name) const {
    for (const auto& e : m_modules)
        if (e.name == name) return &e;
    return nullptr;
}

bool ModuleMap::rva_of(uint64_t va, std::string* module, uint64_t* rva) const {
    for (const auto& e : m_modules) {
        if (!e.contains_va(va)) continue;
        if (module) *module = e.name;
        if (rva)    *rva = va - e.base_va + e.rva_begin;
        return true;
    }
    return false;
}

bool ModuleMap::va_of(const std::string& module, uint64_t rva, uint64_t* va) const {
    const ModuleExtent* e = find(module);
    if (!e) return false;
    if (rva < e->rva_begin || rva > e->rva_end) return false;
    if (va) *va = e->base_va + (rva - e->rva_begin);
    return true;
}

std::string ModuleMap::describe(uint64_t va) const {
    std::string module;
    uint64_t rva = 0;
    if (rva_of(va, &module, &rva)) return module + "+" + hex(rva);
    return hex(va) + " (unmapped)";
}

std::string ModuleMap::summary() const {
    if (m_modules.empty()) return "no modules mapped";
    std::string s = std::to_string(m_modules.size()) + " module(s):";
    for (const auto& e : m_modules) {
        s += " " + e.name + "@" + hex(e.base_va) +
             " (rva " + hex(e.rva_begin) + ".." + hex(e.rva_end) + ")";
    }
    return s;
}

// ---------------------------------------------------------------------------
// pointer reads + paths
// ---------------------------------------------------------------------------
bool mem_read_pointer(const uint8_t* mem, uint64_t mem_size, uint64_t addr, uint64_t* out) {
    if (!mem || !out) return false;
    if (!valid_range(addr, 8, mem_size)) return false;
    uint64_t v = 0;
    std::memcpy(&v, mem + addr, 8);
    *out = v;
    return true;
}

bool load_bias_of(const ModuleMap& modules, const std::string& module, uint64_t* bias) {
    const ModuleExtent* e = modules.find(module);
    if (!e) return false;
    if (bias) *bias = e->base_va;
    return true;
}

std::string PointerResolution::describe() const {
    if (!ok) return "unresolved: " + error;
    std::string s;
    for (size_t i = 0; i < hops.size(); ++i)
        s += hex(hops[i]) + " -> ";
    s += hex(address);
    if (!module.empty()) s += "  [" + module + "+" + hex(final_rva) + "]";
    return s;
}

PointerResolution resolve_pointer_path(const PointerPath& path,
                                       const ModuleMap& modules,
                                       const uint8_t* mem, uint64_t mem_size) {
    PointerResolution r;
    if (!mem) { r.error = "no guest memory"; return r; }
    if (path.offsets.empty()) { r.error = "empty path"; return r; }

    uint64_t base_va = 0;
    if (!load_bias_of(modules, path.module, &base_va)) {
        r.error = "unknown module '" + path.module + "'";
        return r;
    }
    // CE semantics: the ROOT is read first, then each intermediate offset is
    // applied to the pointers we already followed.  Applying offsets[0] to the
    // pointer *slot* instead of its target would resolve a field of the wrong
    // object entirely.
    uint64_t cursor = 0;
    if (!mem_read_pointer(mem, mem_size, base_va + path.root_rva, &cursor)) {
        r.error = "root pointer at " + hex(base_va + path.root_rva) + " is unreadable";
        return r;
    }
    if (!cursor) { r.error = "root pointer is null"; return r; }
    r.hops.push_back(cursor);

    // Dereference all but the last offset, then add the final one.
    for (size_t i = 0; i + 1 < path.offsets.size(); ++i) {
        cursor += path.offsets[i];
        uint64_t ptr = 0;
        if (!mem_read_pointer(mem, mem_size, cursor, &ptr)) {
            r.error = "read of a pointer at " + hex(cursor) + " failed";
            return r;
        }
        if (!ptr) { r.error = "null pointer at level " + std::to_string(i + 1); return r; }
        r.hops.push_back(ptr);
        cursor = ptr;
    }

    r.address = cursor + path.offsets.back();
    r.ok      = true;
    modules.rva_of(r.address, &r.module, &r.final_rva);
    return r;
}

// ---------------------------------------------------------------------------
// LiveObjectMap
// ---------------------------------------------------------------------------
uint64_t LiveObjectMap::add_root(const ObjectRoot& root) {
    LiveObject o;
    o.id            = m_next_id++;
    o.name          = root.label;
    o.type_name     = root.struct_name.empty() ? root.label : root.struct_name;
    o.static_module = root.module;
    o.static_rva    = root.pointer_rva;
    o.byte_size     = root.byte_size;

    // The OBJECT's identity: a heap instance of a named struct layout.  Derived
    // from the static site, never from the live pointer.
    StaticRef ref;
    ref.build_id      = root.build_id;
    ref.module        = root.module;
    ref.kind          = root.kind;
    ref.struct_name   = o.type_name;
    ref.container_rva = root.pointer_rva;
    ref.offset        = 0;
    o.type_ref  = ref;
    o.identity  = IdentityResolver::resolve(ref);

    if (root.direct_value) {
        o.runtime_va = root.direct_va;
        o.previous_va = root.direct_va;
    }

    m_objects.push_back(std::move(o));
    return m_objects.back().id;
}

void LiveObjectMap::set_fields(uint64_t root_id, std::vector<ObjectField> fields) {
    for (auto& o : m_objects) {
        if (o.id != root_id) continue;
        for (auto& f : fields) {
            if (f.identity.base_name.empty()) f.identity = IdentityResolver::resolve(f.ref);
        }
        o.fields = std::move(fields);
        return;
    }
}

bool LiveObjectMap::set_direct_pointer(uint64_t root_id, uint64_t va) {
    for (auto& o : m_objects) {
        if (o.id != root_id) continue;
        o.previous_va = o.runtime_va;
        o.runtime_va  = va;
        if (o.previous_va && o.previous_va != o.runtime_va) {
            ++o.move_count;
            ++m_total_moves;
        }
        return true;
    }
    return false;
}

size_t LiveObjectMap::refresh(uint8_t* mem, uint64_t mem_size, uint64_t frame) {
    size_t moves = 0;
    for (auto& o : m_objects) {
        // A direct_value root is fed by the engine (SRE's accessor); everything
        // else is a static pointer slot we read ourselves.
        if (o.static_module.empty() && o.static_rva == 0) {
            o.last_seen_frame = frame;
            o.alive = o.runtime_va != 0;
            continue;
        }

        uint64_t va = 0;
        const ModuleExtent* mod = m_modules.find(o.static_module);
        if (!mod) {
            o.alive = false;
            continue;
        }
        const uint64_t slot = mod->base_va + (o.static_rva - mod->rva_begin);
        if (!mem_read_pointer(mem, mem_size, slot, &va) || va == 0) {
            o.alive = false;
            continue;
        }

        o.previous_va = o.runtime_va;
        o.runtime_va  = va;
        o.last_seen_frame = frame;
        o.alive = true;

        if (o.previous_va != 0 && o.previous_va != o.runtime_va) {
            // The object moved.  Identity is untouched — only the runtime side
            // of the pipeline was rewritten.  This is the whole point.
            ++o.move_count;
            ++m_total_moves;
            ++moves;
        }

        // Keep the identity index in step, anchored on the object's own name
        // when it has one, so the tag is deterministic rather than session-random.
        StaticRef fref;
        fref.build_id      = o.type_ref.build_id;
        fref.module        = o.type_ref.module;
        fref.kind          = MemKind::HeapInstance;
        fref.struct_name   = o.type_name;
        fref.container_rva = o.type_ref.container_rva;
        fref.offset        = 0;
        m_index.bind(o.runtime_va, fref, frame, o.type_name);
    }
    return moves;
}

size_t LiveObjectMap::sample_fields(const uint8_t* mem, uint64_t mem_size) {
    if (!mem) return 0;
    size_t sampled = 0;
    for (auto& o : m_objects) {
        if (!o.alive || !o.runtime_va) continue;
        for (auto& f : o.fields) {
            const uint64_t addr = o.runtime_va + f.offset;
            std::string v;
            switch (f.width) {
                case 1: case 2: case 4: case 8: {
                    uint64_t raw = 0;
                    if (!valid_range(addr, f.width, mem_size)) { v = "<oob>"; break; }
                    std::memcpy(&raw, mem + addr, f.width);
                    if (f.type_name == "float" && f.width == 4) {
                        float fl = 0.0f;
                        std::memcpy(&fl, mem + addr, 4);
                        char b[48];
                        std::snprintf(b, sizeof(b), "%g", fl);
                        v = b;
                    } else {
                        v = hex(raw);
                    }
                    break;
                }
                default: {
                    v = hex(addr) + " ?";
                    break;
                }
            }
            f.last_value = v;
            ++sampled;
        }
    }
    return sampled;
}

bool LiveObjectMap::remove_root(uint64_t id) {
    for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
        if (it->id == id) { m_objects.erase(it); return true; }
    }
    return false;
}

size_t LiveObjectMap::size() const { return m_objects.size(); }

std::vector<LiveObject> LiveObjectMap::snapshot() const { return m_objects; }

const LiveObject* LiveObjectMap::find(uint64_t id) const {
    for (const auto& o : m_objects)
        if (o.id == id) return &o;
    return nullptr;
}

const LiveObject* LiveObjectMap::find_by_va(uint64_t va) const {
    for (const auto& o : m_objects)
        if (o.alive && o.runtime_va == va) return &o;
    return nullptr;
}

std::string LiveObjectMap::summary() const {
    size_t alive = 0, moved = 0;
    for (const auto& o : m_objects) {
        if (o.alive) ++alive;
        if (o.move_count) ++moved;
    }
    char b[192];
    std::snprintf(b, sizeof(b),
                  "%zu root(s), %zu alive, %zu have moved, %llu re-resolutions total",
                  m_objects.size(), alive, moved,
                  static_cast<unsigned long long>(m_total_moves));
    return b;
}

} // namespace swordfare::research
