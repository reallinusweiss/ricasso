// =============================================================================
// root_walker.cpp — Bounded traversal of the live object graph from g_gsc
// =============================================================================

#include "game/research/root_walker.h"
#include <thread>
#include <cctype>
#include <algorithm>

namespace swordfare::research {

RootWalker& RootWalker::instance() {
    static RootWalker s_instance;
    return s_instance;
}

bool RootWalker::read_u32(uint64_t va, const uint8_t* m, uint64_t sz, uint32_t& out) {
    if (!m || va < 0x10000 || (va + 4) > sz || (va + 4) > 0xE0000000) {
        return false;
    }
    std::memcpy(&out, m + va, 4);
    return true;
}

bool RootWalker::read_u64(uint64_t va, const uint8_t* m, uint64_t sz, uint64_t& out) {
    if (!m || va < 0x10000 || (va + 8) > sz || (va + 8) > 0xE0000000) {
        return false;
    }
    std::memcpy(&out, m + va, 8);
    return true;
}

std::string RootWalker::decode_sso(uint64_t str_va, const uint8_t* mem, uint64_t mem_size) {
    if (!mem || str_va < 0x10000 || (str_va + 24) > mem_size || (str_va + 24) > 0xE0000000) {
        return "";
    }

    const uint8_t* p = mem + str_va;
    uint8_t flag = p[23];
    if ((flag & 1) == 0) {
        // Short string (libc++: capacity in short mode has LSB=0, size in byte 23 >> 1)
        int sz = (flag >> 1) & 0x7F;
        if (sz < 0 || sz > 23) return "";
        char buf[24] = {};
        std::memcpy(buf, p, static_cast<size_t>(sz));
        for (int i = 0; i < sz; ++i) {
            if (!std::isprint(static_cast<unsigned char>(buf[i]))) {
                buf[i] = '.';
            }
        }
        return std::string(buf, static_cast<size_t>(sz));
    } else {
        // Long string (libc++: ptr at [0..7], size at [8..15])
        uint64_t heap_ptr = 0, sz64 = 0;
        std::memcpy(&heap_ptr, p, 8);
        std::memcpy(&sz64, p + 8, 8);
        if (heap_ptr < 0x10000 || heap_ptr >= 0xE0000000 ||
            (heap_ptr + sz64) > mem_size || (heap_ptr + sz64) > 0xE0000000 ||
            sz64 > 64) {
            return "";
        }
        size_t sz = static_cast<size_t>(sz64);
        std::string result(sz, '\0');
        std::memcpy(result.data(), mem + heap_ptr, sz);
        for (auto& c : result) {
            if (!std::isprint(static_cast<unsigned char>(c))) {
                c = '.';
            }
        }
        return result;
    }
}

void RootWalker::walk_rbtree(uint64_t rb_header_va,
                             const uint8_t* mem, uint64_t mem_size,
                             int max_nodes, WalkResult& out)
{
    if (!mem || rb_header_va < 0x10000 || (rb_header_va + 24) > mem_size || (rb_header_va + 24) > 0xE0000000) {
        out.walk_error = "RB-tree header out of bounds";
        return;
    }

    auto r64 = [&](uint64_t va, uint64_t& val) { return read_u64(va, mem, mem_size, val); };
    auto is_valid_ptr = [mem_size](uint64_t ptr) -> bool {
        return ptr >= 0x10000 && ptr < 0xE0000000 && (ptr + 8) <= mem_size && (ptr + 8) <= 0xE0000000 && (ptr % 8 == 0);
    };

    // libc++ std::__tree header:
    // +0x00: __begin_node_ (leftmost node)
    // +0x08: __end_node_.__left_ (root node)
    // +0x10: __size_
    uint64_t begin_node = 0;
    uint64_t root_node  = 0;
    r64(rb_header_va, begin_node);
    r64(rb_header_va + 8, root_node);

    uint64_t sentinel = rb_header_va + 8;
    std::unordered_set<uint64_t> visited_nodes;
    std::unordered_set<uint64_t> visited_objects;

    for (const auto& obj : out.objects) {
        visited_objects.insert(obj.va);
    }

    int nodes_walked = 0;

    auto process_node = [&](uint64_t node_va) {
        // In ARM64 libc++ map<string, intrusive_ptr<SceneObject>>,
        // node layout is: 32B base (left, right, parent, color) + 24B key (string) + 8B value.
        // SceneObject* is at +0x38. If not valid, fallback to +0x20.
        uint64_t obj_va = 0;
        uint64_t ptr38 = 0, ptr20 = 0;
        r64(node_va + 0x38, ptr38);
        r64(node_va + 0x20, ptr20);

        if (is_valid_ptr(ptr38)) {
            obj_va = ptr38;
        } else if (is_valid_ptr(ptr20)) {
            obj_va = ptr20;
        }

        if (obj_va != 0 && visited_objects.count(obj_va) == 0) {
            visited_objects.insert(obj_va);

            // Decode name: SceneObject+0x50, or map key from node+0x20
            std::string name = decode_sso(obj_va + 0x50, mem, mem_size);
            if (name.empty()) {
                name = decode_sso(node_va + 0x20, mem, mem_size);
            }
            if (name.empty()) {
                name = "SceneObject_" + std::to_string(out.objects.size());
            }

            auto res = VtableClassifier::instance().classify(obj_va, mem, mem_size);
            std::string struct_name = res.matched ? res.struct_name : "SceneObject";

            WalkedObject wo;
            wo.va = obj_va;
            wo.struct_name = struct_name;
            wo.display_name = name;
            wo.depth = 1;

            out.total_walked++;
            if (res.matched) out.classified++; else out.unclassified++;

            // Components vector at obj_va + 0xD0 (std::vector<Component*>)
            uint64_t begin_vec = 0, end_vec = 0;
            std::vector<WalkedObject> comp_objects;
            if (r64(obj_va + 0xD0, begin_vec) && r64(obj_va + 0xD8, end_vec)) {
                if (begin_vec >= 0x10000 && end_vec >= begin_vec &&
                    end_vec <= mem_size && end_vec <= 0xE0000000) {
                    uint64_t num_comps = (end_vec - begin_vec) / 8;
                    if (num_comps > 32) num_comps = 32; // Safety cap: max 32 components

                    for (uint64_t c = 0; c < num_comps; ++c) {
                        uint64_t comp_va = 0;
                        if (r64(begin_vec + c * 8, comp_va) && is_valid_ptr(comp_va)) {
                            wo.component_vas.push_back(comp_va);
                            if (visited_objects.count(comp_va) == 0) {
                                visited_objects.insert(comp_va);
                                auto cres = VtableClassifier::instance().classify(comp_va, mem, mem_size);
                                std::string cstruct = cres.matched ? cres.struct_name : "Component";
                                WalkedObject cwo;
                                cwo.va = comp_va;
                                cwo.struct_name = cstruct;
                                cwo.display_name = cstruct;
                                cwo.depth = 2;
                                out.total_walked++;
                                if (cres.matched) out.classified++; else out.unclassified++;
                                comp_objects.push_back(std::move(cwo));
                            }
                        }
                    }
                }
            }

            out.objects.push_back(std::move(wo));
            for (auto& co : comp_objects) {
                out.objects.push_back(std::move(co));
            }
        }
    };

    // Attempt in-order traversal using begin_node
    uint64_t curr = begin_node;
    if (is_valid_ptr(curr) && curr != sentinel) {
        while (curr != 0 && curr != sentinel && is_valid_ptr(curr)) {
            if (visited_nodes.count(curr) > 0) {
                break; // Cycle detected
            }
            visited_nodes.insert(curr);

            if (nodes_walked >= max_nodes) {
                out.truncated = true;
                break;
            }
            nodes_walked++;

            process_node(curr);

            // In-order advance
            uint64_t right = 0;
            if (!r64(curr + 8, right)) break;

            if (right != 0 && right != sentinel && is_valid_ptr(right)) {
                curr = right;
                uint64_t left = 0;
                while (r64(curr, left) && left != 0 && left != sentinel && is_valid_ptr(left)) {
                    if (visited_nodes.count(left) > 0) break;
                    curr = left;
                }
            } else {
                uint64_t p = 0;
                bool found_succ = false;
                while (r64(curr + 16, p) && p != 0 && p != sentinel && is_valid_ptr(p)) {
                    uint64_t p_left = 0;
                    if (r64(p, p_left) && p_left == curr) {
                        curr = p;
                        found_succ = true;
                        break;
                    }
                    curr = p;
                }
                if (!found_succ) {
                    break;
                }
            }
        }
    }

    // Fallback: if begin_node was invalid/empty but root_node is valid, run DFS from root
    if (nodes_walked == 0 && is_valid_ptr(root_node) && root_node != sentinel) {
        std::vector<uint64_t> stack;
        stack.push_back(root_node);
        while (!stack.empty() && nodes_walked < max_nodes) {
            uint64_t n = stack.back();
            stack.pop_back();

            if (visited_nodes.count(n) > 0) continue;
            visited_nodes.insert(n);
            nodes_walked++;

            process_node(n);

            uint64_t left = 0, right = 0;
            if (r64(n + 8, right) && right != 0 && right != sentinel && is_valid_ptr(right)) {
                stack.push_back(right);
            }
            if (r64(n, left) && left != 0 && left != sentinel && is_valid_ptr(left)) {
                stack.push_back(left);
            }
        }
        if (nodes_walked >= max_nodes) {
            out.truncated = true;
        }
    }
}

WalkResult RootWalker::walk(uint64_t gsc_va,
                            const uint8_t* guest_memory,
                            uint64_t mem_size,
                            int max_objects)
{
    WalkResult out;

    if (!guest_memory || gsc_va < 0x10000 || gsc_va >= 0xE0000000 ||
        (gsc_va + 0x100) > mem_size || (gsc_va + 0x100) > 0xE0000000 || (gsc_va % 8) != 0) {
        out.walk_error = "Invalid g_gsc address or guest memory out of bounds";
        return out;
    }

    if (!VtableClassifier::instance().is_ready()) {
        VtableClassifier::instance().init();
    }

    out.gsc_va = gsc_va;
    std::unordered_set<uint64_t> visited;

    auto r64 = [&](uint64_t va, uint64_t& val) { return read_u64(va, guest_memory, mem_size, val); };
    auto r32 = [&](uint64_t va, uint32_t& val) { return read_u32(va, guest_memory, mem_size, val); };
    auto is_valid_ptr = [mem_size](uint64_t ptr) -> bool {
        return ptr >= 0x10000 && ptr < 0xE0000000 && (ptr + 8) <= mem_size && (ptr + 8) <= 0xE0000000 && (ptr % 8 == 0);
    };

    // 1. Root: GameSceneController (Depth 0)
    visited.insert(gsc_va);
    auto gsc_res = VtableClassifier::instance().classify(gsc_va, guest_memory, mem_size);
    WalkedObject gsc_wo;
    gsc_wo.va = gsc_va;
    gsc_wo.struct_name = gsc_res.matched ? gsc_res.struct_name : "GameSceneController";
    gsc_wo.display_name = "GameSceneController";
    gsc_wo.depth = 0;
    out.objects.push_back(gsc_wo);
    out.total_walked++;
    if (gsc_res.matched) out.classified++; else out.unclassified++;

    // 2. GameState* at gsc_va + 0x08 (shared_ptr ptr word)
    uint64_t gs_va = 0;
    if (r64(gsc_va + 0x08, gs_va) && is_valid_ptr(gs_va)) {
        if (visited.count(gs_va) == 0) {
            visited.insert(gs_va);
            auto gs_res = VtableClassifier::instance().classify(gs_va, guest_memory, mem_size);
            WalkedObject gs_wo;
            gs_wo.va = gs_va;
            gs_wo.struct_name = gs_res.matched ? gs_res.struct_name : "GameState";
            gs_wo.display_name = "GameState";
            gs_wo.depth = 1;
            out.objects.push_back(gs_wo);
            out.total_walked++;
            if (gs_res.matched) out.classified++; else out.unclassified++;

            // Embedded CharacterState at gs_va + 0x10
            uint64_t cs_va = gs_va + 0x10;
            if (is_valid_ptr(cs_va) && visited.count(cs_va) == 0) {
                visited.insert(cs_va);
                auto cs_res = VtableClassifier::instance().classify(cs_va, guest_memory, mem_size);
                WalkedObject cs_wo;
                cs_wo.va = cs_va;
                cs_wo.struct_name = cs_res.matched ? cs_res.struct_name : "CharacterState";
                cs_wo.display_name = "CharacterState";
                cs_wo.depth = 2;
                out.objects.push_back(cs_wo);
                out.total_walked++;
                if (cs_res.matched) out.classified++; else out.unclassified++;
            }
        }
    }

    // 3. Scene* at gsc_va + 0x20
    uint64_t scene_va = 0;
    if (r64(gsc_va + 0x20, scene_va) && is_valid_ptr(scene_va)) {
        out.scene_va = scene_va;
        r32(scene_va + 0x20, reinterpret_cast<uint32_t&>(out.pause_count));

        if (visited.count(scene_va) == 0) {
            visited.insert(scene_va);
            auto sc_res = VtableClassifier::instance().classify(scene_va, guest_memory, mem_size);
            WalkedObject sc_wo;
            sc_wo.va = scene_va;
            sc_wo.struct_name = sc_res.matched ? sc_res.struct_name : "Scene";
            sc_wo.display_name = "Scene";
            sc_wo.depth = 1;
            out.objects.push_back(sc_wo);
            out.total_walked++;
            if (sc_res.matched) out.classified++; else out.unclassified++;
        }

        // Walk RB-tree of objects at scene_va + 0xB8
        walk_rbtree(scene_va + 0xB8, guest_memory, mem_size, max_objects, out);
    }

    // 4. Hero SceneObject* at gsc_va + 0xD8
    uint64_t hero_va = 0;
    if (r64(gsc_va + 0xD8, hero_va) && is_valid_ptr(hero_va)) {
        bool already_present = false;
        for (auto& obj : out.objects) {
            if (obj.va == hero_va) {
                already_present = true;
                if (obj.display_name.find("Hero") == std::string::npos) {
                    obj.display_name = "Hero (" + obj.display_name + ")";
                }
                break;
            }
        }
        if (!already_present) {
            visited.insert(hero_va);
            std::string hname = decode_sso(hero_va + 0x50, guest_memory, mem_size);
            if (hname.empty()) hname = "Hero";

            auto h_res = VtableClassifier::instance().classify(hero_va, guest_memory, mem_size);
            WalkedObject hero_wo;
            hero_wo.va = hero_va;
            hero_wo.struct_name = h_res.matched ? h_res.struct_name : "SceneObject";
            hero_wo.display_name = hname;
            hero_wo.depth = 1;

            out.total_walked++;
            if (h_res.matched) out.classified++; else out.unclassified++;

            // Walk hero components
            uint64_t begin_vec = 0, end_vec = 0;
            std::vector<WalkedObject> hero_comps;
            if (r64(hero_va + 0xD0, begin_vec) && r64(hero_va + 0xD8, end_vec)) {
                if (begin_vec >= 0x10000 && end_vec >= begin_vec &&
                    end_vec <= mem_size && end_vec <= 0xE0000000) {
                    uint64_t num_comps = (end_vec - begin_vec) / 8;
                    if (num_comps > 32) num_comps = 32;

                    for (uint64_t c = 0; c < num_comps; ++c) {
                        uint64_t comp_va = 0;
                        if (r64(begin_vec + c * 8, comp_va) && is_valid_ptr(comp_va)) {
                            hero_wo.component_vas.push_back(comp_va);
                            if (visited.count(comp_va) == 0) {
                                visited.insert(comp_va);
                                auto cres = VtableClassifier::instance().classify(comp_va, guest_memory, mem_size);
                                std::string cstruct = cres.matched ? cres.struct_name : "Component";
                                WalkedObject cwo;
                                cwo.va = comp_va;
                                cwo.struct_name = cstruct;
                                cwo.display_name = cstruct;
                                cwo.depth = 2;
                                out.total_walked++;
                                if (cres.matched) out.classified++; else out.unclassified++;
                                hero_comps.push_back(std::move(cwo));
                            }
                        }
                    }
                }
            }

            out.objects.push_back(std::move(hero_wo));
            for (auto& hc : hero_comps) {
                out.objects.push_back(std::move(hc));
            }
        }
    }

    // 5. HealthComponent* at gsc_va + 0xF0
    uint64_t hc_va = 0;
    if (r64(gsc_va + 0xF0, hc_va) && is_valid_ptr(hc_va)) {
        bool already_present = false;
        for (const auto& obj : out.objects) {
            if (obj.va == hc_va) {
                already_present = true;
                break;
            }
        }
        if (!already_present) {
            visited.insert(hc_va);
            auto hc_res = VtableClassifier::instance().classify(hc_va, guest_memory, mem_size);
            WalkedObject hc_wo;
            hc_wo.va = hc_va;
            hc_wo.struct_name = hc_res.matched ? hc_res.struct_name : "HealthComponent";
            hc_wo.display_name = "Hero HealthComponent";
            hc_wo.depth = 1;
            out.objects.push_back(hc_wo);
            out.total_walked++;
            if (hc_res.matched) out.classified++; else out.unclassified++;
        }
    }

    // 6. ManaComponent* at gsc_va + 0xF8
    uint64_t mc_va = 0;
    if (r64(gsc_va + 0xF8, mc_va) && is_valid_ptr(mc_va)) {
        bool already_present = false;
        for (const auto& obj : out.objects) {
            if (obj.va == mc_va) {
                already_present = true;
                break;
            }
        }
        if (!already_present) {
            visited.insert(mc_va);
            auto mc_res = VtableClassifier::instance().classify(mc_va, guest_memory, mem_size);
            WalkedObject mc_wo;
            mc_wo.va = mc_va;
            mc_wo.struct_name = mc_res.matched ? mc_res.struct_name : "ManaComponent";
            mc_wo.display_name = "Hero ManaComponent";
            mc_wo.depth = 1;
            out.objects.push_back(mc_wo);
            out.total_walked++;
            if (mc_res.matched) out.classified++; else out.unclassified++;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_last = out;
    }

    return out;
}

void RootWalker::request_walk(uint64_t gsc_va,
                              const uint8_t* guest_memory,
                              uint64_t mem_size)
{
    m_pending_gsc_va = gsc_va;
    m_pending_mem    = guest_memory;
    m_pending_mem_sz = mem_size;
    m_pending.store(true, std::memory_order_release);
}

const WalkResult& RootWalker::poll_pending_walk()
{
    if (m_pending.load(std::memory_order_acquire)) {
        if (m_pending_mem && m_pending_gsc_va != 0) {
            walk(m_pending_gsc_va, m_pending_mem, m_pending_mem_sz);
        }
        m_pending.store(false, std::memory_order_release);
    }
    std::lock_guard<std::mutex> lock(m_mu);
    return m_last;
}

} // namespace swordfare::research
