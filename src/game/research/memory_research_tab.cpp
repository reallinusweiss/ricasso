// =============================================================================
// memory_research_tab.cpp — ImGui Memory Research panel implementation
// =============================================================================

#include "game/research/memory_research_tab.h"
#include "imgui/imgui.h"
#include "platform/IconsFontAwesome6.h"
#include "platform/xpera/xpera_gui.h"
#include "game/mod_tools.h"   // g_game_speed — the guest-clock speedhack source
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cinttypes>
#include <iostream>
#include <cstdlib>

#ifndef ICON_FA_SITEMAP
#define ICON_FA_SITEMAP "\xef\x83\xa8" // U+F0E8
#endif

namespace swordfare::research {

// Struct name → live root VA helper indices
static constexpr int kRootGSC       = 0;
static constexpr int kRootHero      = 1;
static constexpr int kRootHealth    = 2;
static constexpr int kRootMana      = 3;
static constexpr int kRootCharCtrl  = 4;

// Root display names
static const char* kRootLabels[5] = {
    "GameSceneController",
    "SceneObject (Hero)",
    "HealthComponent (Hero)",
    "ManaComponent (Hero)",
    "CharControllerComponent (Hero)"
};

static const char* kRootStructNames[5] = {
    "GameSceneController",
    "SceneObject",
    "HealthComponent",
    "ManaComponent",   // not in DB yet — will show as "unknown struct"
    "CharControllerComponent"
};

// ---------------------------------------------------------------------------
// Every table in this console is a research grid.  Which columns matter depends
// on the question being asked, so they are resizable / reorderable / hideable
// rather than nailed to a fixed layout, and SizingFixedFit hugs the content
// until the researcher drags a column.  Exactly one column per table is left to
// stretch ("WidthStretch") so the sliders always have slack to eat.
// ---------------------------------------------------------------------------
static constexpr ImGuiTableFlags kGridFlags =
    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit |
    ImGuiTableFlags_ScrollY;

// An identity or an address rendered for the UI.  Both are full 64-bit values
// with meaningful high bits, so neither is ever truncated.
static std::string hex_identity(uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof(b), "0x%016llX", static_cast<unsigned long long>(v));
    return b;
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void MemoryResearchTab::init(const uint8_t* guest_memory, uint64_t guest_mem_size) {
    if (m_ready) return;

    m_decoder = std::make_unique<StructDecoder>(guest_memory, guest_mem_size);

    VtableClassifier::instance().init();
    RecoverySessionManager::instance().init(guest_memory, guest_mem_size);
    // Register epoch callback: invalidate object tree
    RecoverySessionManager::instance().register_callback([this](uint64_t epoch) {
        (void)epoch;
        m_object_tree.dirty = true;
        m_live_decoded = {}; // reset all live decoded
    });

    // Load the struct list for the browser.  If the catalog is not ready yet
    // (or momentarily returns nothing) this stays unloaded and draw() retries,
    // so an empty catalog can never be silently cached for the whole session.
    refresh_catalog();

    // ── Persistent research workspace ───────────────────────────────────
    // The location is overridable so the research tree can live next to the
    // saves rather than in whatever directory the process happened to start in.
    const char* env_root = std::getenv("SWORDFARE_RESEARCH_DIR");
    open_workspace(env_root && *env_root ? env_root : ".swordfare");

    // ── Register the five live roots as identity-tracked objects ────────
    // They are direct-value roots: the engine gives us the pointer, we never
    // read it from a static slot, and the runtime address is free to change
    // under us without the object's identity changing.
    for (int i = 0; i < 5; ++i) {
        ObjectRoot r;
        r.label        = kRootLabels[i];
        r.struct_name  = kRootLabels[i];
        r.kind         = MemKind::HeapInstance;
        r.direct_value = true;
        r.build_id     = "sre13-1.4.13-arm64";
        m_root_object_id[i] = m_object_map.add_root(r);
    }

    m_ready = true;
}

void MemoryResearchTab::sync_root_pointers() {
    const uint64_t vas[5] = {
        m_roots.gsc_va, m_roots.hero_va, m_roots.health_va,
        m_roots.mana_va, m_roots.char_ctrl_va
    };
    for (int i = 0; i < 5; ++i) {
        if (m_root_object_id[i]) m_object_map.set_direct_pointer(m_root_object_id[i], vas[i]);
    }
}

// ---------------------------------------------------------------------------
// refresh_catalog — idempotent, self-healing catalog load
// ---------------------------------------------------------------------------
void MemoryResearchTab::refresh_catalog() {
    if (m_catalog_loaded) return;
    auto& cat = RecoveryCatalog::instance();
    std::vector<CatalogStruct> structs = cat.all_structs();
    if (structs.empty()) return;              // not ready / empty -> retry later
    m_all_structs    = std::move(structs);
    m_catalog_loaded = true;
    std::cout << "[Research] Catalog loaded: " << m_all_structs.size()
              << " structs" << std::endl;
}

// ---------------------------------------------------------------------------
// draw — called once per frame inside an ImGui window
// ---------------------------------------------------------------------------
bool MemoryResearchTab::push_mono() {
    if (!m_fonts.mono) return false;
    ImGui::PushFont(static_cast<ImFont*>(m_fonts.mono));
    return true;
}

void MemoryResearchTab::pop_mono(bool pushed) {
    if (pushed) ImGui::PopFont();
}

void MemoryResearchTab::draw() {
    if (!m_ready) {
        ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Research tab not initialised");
        return;
    }

    // ── Tool typography ───────────────────────────────────────────────────
    // The console is a reverse-engineering surface, not part of the game.  Its
    // chrome gets a proportional UI face and its numbers get a fixed-width one,
    // instead of inheriting the game's display lettering font.  Pushed once here
    // and popped at the end; numeric regions push mono on top.
    const bool pushed_body = m_fonts.body != nullptr;
    if (pushed_body) ImGui::PushFont(static_cast<ImFont*>(m_fonts.body));

    // Self-heal: the catalog may not have been ready when init() ran (or may
    // have been reloaded).  Retry a few times per second until it yields rows.
    if (!m_catalog_loaded && --m_catalog_retry_in <= 0) {
        m_catalog_retry_in = 30;
        refresh_catalog();
    }

    // Populate the address list from the recovered layout of the live roots, so
    // the panel opens with real identified values rather than an empty table.
    // Idempotent; also re-points the rows when a root is reallocated.
    seed_address_list_from_catalog();

    // Tick session manager with current roots
    RecoverySessionManager::instance().scene_tick(
        m_roots.gsc_va, static_cast<uint64_t>(m_roots.gsc_va) /* placeholder for scene_va */,
        static_cast<uint64_t>(m_roots.hero_va));
    // Poll auto-walk
    if (m_auto_walk && ++m_walk_throttle_frames >= kWalkInterval) {
        m_walk_throttle_frames = 0;
        if (m_roots.gsc_va != 0 && m_decoder) {
            RootWalker::instance().request_walk(
                static_cast<uint64_t>(m_roots.gsc_va),
                m_decoder->guest_memory(), m_decoder->guest_mem_size());
        }
    }
    RootWalker::instance().poll_pending_walk(); // executes synchronously if pending

    // ── Nav rail (Xpera) instead of a tab strip ──────────────────────────
    const float nav_h = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##research_nav", ImVec2(196.0f, nav_h), ImGuiChildFlags_None);
    if (xpera::nav_item(ICON_FA_BOOK_OPEN "  Catalog",        nullptr, m_active_section == 0)) m_active_section = 0;
    if (xpera::nav_item(ICON_FA_EYE "  Live Inspector",       nullptr, m_active_section == 1)) m_active_section = 1;
    if (xpera::nav_item(ICON_FA_SITEMAP "  Object Tree",      nullptr, m_active_section == 2)) m_active_section = 2;
#if SWORDFARE_HAVE_HEX_INSPECTOR
    if (xpera::nav_item(ICON_FA_MAGNIFYING_GLASS "  Hex View", nullptr, m_active_section == 3)) m_active_section = 3;
#endif
    if (xpera::nav_item(ICON_FA_CROSSHAIRS "  Watchpoints",   nullptr, m_active_section == 4)) m_active_section = 4;
    if (xpera::nav_item(ICON_FA_DATABASE "  DB Status",       nullptr, m_active_section == 5)) m_active_section = 5;
    xpera::divider(0.5f);
    if (xpera::nav_item(ICON_FA_MAGNIFYING_GLASS "  Scanner",  nullptr, m_active_section == 6)) m_active_section = 6;
    if (xpera::nav_item(ICON_FA_LIST "  Address List",        nullptr, m_active_section == 7)) m_active_section = 7;
    if (xpera::nav_item(ICON_FA_CODE "  Instruction Evidence", nullptr, m_active_section == 8)) m_active_section = 8;
    if (xpera::nav_item(ICON_FA_LIGHTBULB "  Research Leads",   nullptr, m_active_section == 9)) m_active_section = 9;
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 16.0f);

    ImGui::BeginChild("##research_content", ImVec2(0.0f, nav_h), ImGuiChildFlags_None);
    switch (m_active_section) {
        case 0:  draw_catalog_browser(); break;
        case 1:  draw_live_inspector();  break;
        case 2:  draw_object_tree();     break;
#if SWORDFARE_HAVE_HEX_INSPECTOR
        case 3:  draw_hex_view();        break;
#endif
        case 4:  draw_watchpoints();     break;
        case 5:  draw_db_status();       break;
        case 6:  draw_scanner();         break;
        case 7:  draw_address_list();    break;
        case 8:  draw_evidence();        break;
        case 9:  draw_leads();           break;
        default: draw_catalog_browser(); break;
    }
    ImGui::EndChild();

    // Balance the body font pushed at the top of draw().
    if (pushed_body) ImGui::PopFont();
}

// ---------------------------------------------------------------------------
// draw_catalog_browser
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_catalog_browser() {
    // Left: struct list
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##sfilt", "filter structs...", m_struct_filter,
                              sizeof(m_struct_filter));
    ImGui::BeginChild("##slist", ImVec2(190, -1), true);
    for (int i = 0; i < (int)m_all_structs.size(); ++i) {
        const auto& s = m_all_structs[i];
        if (m_struct_filter[0] != '\0' &&
            s.name.find(m_struct_filter) == std::string::npos)
            continue;
        bool sel = (m_selected_struct == i);
        char label[80];
        snprintf(label, sizeof(label), "%-30s  %4u B", s.name.c_str(), s.size_bytes);
        if (ImGui::Selectable(label, sel)) {
            m_selected_struct = i;
            // Load related data
            auto& cat = RecoveryCatalog::instance();
            m_selected_fields  = cat.fields_for_struct(s.id);
            m_selected_rels    = cat.relationships_from(s.id);
            m_selected_vtable  = cat.vtable_for_struct(s.id, true);
            m_selected_proto   = cat.proto_fields_for_struct(s.id);
        }
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine();

    // Right: details
    ImGui::BeginGroup();
    if (m_selected_struct >= 0 && m_selected_struct < (int)m_all_structs.size()) {
        const auto& s = m_all_structs[m_selected_struct];

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.62f, 0.26f, 1.0f));
        ImGui::Text("struct %s  {  // %u bytes", s.name.c_str(), s.size_bytes);
        ImGui::PopStyleColor();

        if (!s.source_file.empty()) {
            ImGui::TextDisabled("  // from: %s", s.source_file.c_str());
        }
        ImGui::Spacing();

        // ── Fields ────────────────────────────────────────────────────────
        if (!m_selected_fields.empty()) {
            if (ImGui::CollapsingHeader("Fields", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::BeginTable("##fields", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                        ImVec2(0, 180))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Offset64",   ImGuiTableColumnFlags_WidthFixed, 70);
                    ImGui::TableSetupColumn("Offset32",   ImGuiTableColumnFlags_WidthFixed, 70);
                    ImGui::TableSetupColumn("Type",       ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Name",       ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Confidence", ImGuiTableColumnFlags_WidthFixed, 72);
                    ImGui::TableHeadersRow();
                    for (const auto& f : m_selected_fields) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextDisabled("+0x%03X", f.offset_arm64);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextDisabled("+0x%03X", f.offset_arm32);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(f.field_type.c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.90f, 1.0f, 1.0f));
                        ImGui::TextUnformatted(f.field_name.c_str());
                        ImGui::PopStyleColor();
                        ImGui::TableSetColumnIndex(4);
                        ImVec4 cc = (f.confidence == "KNOWN")    ? ImVec4(0.3f, 0.9f, 0.5f, 1) :
                                    (f.confidence == "INFERRED") ? ImVec4(1.0f, 0.8f, 0.2f, 1) :
                                                                    ImVec4(0.9f, 0.3f, 0.3f, 1);
                        ImGui::PushStyleColor(ImGuiCol_Text, cc);
                        ImGui::TextUnformatted(f.confidence.c_str());
                        ImGui::PopStyleColor();
                    }
                    ImGui::EndTable();
                }
            }
        }

        // ── Vtable ────────────────────────────────────────────────────────
        if (m_selected_vtable) {
            const auto& vt = *m_selected_vtable;
            char vthdr[80];
            snprintf(vthdr, sizeof(vthdr), "Vtable  vptr64=0x%08llX  (%d slots)",
                     (unsigned long long)vt.vptr_arm64, vt.slot_count);
            if (ImGui::CollapsingHeader(vthdr)) {
                for (const auto& sl : vt.slots) {
                    char slabel[128];
                    snprintf(slabel, sizeof(slabel),
                             "[%2d] 0x%08llX  %s",
                             sl.slot_index,
                             (unsigned long long)sl.vaddr_arm64,
                             sl.symbol_name.c_str());
                    ImGui::TextDisabled("%s", slabel);
                }
            }
        }

        // ── Protobuf fields ────────────────────────────────────────────────
        if (!m_selected_proto.empty()) {
            if (ImGui::CollapsingHeader("Protobuf Fields")) {
                for (const auto& pf : m_selected_proto) {
                    char line[128];
                    snprintf(line, sizeof(line),
                             "tag=%2d  %-16s  %s",
                             pf.tag, pf.cpp_field.c_str(), pf.proto_name.c_str());
                    ImGui::BulletText("%s", line);
                }
            }
        }

        // ── Relationships ────────────────────────────────────────────────
        if (!m_selected_rels.empty()) {
            if (ImGui::CollapsingHeader("Relationships (from)")) {
                for (const auto& r : m_selected_rels) {
                    ImGui::BulletText("+0x%03X  [%s]  → %s",
                                      r.offset_arm64,
                                      r.kind.c_str(),
                                      r.to_name.c_str());
                }
            }
        }

        // Notes
        if (!s.notes.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.60f, 0.68f, 1.0f));
            ImGui::TextWrapped("// %s", s.notes.c_str());
            ImGui::PopStyleColor();
        }
    } else if (m_all_structs.empty()) {
        // Never fail silently: say *why* the catalog is empty.
        auto& cat = RecoveryCatalog::instance();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.45f, 0.35f, 1.0f));
        ImGui::TextUnformatted(cat.is_ready()
            ? "Embedded recovery DB opened but returned no structs"
            : "Embedded recovery DB is not initialised");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextWrapped("The catalog is read via sqlite3_deserialize from the "
                           "DB compiled into the binary. Retrying automatically "
                           "every 30 frames.");
        if (cat.is_ready()) {
            ImGui::Spacing();
            ImGui::TextDisabled("%s", cat.status_summary().c_str());
        }
    } else {
        ImGui::TextDisabled("← Select a struct");
    }
    ImGui::EndGroup();
}

// ---------------------------------------------------------------------------
// draw_live_inspector
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_live_inspector() {
    auto& cat = RecoveryCatalog::instance();

    // Map root index → current VA (ARM32 → extended to 64 for the decoder)
    uint64_t root_vas[5] = {
        (uint64_t)m_roots.gsc_va,
        (uint64_t)m_roots.hero_va,
        (uint64_t)m_roots.health_va,
        (uint64_t)m_roots.mana_va,
        (uint64_t)m_roots.char_ctrl_va,
    };

    for (int ri = 0; ri < 5; ++ri) {
        uint64_t va = root_vas[ri];
        const char* struct_name = kRootStructNames[ri];

        ImGui::PushID(ri);
        ImVec4 header_col = (va != 0) ? ImVec4(0.30f, 0.90f, 0.50f, 1.0f)
                                       : ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, header_col);
        char hdr[128];
        if (va != 0)
            snprintf(hdr, sizeof(hdr), ICON_FA_CIRCLE_DOT "  %s  @  0x%08llX###ri%d",
                     kRootLabels[ri], (unsigned long long)va, ri);
        else
            snprintf(hdr, sizeof(hdr), ICON_FA_CIRCLE_XMARK "  %s  (not yet observed)###ri%d",
                     kRootLabels[ri], ri);
        bool open = ImGui::CollapsingHeader(hdr);
        ImGui::PopStyleColor();

        if (open && va != 0) {
            auto s_opt = cat.find_struct(struct_name);
            if (!s_opt) {
                ImGui::TextDisabled("  Struct '%s' not in catalog", struct_name);
            } else {
                // Reload fields if VA changed
                auto& ld = m_live_decoded[ri];
                if (ld.last_va != va || ld.struct_name != struct_name ||
                    ld.fields.empty()) {
                    ld.struct_name = struct_name;
                    ld.last_va     = va;
                    auto fields = cat.fields_for_struct(s_opt->id);
                    ld.fields = m_decoder->decode(va, *s_opt, fields);
                }

                // Draw compact field table
                if (ImGui::BeginTable("##live_fields", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                        ImVec2(0, 160))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Field",  ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 60);
                    ImGui::TableSetupColumn("Value",  ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();
                    for (const auto& df : ld.fields) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.90f, 1.0f, 1.0f));
                        ImGui::TextUnformatted(df.field_name.c_str());
                        ImGui::PopStyleColor();
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextDisabled("+0x%03X", df.offset_arm64);
                        ImGui::TableSetColumnIndex(2);
                        if (!df.in_range) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f,0.3f,0.3f,1));
                            ImGui::TextUnformatted("<OOB>");
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::TextUnformatted(df.value_str.c_str());
                        }
                    }
                    ImGui::EndTable();
                }

                // Refresh button
                if (ImGui::SmallButton("Refresh")) {
                    auto s2 = cat.find_struct(struct_name);
                    if (s2) {
                        auto fields = cat.fields_for_struct(s2->id);
                        m_live_decoded[ri].fields = m_decoder->decode(va, *s2, fields);
                    }
                }
                ImGui::SameLine();
                // Record observation
                if (ImGui::SmallButton("Record in user DB")) {
                    cat.record_runtime_observation(struct_name, va,
                        "Manually recorded from Live Inspector");
                }
            }
        }
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------
// draw_watchpoints
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_watchpoints() {
    auto& wpm = MemWatchpointManager::instance();

    // ── Add new watchpoint ─────────────────────────────────────────────────
    ImGui::SeparatorText("Add Watchpoint");
    ImGui::SetNextItemWidth(120);
    ImGui::InputScalar("VA (hex)", ImGuiDataType_U32, &m_wp_add_va_u32,
                       nullptr, nullptr, "%08X",
                       ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputScalar("Size", ImGuiDataType_U32, &m_wp_add_size);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    ImGui::InputText("Label", m_wp_add_label, sizeof(m_wp_add_label));

    ImGui::Checkbox("Write", &m_wp_add_write);
    ImGui::SameLine();
    ImGui::Checkbox("Read", &m_wp_add_read);
    ImGui::SameLine();
    if (ImGui::Button("Add##wp_add")) {
        if (m_wp_add_va_u32 != 0 && m_wp_add_size > 0) {
            wpm.add((uint64_t)m_wp_add_va_u32, m_wp_add_size,
                    m_wp_add_label[0] ? m_wp_add_label : "unnamed",
                    m_wp_add_write, m_wp_add_read);
        }
    }

    // ── Active watchpoints ─────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::SeparatorText("Active Watchpoints");
    auto wps = wpm.list();
    if (wps.empty()) {
        ImGui::TextDisabled("No active watchpoints");
    } else {
        if (ImGui::BeginTable("##wptable", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("ID",     ImGuiTableColumnFlags_WidthFixed, 36);
            ImGui::TableSetupColumn("Range",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size",   ImGuiTableColumnFlags_WidthFixed, 48);
            ImGui::TableSetupColumn("Flags",  ImGuiTableColumnFlags_WidthFixed, 48);
            ImGui::TableSetupColumn("Label",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (const auto& wp : wps) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", wp.id);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%08llX", (unsigned long long)wp.va_start);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%llu", (unsigned long long)(wp.va_end - wp.va_start));
                ImGui::TableSetColumnIndex(3);
                char flags[8];
                snprintf(flags, sizeof(flags), "%s%s",
                         wp.watch_write ? "W" : "-",
                         wp.watch_read  ? "R" : "-");
                ImGui::TextUnformatted(flags);
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(wp.label);
                ImGui::SameLine();
                ImGui::PushID((int)wp.id);
                if (ImGui::SmallButton("X")) wpm.remove(wp.id);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    // ── Hit log ───────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::SeparatorText("Hit Log");

    // Drain new hits this frame
    int new_hits = wpm.drain_hits(m_hit_buf.data(), kHitBufSize);
    for (int i = 0; i < new_hits; ++i) {
        m_hit_log.push_back(m_hit_buf[i]);
        if (m_hit_log.size() > 2048)
            m_hit_log.erase(m_hit_log.begin());
    }

    char stats[64];
    snprintf(stats, sizeof(stats), "Total hits: %" PRIu64, wpm.total_hits());
    ImGui::TextDisabled("%s", stats);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        wpm.clear_hits();
        m_hit_log.clear();
    }

    ImGui::BeginChild("##hitlog", ImVec2(0, 200), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    // Show newest-first
    for (int i = (int)m_hit_log.size() - 1; i >= 0 && i >= (int)m_hit_log.size() - 200; --i) {
        const auto& h = m_hit_log[i];
        char line[128];
        snprintf(line, sizeof(line),
                 "#%-6llu  wp=%u  %s  VA=0x%08llX  val=0x%016llX  PC=0x%08llX  (%uB)",
                 (unsigned long long)h.seq,
                 h.wp_id,
                 h.is_write ? "W" : "R",
                 (unsigned long long)h.va,
                 (unsigned long long)h.value,
                 (unsigned long long)h.guest_pc,
                 h.access_size);
        ImGui::TextUnformatted(line);
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// draw_db_status
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_db_status() {
    auto& cat = RecoveryCatalog::instance();

    // Lazy-load stats
    if (!m_stats_loaded) {
        m_table_stats  = cat.table_stats();
        m_stats_loaded = true;
    }

    ImGui::SeparatorText("Embedded Recovery DB");
    ImGui::TextUnformatted(cat.status_summary().c_str());

    ImGui::Spacing();
    ImGui::SeparatorText("Table Statistics");
    if (ImGui::BeginTable("##dbstats", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Table", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Rows",  ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();
        for (const auto& ts : m_table_stats) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(ts.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", ts.row_count);
        }
        ImGui::EndTable();
    }

    if (ImGui::SmallButton("Reload Stats")) {
        m_stats_loaded = false;
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Watchpoint System");
    auto& wpm = MemWatchpointManager::instance();
    ImGui::BulletText("Active watchpoints: %d / %d",
                      (int)wpm.list().size(), kMaxWatchpoints);
    ImGui::BulletText("Ring buffer capacity: %d hits", kHitRingCapacity);
    ImGui::BulletText("Total hits recorded: %" PRIu64, wpm.total_hits());
}

// ---------------------------------------------------------------------------
// draw_object_tree
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_object_tree() {
    using ull = unsigned long long;

    // Show epoch + session log from RecoverySessionManager
    ImGui::Text("Epoch: %llu  Frame: %llu",
        static_cast<ull>(RecoverySessionManager::instance().current_epoch()),
        static_cast<ull>(RecoverySessionManager::instance().frame_counter()));
    
    auto log = RecoverySessionManager::instance().session_log();
    if (ImGui::CollapsingHeader("Session Log")) {
        for (auto it = log.rbegin(); it != log.rend(); ++it) {
            ImGui::BulletText("[E%llu F%llu] %s",
                static_cast<ull>(it->epoch),
                static_cast<ull>(it->frame),
                it->reason.c_str());
        }
    }
    ImGui::Separator();
    
    // Walk controls
    ImGui::Checkbox("Auto-walk every 30 frames", &m_auto_walk);
    ImGui::SameLine();
    if (ImGui::Button("Walk Now")) {
        if (m_roots.gsc_va != 0 && m_decoder) {
            m_object_tree.last_walk = RootWalker::instance().walk(
                static_cast<uint64_t>(m_roots.gsc_va),
                m_decoder->guest_memory(), m_decoder->guest_mem_size());
            m_object_tree.dirty = false;
        }
    }
    
    const auto& wr = RootWalker::instance().last_result();
    ImGui::Text("Objects: %d  Classified: %d  Truncated: %s",
        wr.total_walked, wr.classified, wr.truncated ? "YES" : "no");
    // Classification health: how many known vtable keys are actually valid for
    // the binary being run, plus the vptr RVAs that could not be classified —
    // this is what tells you whether the catalog's vtable data matches reality.
    ImGui::TextDisabled("Classify: %s",
        VtableClassifier::instance().probe_summary().c_str());
    if (!wr.walk_error.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", wr.walk_error.c_str());
    }
    
    // Object list
    ImGui::BeginChild("##objtree", ImVec2(0, -1), true);
    for (const auto& obj : wr.objects) {
        ImGui::PushID(static_cast<int>(obj.va & 0x7FFFFFFF));
        char label[256];
        std::snprintf(label, sizeof(label), "%*s[%s] 0x%08llX  %s",
                      obj.depth * 2, "",
                      obj.struct_name.c_str(),
                      static_cast<ull>(obj.va),
                      obj.display_name.c_str());
        bool sel = (m_hex_target_va == obj.va);
        if (ImGui::Selectable(label, sel)) {
            // Drive the Hex Inspector: store VA + fetch catalog fields for overlay
            m_hex_target_va   = obj.va;
            m_hex_target_type = obj.struct_name;
            m_hex_target_fields.clear();
            m_hex_target_struct.reset();

            // Try to find this struct in the catalog and load its fields
            for (const auto& s : m_all_structs) {
                if (s.name == obj.struct_name) {
                    m_hex_target_struct = s;
                    m_hex_target_fields = RecoveryCatalog::instance().fields_for_struct(s.id);
                    break;
                }
            }

#if SWORDFARE_HAVE_HEX_INSPECTOR
            m_hex_inspector.set_address(
                obj.va,
                m_hex_target_struct.has_value() ? &m_hex_target_struct.value() : nullptr,
                m_hex_target_fields.empty()     ? nullptr : &m_hex_target_fields);
#endif
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("VA: 0x%08llX\nType: %s\nComponents: %d",
                              static_cast<ull>(obj.va),
                              obj.struct_name.c_str(),
                              static_cast<int>(obj.component_vas.size()));
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// draw_hex_view — live hex dump with struct field overlay
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_hex_view() {
#if SWORDFARE_HAVE_HEX_INSPECTOR
    if (!m_decoder) {
        ImGui::TextDisabled("Not initialized.");
        return;
    }

    // Target info bar
    if (m_hex_target_va != 0) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f),
            "Target: %s @ 0x%08llX",
            m_hex_target_type.empty() ? "?" : m_hex_target_type.c_str(),
            (unsigned long long)m_hex_target_va);
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            m_hex_target_va = 0;
            m_hex_target_type.clear();
            m_hex_target_fields.clear();
            m_hex_target_struct.reset();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("  %d fields overlaid",
            (int)m_hex_target_fields.size());
    } else {
        ImGui::TextDisabled("Click an object in the Object Tree to inspect it here.");
    }

    // Column toggle
    ImGui::SameLine(0, 20);
    int cols = m_hex_inspector.columns();
    if (ImGui::RadioButton("8B", cols == 8))  { m_hex_inspector.set_columns(8);  }
    ImGui::SameLine();
    if (ImGui::RadioButton("16B", cols == 16)) { m_hex_inspector.set_columns(16); }

    ImGui::Separator();

    // Highlight active watchpoints in the hex view
    m_hex_inspector.clear_highlights();
    const auto wps = MemWatchpointManager::instance().list();
    for (const auto& wp : wps) {
        if (wp.active) {
            m_hex_inspector.highlight(wp.va_start,
                                      (uint32_t)(wp.va_end - wp.va_start),
                                      ImVec4(1.0f, 0.8f, 0.0f, 0.5f)); // amber
        }
    }

    m_hex_inspector.draw(m_decoder->guest_memory(), m_decoder->guest_mem_size());
#else
    ImGui::TextDisabled("HexInspector not compiled (hex_inspector.h not found).");
#endif
}

// ===========================================================================
// Scanner / Address List / Instruction Evidence
// ===========================================================================
namespace {

const char* kValueTypeNames[] = {
    "1 Byte", "2 Bytes", "4 Bytes", "8 Bytes", "Float", "Double", "String", "Array of Bytes"
};
const ValueType kValueTypes[] = {
    ValueType::Byte, ValueType::Word, ValueType::Dword, ValueType::Qword,
    ValueType::Float, ValueType::Double, ValueType::Str, ValueType::AoB
};
constexpr int kValueTypeCount = 8;

const char* kScanKindNames[] = {
    "Exact Value", "Unknown Initial Value", "Increased Value", "Increased By",
    "Decreased Value", "Decreased By", "Changed Value", "Unchanged Value",
    "Bigger Than", "Smaller Than", "Value Between", "Not Equal To"
};
const ScanType kScanKinds[] = {
    ScanType::Exact, ScanType::UnknownInitial, ScanType::Increased, ScanType::IncreasedBy,
    ScanType::Decreased, ScanType::DecreasedBy, ScanType::Changed, ScanType::Unchanged,
    ScanType::BiggerThan, ScanType::SmallerThan, ScanType::Between, ScanType::NotEqual
};
constexpr int kScanKindCount = 12;

// "0x71A50090" / "71A50090" / "1000" -> true + value
bool parse_address(const char* text, uint64_t* out) {
    if (!text || !*text || !out) return false;
    std::string t = text;
    if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) t = t.substr(2);
    if (t.empty()) return false;
    char* end = nullptr;
    const unsigned long long v = std::strtoull(t.c_str(), &end, 16);
    if (!end || *end != '\0') return false;
    *out = v;
    return true;
}

ImVec4 provenance_colour(Provenance p) {
    switch (p) {
        case Provenance::Confirmed:  return ImVec4(0.42f, 0.90f, 0.55f, 1.0f);
        case Provenance::Recovered:  return ImVec4(0.55f, 0.85f, 0.60f, 1.0f);
        case Provenance::Correlated: return ImVec4(0.45f, 0.75f, 0.95f, 1.0f);
        case Provenance::Inferred:   return ImVec4(0.85f, 0.72f, 0.40f, 1.0f);
        case Provenance::Observed:   return ImVec4(0.75f, 0.62f, 0.85f, 1.0f);
        case Provenance::Unknown:    break;
    }
    return ImVec4(0.55f, 0.58f, 0.65f, 1.0f);
}

const char* tier_label(Provenance p) {
    if (provenance_is_proven(p)) return "KNOWN";
    if (p == Provenance::Unknown) return "UNMAPPED";
    return "UNKNOWN-BUT-IDENTIFIABLE";
}

} // namespace

void MemoryResearchTab::open_workspace(const std::string& root) {
    std::string err;
    if (!m_workspace.open(root, &err)) {
        m_workspace_status = "workspace unavailable: " + err +
                             "  (research will NOT persist this session)";
        m_workspace_checked = true;
        return;
    }
    m_session_seed = IdentityResolver::stable_tag_value(root) * 2654435761ull + 12345ull;
    m_workspace.symbols().size();   // ensure loaded
    (void)m_workspace.begin_session(m_session_seed, &err);
    m_workspace_checked = true;
    m_recovery_banner = m_workspace.latest_session_summary();
    m_workspace_status = m_workspace.summary();
}

// ---------------------------------------------------------------------------
// empty_state — every list explains itself
// ---------------------------------------------------------------------------
void MemoryResearchTab::empty_state(const char* headline, const char* detail) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.65f, 1.0f), "%s", headline);
    if (detail && *detail) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 620.0f);
        ImGui::TextDisabled("%s", detail);
        ImGui::PopTextWrapPos();
    }
    ImGui::Spacing();
}

bool MemoryResearchTab::module_scan_range(uint64_t* begin, uint64_t* end) const {
    const auto& mods = m_object_map.modules();
    if (mods.modules().empty()) return false;
    uint64_t lo = UINT64_MAX, hi = 0;
    for (const auto& m : mods.modules()) {
        lo = std::min(lo, m.base_va + m.rva_begin);
        hi = std::max(hi, m.base_va + m.rva_end + 1);
    }
    if (lo >= hi) return false;
    if (begin) *begin = lo;
    if (end)   *end   = hi;
    return true;
}

std::string MemoryResearchTab::address_list_diagnosis() const {
    const uint64_t root_vas[5] = {
        m_roots.gsc_va, m_roots.hero_va, m_roots.health_va,
        m_roots.mana_va, m_roots.char_ctrl_va
    };

    std::string why;
    int live = 0;
    for (int i = 0; i < 5; ++i) if (root_vas[i]) ++live;

    if (m_all_structs.empty()) {
        why += "The recovery catalog has not loaded (0 structs), so no layout is known to seed "
               "from.  ";
    }
    if (live == 0) {
        why += "No live root pointer has been observed yet, so there is nothing to attach a "
               "layout to.  Roots come from the running game: boot with ARM64 + SRE and reach "
               "in-game (title screen and menus do not create a GameSceneController).  ";
    } else {
        why += std::to_string(live) + " of 5 live root pointer(s) resolved.  ";
        std::string missing;
        for (int i = 0; i < 5; ++i) {
            if (!root_vas[i]) continue;
            bool found = false;
            for (const auto& s : m_all_structs)
                if (s.name == kRootStructNames[i]) { found = true; break; }
            if (!found) {
                if (!missing.empty()) missing += ", ";
                missing += kRootStructNames[i];
            }
        }
        if (!missing.empty())
            why += "No recovered layout exists for: " + missing +
                   " (that is a gap in the recovery database, not a bug here).  ";
    }
    if (m_seeded_structs.empty() && !m_all_structs.empty() && live > 0)
        why += "Roots and layouts are both present but nothing was seeded \u2014 please report this.";
    if (why.empty())
        why = "Seeding is waiting on a live root pointer; this will populate itself in game.";
    return why;
}

// ---------------------------------------------------------------------------
// seed_address_list_from_catalog — the recovered fields of the LIVE roots.
//
// This is what makes the panel useful the moment it opens: the objects the
// engine already resolved get their recovered layout turned into address-list
// rows, each carrying its deterministic identity, so a value that is genuinely
// known shows up as "currentHealth" and one that is merely inferred shows up as
// VAR_#### with its tier visible.  Nothing here invents a name.
// ---------------------------------------------------------------------------
void MemoryResearchTab::seed_address_list_from_catalog() {
    if (m_all_structs.empty()) return;

    const uint64_t root_vas[5] = {
        m_roots.gsc_va, m_roots.hero_va, m_roots.health_va,
        m_roots.mana_va, m_roots.char_ctrl_va
    };

    for (int r = 0; r < 5; ++r) {
        if (!root_vas[r]) continue;

        // The catalog key is the STRUCT name (kRootStructNames), not the UI
        // label ("HealthComponent (Hero)") — matching on the label silently
        // matched almost nothing.
        const CatalogStruct* st = nullptr;
        for (const auto& s : m_all_structs) {
            if (s.name == kRootStructNames[r]) { st = &s; break; }
        }
        if (!st) continue;   // e.g. ManaComponent is genuinely not in the DB yet
        if (std::find(m_seeded_structs.begin(), m_seeded_structs.end(), st->id) !=
            m_seeded_structs.end())
            continue;

        const std::vector<CatalogField> fields =
            RecoveryCatalog::instance().fields_for_struct(st->id);
        if (fields.empty()) continue;

        for (const auto& f : fields) {
            if (f.size_bytes == 0 || f.size_bytes > 8) continue;
            if (f.offset_arm64 == 0) continue;   // offset 0 is the vptr slot

            StaticRef ref;
            ref.build_id      = st->build.empty() ? "sre13-1.4.13-arm64" : st->build;
            ref.module        = "libswordigo.so";
            ref.kind        = MemKind::StructField;
            ref.struct_name = st->name;
            // No fabricated address: the DB row index is NOT a module RVA, and
            // pretending otherwise would corrupt every canonical key it feeds.
            // Identity is already fully determined by struct_name + offset +
            // type, all of which are static facts.
            ref.container_rva = 0;
            ref.offset        = f.offset_arm64;
            ref.type_name     = f.field_type;
            if (f.confidence == "KNOWN") {
                ref.recovered_name = f.field_name;
                ref.provenance     = Provenance::Recovered;
            } else if (f.confidence == "INFERRED") {
                ref.recovered_name = f.field_name;
                ref.provenance     = Provenance::Inferred;
            } else {
                ref.provenance     = Provenance::Observed;
            }

            ValueType vt = ValueType::Dword;
            std::string t = f.field_type;
            std::transform(t.begin(), t.end(), t.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (t.find("float") != std::string::npos && f.size_bytes == 4) vt = ValueType::Float;
            else if (t.find("double") != std::string::npos && f.size_bytes == 8) vt = ValueType::Double;
            else if (f.size_bytes == 1) vt = ValueType::Byte;
            else if (f.size_bytes == 2) vt = ValueType::Word;
            else if (f.size_bytes == 4) vt = ValueType::Dword;
            else if (f.size_bytes == 8) vt = ValueType::Qword;

            const uint64_t va = root_vas[r] + f.offset_arm64;
            m_addresses.add(ref, va, vt, st->name,
                            root_vas[r] + f.offset_arm64 + f.size_bytes);
        }
        m_seeded_structs.push_back(st->id);
        m_seeded_root_va[r] = root_vas[r];
        m_seeded_groups[r] = st->name;
    }

    // A live root can be reallocated (scene reload).  Its identity does not
    // change, so every seeded row in that group is re-pointed to the new base
    // rather than being re-created or, worse, silently left dangling.
    for (int r = 0; r < 5; ++r) {
        if (!root_vas[r] || !m_seeded_root_va[r]) continue;
        if (root_vas[r] == m_seeded_root_va[r]) continue;
        const int64_t delta = static_cast<int64_t>(root_vas[r]) -
                              static_cast<int64_t>(m_seeded_root_va[r]);
        for (const AddressEntry& row : m_addresses.snapshot()) {
            if (row.group != m_seeded_groups[r]) continue;
            m_addresses.rebind(row.id, static_cast<uint64_t>(
                static_cast<int64_t>(row.runtime_va) + delta));
        }
        m_seeded_root_va[r] = root_vas[r];
    }
}

// ---------------------------------------------------------------------------
// explore_tick — feed the explorer, on the emulated frame.
//
// The two signals it correlates against are the scene (a re-created
// GameSceneController) and object rebuilds (a tracked object moving).  Both are
// events the engine already detects for the identity layer, so the explorer
// needs no new hooks to be useful.
// ---------------------------------------------------------------------------
void MemoryResearchTab::explore_tick(uint8_t* guest_memory, uint64_t guest_mem_size, uint64_t frame) {
    if (!m_explore_enabled || !guest_memory) return;
    if (frame < m_explore_last_frame + m_explore_every_frames) return;
    m_explore_last_frame = frame;

    // Configure once, or whenever the caller changes the region/type: a reshape
    // invalidates the sample baseline and must go through configure().
    if (!m_explore_started) {
        ExploreConfig c = m_explorer.config();
        if (m_explore_type >= 0 && m_explore_type < kValueTypeCount)
            c.type = kValueTypes[m_explore_type];
        uint64_t rlo = 0, rhi = 0;
        if (module_scan_range(&rlo, &rhi)) {
            c.region_begin = rlo;
            c.region_end   = rhi;
            m_explore_region_begin = rlo;
            m_explore_region_end   = rhi;
        }
        if (c.region_end <= c.region_begin) return;   // nothing to explore yet
        m_explorer.configure(c);
        m_explore_started = true;
    }
    (void)guest_mem_size;

    // ── Signals ──────────────────────────────────────────────────────────
    // A scene change is a new GameSceneController address: the whole scene was
    // torn down and rebuilt, so anything scene-scoped should have moved.
    const uint64_t gsc = m_seeded_root_va[0];
    bool scene_signal = false;
    if (gsc && m_explore_scene_epoch && gsc != m_explore_scene_epoch) scene_signal = true;
    if (gsc) m_explore_scene_epoch = gsc;

    const uint64_t moves = m_object_map.total_moves();
    const bool rebuild_signal = (moves != m_explore_last_moves);
    m_explore_last_moves = moves;

    // ── Claimed set ──────────────────────────────────────────────────────
    // Everything already identified, so "unclaimed" means exactly unclaimed:
    //   * every tracked live object's byte range
    //   * every address-list row that knows its field extent
    //   * every live root pointer itself
    std::vector<std::pair<uint64_t, uint64_t>> claimed;
    for (const auto& o : m_object_map.snapshot()) {
        if (!o.alive || !o.runtime_va) continue;
        claimed.emplace_back(o.runtime_va, o.runtime_va + (o.byte_size ? o.byte_size : 0x400));
    }
    for (const auto& r : m_addresses.snapshot()) {
        const uint64_t width = value_type_size(r.type);
        const uint64_t end = (r.field_bound_end > r.runtime_va) ? r.field_bound_end
                                                               : r.runtime_va + (width ? width : 4);
        claimed.emplace_back(r.runtime_va, end);
    }
    const uint64_t root_vas[5] = {m_roots.gsc_va, m_roots.hero_va, m_roots.health_va,
                                  m_roots.mana_va, m_roots.char_ctrl_va};
    for (uint64_t va : root_vas)
        if (va) claimed.emplace_back(va, va + 8);
    m_explorer.set_claimed(std::move(claimed));

    const auto r = m_explorer.pass(guest_memory, guest_mem_size, frame,
                                   scene_signal, rebuild_signal);
    if (r.slots_changed) m_explore_dirty = true;

    char b[224];
    std::snprintf(b, sizeof(b),
                  "pass %llu: %zu slot(s) examined, %zu skipped as already identified, "
                  "%zu changed, %zu promoted to lead candidates%s",
                  static_cast<unsigned long long>(m_explorer.passes()),
                  r.slots_examined, r.slots_skipped_claimed, r.slots_changed, r.promotions,
                  r.track_cap_hit ? "  (track cap reached \u2014 raise it or narrow the region)" : "");
    m_explore_status = b;
}

void MemoryResearchTab::register_module(const ModuleExtent& e) {
    ModuleMap mods = m_object_map.modules();
    mods.set(e);
    m_object_map.set_modules(mods);
    ++m_site_stamp;   // module extents changed: drop the symbol memos
}

bool MemoryResearchTab::register_module_sections(const std::string& module,
                                                const void* shdrs, size_t count,
                                                const char* names, size_t names_size,
                                                bool is_64) {
    ++m_site_stamp;
    return m_sections.build(module, shdrs, count, names, names_size, is_64);
}

bool MemoryResearchTab::register_module_symbols(const std::string& module,
                                               const void* symtab, size_t count,
                                               const char* strtab, size_t strtab_size,
                                               bool is_64) {
    // The ELF loader hands us pointers straight into the loaded image, so this is
    // a walk of the image the emulator is already running — not a re-read of the
    // file.  Failure is non-fatal and leaves the table empty, which the UI reports
    // as "no symbol evidence" rather than silently showing nothing.
    ++m_site_stamp;
    return m_symbols.build(module, symtab, count, strtab, strtab_size, is_64);
}

void MemoryResearchTab::tick(uint64_t frame, uint8_t* guest_memory, uint64_t guest_mem_size) {
    if (!guest_memory) return;
    m_frame_counter = frame;

    // 1. Freeze.  Applied on the guest's own tick so a frozen value cannot race
    //    the game's write between frames.
    m_addresses.apply_freeze(guest_memory, guest_mem_size);

    // 2. Re-resolve every tracked object.  A moved object keeps its identity;
    //    only the runtime half of the pipeline is rewritten.
    std::vector<LiveObject> objects;
    bool any_moved = false;
    if (m_object_map.size()) {
        m_object_map.refresh(guest_memory, guest_mem_size, frame);
        m_object_map.sample_fields(guest_memory, guest_mem_size);
        objects = m_object_map.snapshot();
        for (const auto& o : objects) any_moved = any_moved || o.moved();
    }

    // 3. Address-list rows that pointed INTO an object that just moved follow
    //    it.  This is the thing a cross-process editor cannot do: it would
    //    silently keep pointing at the old, now-unrelated memory.
    if (any_moved) {
        for (const AddressEntry& row : m_addresses.snapshot()) {
            for (const auto& o : objects) {
                if (!o.moved()) continue;
                const uint64_t lo = o.previous_va;
                const uint64_t hi = o.previous_va + (o.byte_size ? o.byte_size : 0x1000);
                if (row.runtime_va < lo || row.runtime_va >= hi) continue;
                const uint64_t offset = row.runtime_va - lo;
                m_addresses.rebind(row.id, o.runtime_va + offset);
                break;
            }
        }
    }

    // 4. Bound the claim set each pass, then take one exploration step.  Records
    //    the scene/rebuild signals from this window so leads can be ranked by
    //    how they relate to the scene.
    explore_tick(guest_memory, guest_mem_size, frame);
}

// ---------------------------------------------------------------------------
// resolve_site / build_id_of / unmapped_site_name — the three identity tiers
// ---------------------------------------------------------------------------
std::string MemoryResearchTab::build_id_of(const std::string& module) const {
    if (const ModuleExtent* e = m_object_map.modules().find(module)) return e->build_id;
    return std::string();
}

SiteIdentity MemoryResearchTab::resolve_site(
    uint64_t va, ValueType type, const std::vector<AddressEntry>* known_rows) const {
    SiteIdentity s;

    // Normalise first.  Everything below is expressed in static space: the raw
    // runtime address is never the identity of anything.
    uint64_t rva = 0;
    s.mapped = m_object_map.modules().rva_of(va, &s.site_module, &rva);
    if (s.mapped) {
        s.site_rva = rva;
        char b[128];
        std::snprintf(b, sizeof(b), "%s+0x%llX", s.site_module.c_str(),
                      static_cast<unsigned long long>(rva));
        s.site = b;
        // The image is unstripped (~17.7k dynamic symbols in libswordigo.so), so
        // "libswordigo.so+0x31058" is not the best answer available: the site can
        // be named by the last dynamic symbol at or below it, and by the symbol on
        // the far side of the gap it actually sits in.
        symbol_evidence(s.site_module, rva, &s);
        // The symbol table cannot describe the ELF's own metadata blocks (its
        // lowest symbol is above them), so the section answers for those.
        section_evidence(s.site_module, rva, &s);
        if (s.symbol.empty() && !s.section.empty()) {
            s.symbol  = s.section;
            s.sym_kind = s.in_metadata ? "metadata" : "section";
        }
    } else {
        s.site = "unmapped (stack/heap/JIT)";
    }

    // ── Tier A: a live object the map is already tracking ────────────────────
    if (const LiveObject* o = m_object_map.find_by_va(va)) {
        s.name  = o->display_name();
        s.owner = "object base";
        s.prov  = o->identity.provenance;
        s.known = provenance_is_proven(s.prov);
        s.tier  = s.known ? "known" : "identifiable";
        s.var_id = o->identity.var_id;
        s.note  = "object tracked by the live object map; the pointer is re-resolved "
                  "every frame, so this name survives the object moving";
        return s;
    }

    // ── Tier A: a field already bound to an identity ─────────────────────────
    if (const LiveMemoryEntry* e = m_object_map.index().find_by_va(va)) {
        s.name  = e->display_name.empty() ? e->identity.base_name : e->display_name;
        s.prov  = e->identity.provenance;
        s.known = provenance_is_proven(s.prov);
        s.tier  = s.known ? "known" : "identifiable";
        s.var_id = e->identity.var_id;
        s.note  = std::string("bound from static key ") + e->identity.canonical;
        return s;
    }

    // ── Tier A: a recovered field already sitting in the address list ────────
    if (known_rows && !known_rows->empty()) {
        for (const AddressEntry& r : *known_rows) {
            const uint64_t w = value_type_size(r.type);
            const uint64_t end = r.runtime_va + (w ? w : 1);
            if (va < r.runtime_va || va >= end) continue;
            s.name  = r.label();
            s.owner = r.group;
            s.prov  = r.provenance;
            s.known = r.is_known();
            s.tier  = s.known ? "known" : "identifiable";
            s.var_id = r.identity.var_id;
            s.note  = s.known
                ? std::string("recovered field of ") + (r.group.empty() ? "a live object" : r.group)
                : std::string("identity held by an address-list row (") + provenance_name(s.prov) + ")";
            return s;
        }
    }

    // ── Tier B: a static site that nobody has named yet ─────────────────────
    if (s.mapped) {
        StaticRef ref;
        ref.build_id      = build_id_of(s.site_module);
        ref.module        = s.site_module;
        ref.kind          = MemKind::Global;
        ref.container_rva = rva;
        ref.type_name     = value_type_name(type);
        ref.provenance    = Provenance::Observed;
        const Identity id = IdentityResolver::resolve(ref);
        s.name   = id.base_name;
        s.var_id = id.var_id;
        s.prov   = Provenance::Observed;
        s.tier   = "identifiable";
        s.note = std::string("no recovered name yet, but the static site is known, so this "
                             "name is derived from the build + module + RVA + type and is the "
                             "SAME on every boot (" + ref.canonical() + ")");
        if (!s.symbol.empty()) s.note += "\nsymbol evidence: " + s.between;
        return s;
    }

    // ── Tier C: no static anchor at all ─────────────────────────────────────
    s.name   = session_scoped_site_name(va);
    s.prov   = Provenance::Unknown;
    s.tier   = "runtime only";
    s.note   = "this slot has no static anchor (stack, heap or JIT arena), so no name can "
               "be proven stable across boots; this one is session-scoped by design";
    return s;
}

// ---------------------------------------------------------------------------
// symbol_evidence — describe a static site with the image's own symbol table.
//
// Pure function of (module, RVA), memoised on that basis.  Most addresses sit
// *between* symbols rather than inside one (the engine has tens of thousands of
// small functions and plenty of padding), so both sides of the gap are reported
// and "inside" is only claimed when a symbol's extent actually covers the
// address.  A wrong membership claim would be worse than no name at all.
// ---------------------------------------------------------------------------
void MemoryResearchTab::symbol_evidence(const std::string& module, uint64_t rva,
                                        SiteIdentity* s) const {
    if (!s) return;
    SymCacheEntry& slot = m_sym_cache[(rva ^ (rva >> 13)) % kSymCacheSlots];
    if (slot.valid && slot.stamp == m_site_stamp && slot.rva == rva && slot.module == module) {
        s->symbol        = slot.symbol;
        s->between       = slot.between;
        s->sym_kind      = slot.kind;
        s->sym_qualified = slot.qualified;
        s->sym_inside    = slot.inside;
        return;
    }

    const SymbolEntry* fi = m_symbols.floor(module, rva);
    if (!fi) {
        if (const SymbolEntry* ce = m_symbols.ceil(module, rva)) {
            s->between  = "before " + ce->brief;
            s->sym_kind = "before";
        }
    } else {
        char off[32];
        std::snprintf(off, sizeof(off), "+0x%llX",
                      static_cast<unsigned long long>(rva - fi->rva));
        s->symbol        = fi->brief + off;
        s->sym_qualified = fi->name;
        s->sym_inside    = SymbolTable::covers(*fi, rva);
        s->sym_kind      = s->sym_inside ? "inside" : "after";
        if (const SymbolEntry* ce = m_symbols.ceil(module, rva)) {
            if (s->sym_inside) {
                char in[160];
                std::snprintf(in, sizeof(in), "inside %s  (0x%llX of 0x%llX)",
                              fi->brief.c_str(),
                              static_cast<unsigned long long>(rva - fi->rva),
                              static_cast<unsigned long long>(fi->size));
                s->between = in;
            } else {
                s->between = s->symbol + "  ..  " + ce->brief;
                s->sym_kind = "between";
            }
        } else {
            s->between = "after " + fi->brief;
        }
    }

    slot.module    = module;
    slot.rva       = rva;
    slot.stamp     = m_site_stamp;
    slot.symbol    = s->symbol;
    slot.between   = s->between;
    slot.kind      = s->sym_kind;
    slot.qualified = s->sym_qualified;
    slot.inside    = s->sym_inside;
    slot.valid     = true;
}

// ---------------------------------------------------------------------------
// section_evidence — the coarse half of "where is this address?", used when the
// symbol table has nothing to say.
//
// The symbol table has a hole below its lowest symbol (0x264397 in the shipped
// libswordigo.so), and a module-scoped scan lands right in it: measured on the
// real image, 8 of 12 hits for a common 4-byte value sat inside .dynsym and
// .rela.plt.  "No symbol below" is true but useless there; ".dynsym+0x30D60"
// tells a researcher immediately that the hit is the emulator's own bookkeeping.
// ---------------------------------------------------------------------------
void MemoryResearchTab::section_evidence(const std::string& module, uint64_t rva,
                                         SiteIdentity* s) const {
    if (!s) return;
    SecCacheEntry& slot = m_sec_cache[(rva ^ (rva >> 11)) % kSecCacheSlots];
    if (slot.valid && slot.stamp == m_site_stamp && slot.rva == rva && slot.module == module) {
        s->section      = slot.section;
        s->section_kind = slot.kind;
        s->in_metadata  = slot.meta;
        return;
    }
    const SectionEntry* se = m_sections.find(module, rva);
    if (se) {
        s->section      = m_sections.describe(module, rva);
        s->section_kind = section_kind_name(
            section_kind_of(se->type, se->flags, se->name));
        s->in_metadata  = section_kind_is_metadata(
            section_kind_of(se->type, se->flags, se->name));
    }
    slot.module  = module;
    slot.rva     = rva;
    slot.stamp   = m_site_stamp;
    slot.section = s->section;
    slot.kind    = s->section_kind;
    slot.meta    = s->in_metadata;
    slot.valid   = true;
}

// ---------------------------------------------------------------------------
// draw_hud — the in-game mini bar
//
// Deliberately not a second console.  It shows the smallest set of things that
// let someone keep playing: the rows they are watching, and the three actions
// that matter mid-game (freeze, nudge, set).  Everything else is one click away
// through "full screen", which returns to the console on the section the HUD was
// operating on rather than wherever it was left.
// ---------------------------------------------------------------------------
bool MemoryResearchTab::draw_hud() {
    if (!m_hud_enabled || !m_ready) return false;

    const uint8_t* mem = m_decoder ? m_decoder->guest_memory() : nullptr;
    const uint64_t mem_size = m_decoder ? m_decoder->guest_mem_size() : 0;
    uint8_t* writable = m_decoder ? const_cast<uint8_t*>(m_decoder->guest_memory()) : nullptr;
    if (!mem) return false;

    bool want_full = false;

    if (!m_hud_placed) {
        ImGui::SetNextWindowPos(ImVec2(28.0f, 140.0f), ImGuiCond_Always);
        m_hud_placed = true;
    }
    ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.80f);

    bool open = true;
    // The bar overlays the game, so it uses the tool face for its own chrome.
    const bool pushed_body = m_fonts.body != nullptr;
    if (pushed_body) ImGui::PushFont(static_cast<ImFont*>(m_fonts.body));
    if (ImGui::Begin("Swordfare##sf_hud", &open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize)) {
        // ── Header: what state the tool is in, in one line ──────────────────
        ImGui::TextDisabled("LIVE MEMORY");
        ImGui::SameLine();
        if (m_scan_current_shot)
            ImGui::TextDisabled("shot #%llu", static_cast<unsigned long long>(m_scan_current_shot));
        else
            ImGui::TextDisabled("no scan yet");
        ImGui::SameLine();
        ImGui::TextDisabled("| %zu offset(s)", m_scanner.candidate_count());

        ImGui::Separator();

        // ── Rows worth poking mid-game ──────────────────────────────────────
        // The address list first (it is the shortlist the researcher built), the
        // scan results as the fallback while a search is still being narrowed.
        const std::vector<AddressEntry> rows = m_addresses.snapshot();
        if (!rows.empty()) {
            const size_t shown = std::min<size_t>(rows.size(), 12);
            for (size_t i = 0; i < shown; ++i) {
                const AddressEntry& r = rows[i];
                ImGui::PushID(static_cast<int>(r.id));

                bool frozen = r.frozen;
                if (ImGui::Checkbox("##f", &frozen)) {
                    std::string err;
                    if (!m_addresses.set_frozen(r.id, frozen, mem, mem_size, "", &err))
                        m_hud_status = err;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("freeze %s", r.label().c_str());
                ImGui::SameLine();

                ImGui::TextColored(provenance_colour(r.provenance), "%s", r.label().c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\n%s", provenance_name(r.provenance),
                                      m_object_map.modules().describe(r.runtime_va).c_str());

                ImGui::SameLine();
                ImGui::SetCursorPosX(220.0f);
                if (m_hud_editing == r.id) {
                    // Same editor as the console, so "why did this revert?" cannot
                    // be answered differently on the two surfaces.
                    draw_value_editor(r.id, r, mem, writable, mem_size,
                                      m_hud_edit, sizeof(m_hud_edit),
                                      &m_hud_edit_domain, &m_hud_edit_domain,
                                      &m_hud_edit_dirty, &m_hud_status,
                                      &m_hud_edit_error, &m_hud_edit_counterpart,
                                      72.0f, /*compact=*/true);
                    if (m_value_edit_over) { m_hud_editing = 0; m_value_edit_over = false; }
                } else {
                    {
                        const bool mono = push_mono();
                        ImGui::TextUnformatted(m_addresses.read_value(r.id, mem, mem_size).c_str());
                        pop_mono(mono);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton(ICON_FA_PEN "##he")) {
                        m_hud_editing     = r.id;
                        m_hud_edit_domain = r.domain;
                        m_hud_edit_dirty  = false;
                        m_hud_edit_error.clear();
                        m_hud_edit_counterpart.clear();
                        // Editable form, not the rendered form.
                        const std::string seed =
                            m_addresses.read_editable(r.id, mem, mem_size, r.domain);
                        std::snprintf(m_hud_edit, sizeof(m_hud_edit), "%s", seed.c_str());
                    }
                    // Nudge: the whole reason this bar exists is to change a
                    // value while the game is running, so +-1 is one click.
                    ImGui::SameLine();
                    if (ImGui::SmallButton("-1")) nudge_row(r.id, -1, writable, mem, mem_size);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("+1")) nudge_row(r.id, +1, writable, mem, mem_size);
                }
                ImGui::PopID();
            }
            if (rows.size() > shown) ImGui::TextDisabled("... and %zu more in the full console", rows.size() - shown);
        } else if (m_scanner.candidate_count()) {
            const size_t shown = std::min<size_t>(m_scanner.candidate_count(), 8);
            const std::vector<AddressEntry> known_rows;   // scan hits resolve as static sites
            for (size_t i = 0; i < shown; ++i) {
                const ScanCandidate& c = m_scanner.candidates()[i];
                const SiteIdentity si = resolve_site(c.address, c.type, &known_rows);
                ImGui::TextColored(ImVec4(0.80f, 0.78f, 0.45f, 1.0f), "%s", si.name.c_str());
                ImGui::SameLine();
                ImGui::SetCursorPosX(220.0f);
                {
                    const bool mono = push_mono();
                    ImGui::TextUnformatted(
                        MemScanner::format_value(mem, mem_size, c.address, c.type).c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("@0x%llX", static_cast<unsigned long long>(c.address));
                    pop_mono(mono);
                }
            }
        } else {
            ImGui::TextDisabled("nothing to watch yet \u2014 run a scan in the full console");
        }

        if (!m_hud_status.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", m_hud_status.c_str());
        }

        // ── Footer ──────────────────────────────────────────────────────────
        ImGui::Separator();
        if (ImGui::Button("full screen")) {
            // Land on the section the bar was working with, not wherever the
            // console was last left.
            m_active_section = m_addresses.size() ? 8 : 7;
            want_full = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("scan")) {
            m_active_section = 7;
            want_full = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_workspace.is_open() ? "research store live" : "no store");
    }
    ImGui::End();
    if (pushed_body) ImGui::PopFont();

    if (!open) m_hud_enabled = false;
    return want_full;
}

// ---------------------------------------------------------------------------
// drop_metadata_hits — remove scan hits that land in the image's own bookkeeping.
//
// Measured on the shipped libswordigo.so: the lowest symbol is at rva 0x264397,
// so .dynsym (0x2F8), .dynstr (0x8E7B8), .rela.dyn (0x197BD0) and .rela.plt
// (0x20BC70) have no symbols at all — and a module-scoped exact scan lands right
// in them, because those tables are full of small repeated integers (relocation
// addends, field-number constants, hash buckets).  A real 12-hit scan for a
// common 4-byte value had 8 of its 12 hits inside .dynsym / .rela.plt.
//
// Dropping them is a post-filter over the candidate set rather than a rule in the
// scan loop: the loop stays a single flat pass with no per-position branch, and
// filtering a few thousand candidates is free next to scanning 16 million
// positions.  Returns how many were removed so the UI can say so out loud.
// ---------------------------------------------------------------------------
size_t MemoryResearchTab::drop_metadata_hits() {
    if (!m_scan_skip_metadata) return 0;
    const std::vector<ScanCandidate>& cands = m_scanner.candidates();
    if (cands.empty()) return 0;

    // Collect the metadata ranges of every registered image once, in module
    // space, then test each candidate after normalising its address.
    struct Ranges { std::string module; uint64_t base; std::vector<std::pair<uint64_t, uint64_t>> rs; };
    std::vector<Ranges> per_module;
    for (const ModuleExtent& m : m_object_map.modules().modules()) {
        Ranges r;
        r.module = m.name;
        r.base   = m.base_va;
        r.rs     = m_sections.metadata_ranges(m.name);
        if (!r.rs.empty()) per_module.push_back(std::move(r));
    }
    size_t removed = 0;
    if (!per_module.empty()) {
        std::vector<ScanCandidate> kept;
        kept.reserve(cands.size());
        for (const ScanCandidate& c : cands) {
            bool drop = false;
            for (const Ranges& r : per_module) {
                if (c.address < r.base) continue;
                if (rva_in_ranges(r.rs, c.address - r.base)) { drop = true; break; }
            }
            if (drop) ++removed; else kept.push_back(c);
        }
        if (removed) m_scanner.replace_candidates(std::move(kept));
    }
    m_scan_skipped_meta = removed;
    return removed;
}

// Nudge an integer/fractional row by `delta` using the same validated write path
// as the console, so the HUD can never do anything the console would refuse.
void MemoryResearchTab::nudge_row(uint64_t id, int delta,
                                  uint8_t* mem, const uint8_t* ro, uint64_t mem_size) {
    const AddressEntry* r = m_addresses.find(id);
    if (!r) return;
    // Read in the row's own domain rather than parsing its rendered text: a
    // float row renders as "2.5", and adding to the printed integer would have
    // silently written 3.  Integer types stay exact (int64) so a 64-bit value is
    // never round-tripped through a double.
    MemNumber cur;
    if (!mem_read_number(ro, mem_size, r->runtime_va, r->type, &cur)) {
        m_hud_status = "that row cannot be read as a number, so it cannot be nudged";
        return;
    }
    char buf[64];
    if (cur.is_float)
        std::snprintf(buf, sizeof(buf), "%.9g", cur.f + delta);
    else
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(cur.i) + delta);
    std::string err;
    if (!m_addresses.set_value(id, buf, mem, mem_size, &err))
        m_hud_status = err;
    else
        m_hud_status = r->label() + " \u2190 " + buf;
}

// ---------------------------------------------------------------------------
// draw_scanner
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_scanner() {
    const uint8_t* mem = m_decoder ? m_decoder->guest_memory() : nullptr;
    const uint64_t mem_size = m_decoder ? m_decoder->guest_mem_size() : 0;

    xpera::section_header("Scan the live guest address space",
                          "own the address space: one pass, no per-region walk");
    ImGui::Spacing();

    // ── In-game mini bar toggle ─────────────────────────────────────────────
    {
        bool hud = m_hud_enabled;
        if (ImGui::Checkbox("in-game mini bar", &hud)) m_hud_enabled = hud;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("dock the watched rows over the game as a small bar, so you can "
                              "keep playing while poking values; \"full screen\" in the bar "
                              "brings this console back");
        ImGui::SameLine();
        ImGui::TextDisabled("stays on top of the game, frozen rows keep being rewritten");
    }

    // ── Symbol evidence ─────────────────────────────────────────────────────
    // libswordigo.so is unstripped (~17.7k dynamic symbols), so a static site can
    // be described by the symbol it falls in or between instead of a bare
    // `module+0xOFFSET`.  This is annotation only: the identity handle above is
    // unchanged by it.
    {
        const bool have = m_symbols.count("libswordigo.so") > 0;
        if (have) {
            ImGui::TextDisabled("SYMBOLS  %s", m_symbols.summary().c_str());
            if (m_sections.has("libswordigo.so")) {
                ImGui::TextDisabled("SECTIONS %s", m_sections.summary().c_str());
            }
        } else {
            ImGui::TextDisabled("SYMBOLS  none loaded \u2014 sites show as module+offset only");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("the symbol table comes from the loaded image; it is handed "
                                  "over at boot once libswordigo.so is mapped");
        }
    }
    ImGui::Spacing();

    // ── Speedhack ────────────────────────────────────────────────────────
    // This is NOT a busy-loop or a hooked timing syscall like CE/GG need in a
    // foreign process.  Swordfare already scales the guest's virtual clock
    // (`advance_guest_virtual_clock(dt * g_game_speed)` in the emulator loop),
    // so the control here simply drives that existing, scalable tick source.
    {
        ImGui::TextDisabled("SPEEDHACK  (scales the guest virtual clock)");
        ImGui::SetNextItemWidth(260);
        float speed = g_game_speed;
        if (ImGui::SliderFloat("##gspeed", &speed, 0.05f, 8.0f, "x%.2f",
                               ImGuiSliderFlags_Logarithmic))
            g_game_speed = speed;
        ImGui::SameLine();
        if (ImGui::SmallButton("1.0x")) g_game_speed = 1.0f;
        ImGui::SameLine();
        ImGui::TextDisabled("applies to dt on the emulated frame, not to the render loop");
    }

    ImGui::Spacing();
    xpera::divider(0.5f);
    ImGui::Spacing();

    ImGui::SetNextItemWidth(120);
    ImGui::Combo("##vt", &m_scan_value_type, kValueTypeNames, kValueTypeCount);
    ImGui::SameLine();

    // ── Filter list: only offer what can actually run ───────────────────────
    //
    // Ported from libmemscan's validateCombo() (LGPL-3.0).  Offering all twelve
    // filters always means "Increased Value" is selectable on a first scan, where
    // there is no previous value to compare against — and a scanner that then
    // falls back internally turns the operator's filter into an invisible no-op
    // that looks like a result.  So the unusable ones are removed from the list,
    // and the count of removed ones is stated rather than left to be noticed.
    const ValueType scan_type = kValueTypes[m_scan_value_type];
    const bool first_pass = m_scanner.candidate_count() == 0 && m_scan_current_shot == 0;

    if (m_scan_kind >= 0 && m_scan_kind < kScanKindCount &&
        !scan_type_supported(kScanKinds[m_scan_kind], scan_type, first_pass)) {
        // The stored choice became unusable (the type changed, or this is a first
        // scan again).  Snap to Exact, which is always valid, and say why.
        const char* why = scan_type_unsupported_reason(kScanKinds[m_scan_kind], scan_type,
                                                       first_pass);
        m_scan_status = std::string("filter reset to Exact Value \u2014 ") +
                        (why ? why : "that filter cannot run here");
        m_scan_kind = 0;
    }

    int         vis_map[kScanKindCount];
    const char* vis_names[kScanKindCount];
    int         vis_count = 0;
    int         hidden    = 0;
    const char* hidden_why = nullptr;
    for (int i = 0; i < kScanKindCount; ++i) {
        if (scan_type_supported(kScanKinds[i], scan_type, first_pass)) {
            vis_names[vis_count] = kScanKindNames[i];
            vis_map[vis_count]   = i;
            ++vis_count;
        } else {
            ++hidden;
            if (!hidden_why)
                hidden_why = scan_type_unsupported_reason(kScanKinds[i], scan_type, first_pass);
        }
    }
    int shown_kind = 0;
    for (int i = 0; i < vis_count; ++i)
        if (vis_map[i] == m_scan_kind) shown_kind = i;

    ImGui::SetNextItemWidth(190);
    if (ImGui::Combo("##sk", &shown_kind, vis_names, vis_count))
        m_scan_kind = vis_map[shown_kind];
    if (hidden > 0 && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%d filter(s) hidden: %s", hidden,
                          hidden_why ? hidden_why : "not usable with this type and pass");
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##sv", m_scan_hex ? "value (hex: 1F 40 A0)" : "value",
                             m_scan_value, sizeof(m_scan_value));

    // Between needs a second bound.
    if (kScanKinds[m_scan_kind] == ScanType::Between) {
        ImGui::SetNextItemWidth(140);
        ImGui::InputTextWithHint("##sv2", "upper bound", m_scan_value2, sizeof(m_scan_value2));
        ImGui::SameLine();
    }
    ImGui::TextDisabled(
        "multi-shot workflow: search a value, change it in the game, type the NEW value and press "
        "\"Next scan\" with \"Exact Value\" \u2014 only offsets that were in the previous shot AND "
        "now read that value survive. Every shot is kept below, so you can go back to any of them.");
    if (hidden > 0) {
        ImGui::TextDisabled("%d filter(s) unavailable here: %s", hidden,
                            hidden_why ? hidden_why : "not usable with this type and pass");
    }
    ImGui::Spacing();

    ImGui::Checkbox("hex", &m_scan_hex);
    ImGui::SameLine();

    // The image's own tables are full of small repeated integers (relocation
    // addends, field-number constants, hash buckets), so a module-scoped scan
    // hits them constantly — 8 of 12 hits for a common value, measured on the
    // shipped libswordigo.so.  Skipping them is the difference between a results
    // table worth reading and one dominated by the emulator's own bookkeeping.
    ImGui::Checkbox("skip ELF tables", &m_scan_skip_metadata);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("exclude hits inside .dynsym / .dynstr / .rela.* / .hash \u2014 "
                          "metadata the image needs in order to run, never game state");
    if (m_scan_skipped_meta) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu filtered last scan)", m_scan_skipped_meta);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::Combo("##align", &m_scan_alignment,
                 (const char*[]){"natural", "1", "2", "4", "8"}, 5);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    int maxres = static_cast<int>(m_scan_max_results);
    if (ImGui::InputInt("max", &maxres, 0, 0)) {
        if (maxres < 0) maxres = 0;
        m_scan_max_results = static_cast<uint64_t>(maxres);
    }
    ImGui::SameLine();
    ImGui::Checkbox("module range only", &m_scan_restrict_module);

    const bool can_scan = mem != nullptr && mem_size != 0;
    if (!can_scan) {
        empty_state("Guest memory is not mapped yet.",
                    "The scanner runs against the flat guest buffer, which is only available once "
                    "the game has booted.  Boot with ARM64 + SRE and reopen this tab.");
    }
    if (!can_scan) ImGui::BeginDisabled();

    if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS "  First scan", ImVec2(140, 0))) {
        ScanValue v;
        std::string err;
        ScanOptions o;
        o.type      = kValueTypes[m_scan_value_type];
        o.scan      = kScanKinds[m_scan_kind];
        o.alignment = static_cast<size_t>(m_scan_alignment ? (1 << (m_scan_alignment - 1)) : 0);
        o.max_results = static_cast<size_t>(m_scan_max_results);
        const bool is_str_or_aob = (o.type == ValueType::Str || o.type == ValueType::AoB);
        // "Unknown initial" seeds every slot; an exact string/AoB scan needs text.
        const bool needs_value = !is_str_or_aob &&
                                 o.scan != ScanType::UnknownInitial &&
                                 o.scan != ScanType::Changed &&
                                 o.scan != ScanType::Unchanged &&
                                 o.scan != ScanType::Increased &&
                                 o.scan != ScanType::Decreased;
        bool rejected = false;
        if ((needs_value || is_str_or_aob) && m_scan_value[0] == '\0') {
            m_scan_status = is_str_or_aob ? "enter the text / byte pattern to search for"
                                         : "enter a value to search for";
            rejected = true;
        }
        if (!rejected) {
            std::string text = m_scan_value;
            if (m_scan_hex && !is_str_or_aob) text = "0x" + text;
            if (!MemScanner::parse_typed_text(o.type, text, &v)) {
                m_scan_status = "that value does not fit the selected type";
                rejected = true;
            }
        }
        if (!rejected && o.scan == ScanType::Between) {
            std::string t2 = m_scan_value2;
            if (m_scan_hex) t2 = "0x" + t2;
            ScanValue upper;
            if (!MemScanner::parse_typed_text(o.type, t2, &upper)) {
                m_scan_status = "upper bound does not fit the selected type";
                rejected = true;
            } else {
                v.num2 = upper.num;
            }
        }
        // Honour "module range only".  A control that silently does nothing is
        // worse than one that refuses loudly.
        if (!rejected && m_scan_restrict_module) {
            uint64_t rlo = 0, rhi = 0;
            if (module_scan_range(&rlo, &rhi)) {
                o.range_begin = rlo;
                o.range_end   = rhi;
            } else {
                m_scan_status = "no image is registered with the research engine, so \"module "
                                "range only\" cannot be honoured \u2014 untick it to scan the whole "
                                "address space";
                rejected = true;
            }
        }
        if (!rejected) {
            ScanStats st;
            m_scanner.first_scan(mem, mem_size, v, o, &st);
            m_scan_elapsed_ms = st.elapsed_ms;
            ++m_scan_generation;
            // Every shot becomes a restore point, so "what were the offsets
            // before I narrowed to this?" is always answerable.
            const size_t skipped = drop_metadata_hits();
            m_scan_current_shot = m_scanner.save_pass(o.scan, o.type, m_scan_value);
            m_scanner.trim_passes(32);
            char extra[160] = {};
            if (skipped) {
                std::snprintf(extra, sizeof(extra),
                              "  \u2014 excluded %zu hit(s) inside the image's own tables "
                              "(dynsym/dynstr/rela), which are never game state", skipped);
            }
            char b[384];
            std::snprintf(b, sizeof(b),
                          "shot #%llu: %zu result(s) in %.1f ms  (%zu positions over 0x%llX bytes%s)%s",
                          static_cast<unsigned long long>(m_scan_current_shot),
                          m_scanner.candidate_count(), st.elapsed_ms, st.compared,
                          static_cast<unsigned long long>(st.bytes_scanned),
                          m_scan_restrict_module ? ", module range" : ", whole address space",
                          extra);
            m_scan_status = b;
            // The engine reports when it could not evaluate the requested filter.
            // Saying so beats shipping a filter that quietly became a seed.
            if (st.filter_ignored) {
                m_scan_status += "\nnote: '" + std::string(scan_type_name(st.requested)) +
                                 "' needs a previous value, so this seeded every slot "
                                 "instead '" + scan_type_name(st.applied) +
                                 "'. Narrow it with a filter below.";
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Next scan", ImVec2(110, 0))) {
        ScanValue v;
        ScanOptions o;
        o.type        = kValueTypes[m_scan_value_type];
        o.scan        = kScanKinds[m_scan_kind];
        o.max_results = static_cast<size_t>(m_scan_max_results);
        if (!MemScanner::parse_typed_text(o.type,
                                          m_scan_hex ? "0x" + std::string(m_scan_value)
                                                     : std::string(m_scan_value), &v)) {
            m_scan_status = "next scan needs a parseable value (except for the relative scans)";
        } else {
            const ScanValue* vp = &v;
            ScanValue dummy;   // Changed/Unchanged/Increased/Decreased ignore the target
            if (o.scan == ScanType::Changed || o.scan == ScanType::Unchanged ||
                o.scan == ScanType::Increased || o.scan == ScanType::Decreased)
                vp = &dummy;
            ScanStats st;
            const size_t n = m_scanner.next_scan(mem, mem_size, *vp, o, &st);
            m_scan_elapsed_ms = st.elapsed_ms;
            ++m_scan_generation;
            m_scan_current_shot = m_scanner.save_pass(o.scan, o.type, m_scan_value);
            m_scanner.trim_passes(32);
            char b[256];
            std::snprintf(b, sizeof(b),
                          "shot #%llu (%s %s): narrowed to %zu result(s) in %.1f ms",
                          static_cast<unsigned long long>(m_scan_current_shot),
                          scan_type_name(o.scan), m_scan_value, n, st.elapsed_ms);
            m_scan_status = b;
            if (st.filter_ignored) {
                m_scan_status += "\nnote: the filter '" + std::string(scan_type_name(st.requested)) +
                                 "' could not be evaluated on this pass";
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh values", ImVec2(130, 0))) {
        ScanOptions o;
        m_scanner.snapshot_values(mem, mem_size, o);
        m_scan_status = "baseline refreshed (next scan now compares against this frame)";
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(80, 0))) {
        m_scanner.clear();
        ++m_scan_generation;
        m_scan_current_shot = 0;
        m_scan_status = "scan reset (saved shots dropped too)";
    }

    // ── Saved shots ─────────────────────────────────────────────────────
    // The point of the multi-shot model: a search is a sequence, and the useful
    // moment is often two shots back, not the one you are looking at.
    {
        const auto& passes = m_scanner.passes();
        ImGui::Spacing();
        ImGui::TextDisabled("SHOTS  (every search is kept as a restore point)");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180);
        ImGui::InputTextWithHint("##plabel", "label this shot (optional)",
                                 m_scan_pass_label, sizeof(m_scan_pass_label));
        ImGui::SameLine();
        if (ImGui::SmallButton("save current offsets")) {
            m_scan_current_shot = m_scanner.save_pass(
                kScanKinds[m_scan_kind], kValueTypes[m_scan_value_type], m_scan_value,
                m_scan_pass_label);
            m_scanner.trim_passes(32);
            m_scan_status = "saved the current " +
                            std::to_string(m_scanner.candidate_count()) +
                            " offset(s) as shot #" + std::to_string(m_scan_current_shot);
            m_scan_pass_label[0] = '\0';
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("keep this exact offset list so you can come back to it after "
                              "further scans");

        if (passes.empty()) {
            ImGui::TextDisabled("  no shots yet \u2014 run a scan and it will be recorded here");
        } else if (ImGui::BeginTable("##shots", 6, kGridFlags, ImVec2(0, 132))) {
            ImGui::TableSetupColumn("shot", ImGuiTableColumnFlags_WidthFixed, 46);
            ImGui::TableSetupColumn("filter", ImGuiTableColumnFlags_WidthFixed, 110);
            ImGui::TableSetupColumn("target", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("offsets", ImGuiTableColumnFlags_WidthFixed, 110);
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 160);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableHeadersRow();
            for (const ScanPass& p : passes) {
                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(p.shot));
                const bool is_current = (p.shot == m_scan_current_shot);

                ImGui::TableSetColumnIndex(0);
                if (is_current)
                    ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.60f, 1.0f), "#%zu", p.shot);
                else
                    ImGui::Text("#%zu", p.shot);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", scan_type_name(p.scan));

                ImGui::TableSetColumnIndex(2);
                if (p.target.empty()) ImGui::TextDisabled("\u2014");
                else                  ImGui::TextUnformatted(p.target.c_str());

                ImGui::TableSetColumnIndex(3);
                if (p.from != p.results)
                    ImGui::Text("%zu  (from %zu)", p.results, p.from);
                else
                    ImGui::Text("%zu", p.results);

                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(p.label.c_str());

                ImGui::TableSetColumnIndex(5);
                if (!p.has_set) {
                    ImGui::TextDisabled("too large to keep");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("this shot had more than %zu offsets, so only its "
                                          "summary was kept",
                                          MemScanner::kMaxSavedCandidates);
                } else if (ImGui::SmallButton("restore")) {
                    if (m_scanner.restore_pass(p.shot)) {
                        m_scan_current_shot = p.shot;
                        ++m_scan_generation;
                        m_scan_status = "restored shot #" + std::to_string(p.shot) + " \u2014 " +
                                        std::to_string(m_scanner.candidate_count()) +
                                        " offset(s); filter it again from here";
                    }
                }
                if (ImGui::IsItemHovered() && p.has_set)
                    ImGui::SetTooltip("make these offsets the live set again, so you can apply a "
                                      "different filter to them");
                ImGui::SameLine();
                if (ImGui::SmallButton("drop")) {
                    const size_t ds = p.shot;
                    m_scanner.drop_pass(ds);
                    if (m_scan_current_shot == ds) m_scan_current_shot = 0;
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    if (!can_scan) ImGui::EndDisabled();

    if (!m_scan_status.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", m_scan_status.c_str());
    }

    ImGui::Spacing();
    xpera::divider();

    // ── Results ─────────────────────────────────────────────────────────
    const size_t total = m_scanner.candidate_count();
    if (total == 0) {
        empty_state(
            "No scan results yet.",
            "Pick a type and a value, then press \"First scan\".  A first scan walks the whole "
            "guest address space in one pass; \"Next scan\" narrows the existing set, and "
            "\"Unknown Initial Value\" seeds every slot so you can then filter by "
            "Increased / Decreased / Changed without knowing the value up front.");
        return;
    }
    ImGui::Text("%zu result(s)", total);
    ImGui::SameLine();
    if (ImGui::SmallButton("select all")) {
        m_scan_selected.clear();
        const size_t cap = std::min<size_t>(total, 500);
        m_scan_selected.reserve(cap);
        for (size_t i = 0; i < cap; ++i) m_scan_selected.push_back(static_cast<uint32_t>(i));
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("clear selection")) m_scan_selected.clear();
    ImGui::SameLine();
    if (ImGui::SmallButton("add selected to address list")) {
        size_t added = 0;
        for (uint32_t idx : m_scan_selected) {
            if (idx >= total) continue;
            const ScanCandidate& c = m_scanner.candidates()[idx];
            // Route the hit through the identity model before it is displayed:
            // a hit is a static site, and only its live address is transient.
            StaticRef ref;
            ref.type_name  = value_type_name(c.type);
            ref.provenance = Provenance::Observed;
            InstanceTag tag;
            std::string mod;
            uint64_t rva = 0;
            if (m_object_map.modules().rva_of(c.address, &mod, &rva)) {
                // Tier B: the container RVA is what makes the identity stable.
                // Using the runtime address here would have baked a transient
                // number into a key that is supposed to be boot-independent.
                ref.module        = mod;
                ref.kind          = MemKind::Global;
                ref.container_rva = rva;
                ref.build_id      = build_id_of(mod);
            } else {
                // Tier C: no static anchor.  Do not label it as a global; give it
                // a session-scoped instance token instead and say so.
                ref.kind   = MemKind::Unknown;
                tag.value  = IdentityResolver::stable_tag_value(std::to_string(c.address));
                tag.source = TagSource::Session;
            }
            const uint64_t row = m_addresses.add(ref, c.address, c.type, "scan result", 0, tag);
            (void)row;
            ++added;
        }
        char b[96];
        std::snprintf(b, sizeof(b), "%zu result(s) added to the address list", added);
        m_scan_status = b;
        m_scan_selected.clear();
    }

    // Fetched once: resolve_site() needs to know whether a hit is already a
    // recovered field, and re-snapshotting per row would be quadratic.
    const std::vector<AddressEntry> known_rows = m_addresses.snapshot();

    if (ImGui::BeginTable("##scanres", 7, kGridFlags, ImVec2(0, -1))) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 26);
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch, 200);
        ImGui::TableSetupColumn("address", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("type", ImGuiTableColumnFlags_WidthFixed, 66);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("symbol + offset", ImGuiTableColumnFlags_WidthFixed, 240);
        ImGui::TableSetupColumn("between", ImGuiTableColumnFlags_WidthFixed, 300);
        ImGui::TableHeadersRow();

        const size_t shown = std::min<size_t>(total, 2000);
        for (size_t i = 0; i < shown; ++i) {
            const ScanCandidate& c = m_scanner.candidates()[i];
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            bool sel = std::find(m_scan_selected.begin(), m_scan_selected.end(),
                                 static_cast<uint32_t>(i)) != m_scan_selected.end();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Checkbox("##sel", &sel)) {
                if (sel) m_scan_selected.push_back(static_cast<uint32_t>(i));
                else m_scan_selected.erase(
                    std::remove(m_scan_selected.begin(), m_scan_selected.end(),
                                static_cast<uint32_t>(i)), m_scan_selected.end());
            }
            ImGui::PopID();

            // Every hit is described by its identity, never by its address.  A
            // bare address is the one thing about a memory location that is
            // guaranteed to be meaningless on the next boot.
            const SiteIdentity si = resolve_site(c.address, c.type, &known_rows);

            ImGui::TableSetColumnIndex(1);
            const ImVec4 name_col = si.known   ? ImVec4(0.55f, 0.85f, 0.60f, 1.0f)
                                  : si.mapped  ? ImVec4(0.80f, 0.78f, 0.45f, 1.0f)
                                               : ImVec4(0.60f, 0.60f, 0.62f, 1.0f);
            ImGui::TextColored(name_col, "%s", si.name.c_str());
            if (ImGui::IsItemHovered() && !si.note.empty())
                ImGui::SetTooltip("%s\n%s", si.name.c_str(), si.note.c_str());
            if (!si.owner.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", si.owner.c_str());
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("0x%llX", static_cast<unsigned long long>(c.address));

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(value_type_name(c.type));

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(
                MemScanner::format_value(mem, mem_size, c.address, c.type).c_str());

            // ── Symbol evidence ────────────────────────────────────────────
            // The last dynamic symbol at or below the address, and where the
            // address lands inside it.  `module+0xNNN` is still there — on the
            // tooltip — because it is the exact, unambiguous form.
            ImGui::TableSetColumnIndex(5);
            const bool meta_hit = si.in_metadata;
            if (meta_hit) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::TextDisabled("%s",
                                si.symbol.empty() ? (si.mapped ? si.site.c_str() : "\u2014")
                                                  : si.symbol.c_str());
            if (meta_hit) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s\n%s\ntier: %s%s%s",
                                  si.symbol.empty() ? "no symbol at or below this address"
                                                    : si.symbol.c_str(),
                                  si.site.c_str(), si.tier.c_str(),
                                  si.sym_qualified.empty() ? "" : "\n",
                                  si.sym_qualified.c_str());

            ImGui::TableSetColumnIndex(6);
            if (si.between.empty()) ImGui::TextDisabled("\u2014");
            else                    ImGui::TextDisabled("%s", si.between.c_str());
            if (ImGui::IsItemHovered()) {
                if (si.in_metadata) {
                    ImGui::SetTooltip("this hit is inside the image's own %s (%s) \u2014 that "
                                      "table is metadata the loader needs in order to run, not "
                                      "game state; \"skip ELF tables\" removes these",
                                      si.section_kind.c_str(), si.section.c_str());
                } else if (!si.sym_kind.empty()) {
                    ImGui::SetTooltip("this address is %s\nsymbol evidence only \u2014 the "
                                      "identity above stays short and build-independent",
                                      si.sym_kind.c_str());
                }
            }
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// draw_address_list
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_address_list() {
    const uint8_t* mem = m_decoder ? m_decoder->guest_memory() : nullptr;
    const uint64_t mem_size = m_decoder ? m_decoder->guest_mem_size() : 0;
    uint8_t* writable = m_decoder ? const_cast<uint8_t*>(m_decoder->guest_memory()) : nullptr;

    xpera::section_header("Address list",
                          "rows are keyed by identity: a rename never destroys what a row is");
    ImGui::Spacing();

    // ── Add manually ────────────────────────────────────────────────────
    ImGui::SetNextItemWidth(130);
    ImGui::InputTextWithHint("##addva", "address (hex)", m_addr_add_va, sizeof(m_addr_add_va));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##addlbl", "researcher label (optional)",
                             m_addr_add_label, sizeof(m_addr_add_label));
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        uint64_t va = 0;
        if (!parse_address(m_addr_add_va, &va)) {
            m_addr_status = "address must be hex, e.g. 71A50090";
        } else {
            StaticRef ref;
            ref.kind      = MemKind::Unknown;
            ref.type_name = "4 Bytes";
            ref.provenance = Provenance::Observed;
            InstanceTag tag;
            std::string mod;
            uint64_t rva = 0;
            if (m_object_map.modules().rva_of(va, &mod, &rva)) {
                ref.module        = mod;
                ref.kind          = MemKind::Global;
                ref.container_rva = rva;
                ref.build_id      = build_id_of(mod);
            } else {
                // No static anchor: keep the honest tier-C shape (session token)
                // instead of claiming a static identity we cannot support.
                tag.value  = IdentityResolver::stable_tag_value(std::to_string(va));
                tag.source = TagSource::Session;
            }
            const uint64_t id = m_addresses.add(ref, va, ValueType::Dword, "manual", 0, tag);
            if (m_addr_add_label[0]) m_addresses.set_label(id, m_addr_add_label);
            if (tag.source == TagSource::Session) {
                m_addr_status = "row added \u2014 no static anchor for that address, so it got a "
                                "session-scoped name; it cannot be claimed stable across boots";
            } else {
                m_addr_status = "row added";
            }
        }
    }

    ImGui::Spacing();
    // ── Categorisation (§8) ─────────────────────────────────────────────
    ImGui::TextDisabled("FILTER");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(210);
    ImGui::Combo("##tier", &m_addr_filter_tier,
                 (const char*[]){"all tiers", "known (recovered/confirmed)",
                                 "unknown but identifiable", "runtime observation only"}, 4);
    ImGui::SameLine();
    m_addr_groups = m_addresses.groups();
    std::vector<const char*> group_names;
    group_names.push_back("(any group)");
    for (const auto& g : m_addr_groups) group_names.push_back(g.c_str());
    ImGui::SetNextItemWidth(180);
    ImGui::Combo("##grp", &m_addr_filter_group, group_names.data(),
                 static_cast<int>(group_names.size()));
    ImGui::SameLine();
    ImGui::Checkbox("frozen only", &m_addr_filter_frozen);

    AddressFilter f;
    f.frozen_only = m_addr_filter_frozen;
    switch (m_addr_filter_tier) {
        case 1: f.min_provenance = Provenance::Recovered; break;
        case 2: f.unresolved_only = true; break;
        case 3: f.min_provenance = Provenance::Observed;
                f.unresolved_only = true; break;
        default: break;
    }
    if (m_addr_filter_group > 0 && m_addr_filter_group - 1 < static_cast<int>(m_addr_groups.size())) {
        f.group     = m_addr_groups[m_addr_filter_group - 1];
        f.has_group = true;
    }

    std::vector<AddressEntry> rows = m_addresses.snapshot(f);
    ImGui::Spacing();

    if (rows.empty()) {
        if (m_addresses.size() == 0) {
            const std::string why = address_list_diagnosis();
            empty_state(
                "Address list is empty.",
                (why + "\n\nThe recovered fields of every live root are added here automatically; "
                       "scan results can be pushed over with \"add selected to address list\"; "
                       "and any address can be added by hand in the box above.").c_str());
        } else {
            empty_state(
                "Rows exist but the filter hides them all.",
                "Set the tier filter back to \"all tiers\" and the group filter to \"(any group)\" "
                "and untick \"frozen only\" to see every row.");
        }
        if (!m_addr_status.empty()) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", m_addr_status.c_str());
        }
        return;
    }

    ImGui::Text("%zu row(s) shown  |  %s", rows.size(), m_addresses.summary().c_str());

    if (ImGui::BeginTable("##addrlist", 9, kGridFlags, ImVec2(0, -1))) {
        ImGui::TableSetupColumn("freeze", ImGuiTableColumnFlags_WidthFixed, 44);
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch, 200);
        ImGui::TableSetupColumn("kind", ImGuiTableColumnFlags_WidthFixed, 86);
        ImGui::TableSetupColumn("owner", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("address", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("symbol + offset", ImGuiTableColumnFlags_WidthFixed, 240);
        ImGui::TableSetupColumn("between", ImGuiTableColumnFlags_WidthFixed, 300);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 86);
        ImGui::TableHeadersRow();

        for (const auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(r.id));

            ImGui::TableSetColumnIndex(0);
            bool frozen = r.frozen;
            if (ImGui::Checkbox("##frz", &frozen)) {
                std::string err;
                if (!m_addresses.set_frozen(r.id, frozen, mem, mem_size, "", &err))
                    m_addr_status = err;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("freeze: the value is rewritten every emulated tick, so the game\n"
                                  "cannot change it again after you set it");

            // ── name: the deterministic identity, never the address ─────────
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(provenance_colour(r.provenance), "%s %s",
                               provenance_badge(r.provenance), r.label().c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("identity    %s\nresolved    %s\nstatic key  %s",
                                  hex_identity(r.identity.var_id).c_str(),
                                  r.display_name.empty() ? "<none>" : r.display_name.c_str(),
                                  r.identity.canonical.empty() ? "<none>" : r.identity.canonical.c_str());
            }
            if (!r.user_label.empty() && r.user_label != r.display_name) {
                ImGui::SameLine();
                ImGui::TextDisabled("(was %s)", r.display_name.c_str());
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", mem_kind_name(r.kind));

            ImGui::TableSetColumnIndex(3);
            if (r.group.empty()) ImGui::TextDisabled("\u2014");
            else                 ImGui::TextUnformatted(r.group.c_str());

            ImGui::TableSetColumnIndex(4);
            // Addresses are hex: fixed-width so the column reads as a column.
            {
                const bool mono = push_mono();
                ImGui::Text("0x%llX", static_cast<unsigned long long>(r.runtime_va));
                pop_mono(mono);
            }

            // ── static site, named by the image's own symbol table ──────────
            // (the boot-stable half of the row; `module+0xNNN` is on the tooltip)
            SiteIdentity si;
            if (r.kind == MemKind::Unknown || r.identity.canonical.empty()) {
                ImGui::TableSetColumnIndex(5);
                ImGui::TextDisabled("runtime only");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("no static anchor: this row is only meaningful for this "
                                      "session, because there is nothing static for its name "
                                      "to be derived from");
                ImGui::TableSetColumnIndex(6);
                ImGui::TextDisabled("\u2014");
            } else {
                std::string mod;
                uint64_t rva = 0;
                if (m_object_map.modules().rva_of(r.runtime_va, &mod, &rva))
                    symbol_evidence(mod, rva, &si);
                ImGui::TableSetColumnIndex(5);
                if (si.symbol.empty())
                    ImGui::TextDisabled("%s",
                        m_object_map.modules().describe(r.runtime_va).c_str());
                else
                    ImGui::TextDisabled("%s", si.symbol.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\nsymbol evidence only \u2014 the identity stays short "
                                      "and build-independent",
                                      m_object_map.modules().describe(r.runtime_va).c_str());
                ImGui::TableSetColumnIndex(6);
                if (si.between.empty()) ImGui::TextDisabled("\u2014");
                else                    ImGui::TextDisabled("%s", si.between.c_str());
            }

            // ── value: edit -> explicit set / cancel, dec/hex aware ───────────────────────
            ImGui::TableSetColumnIndex(7);
            if (m_addr_editing == r.id) {
                draw_value_editor(r.id, r, mem, writable, mem_size,
                                  m_addr_edit, sizeof(m_addr_edit),
                                  &m_addr_edit_domain, &m_addr_edit_row_domain,
                                  &m_addr_edit_dirty, &m_addr_status,
                                  &m_addr_edit_error, &m_addr_edit_counterpart,
                                  110.0f, /*compact=*/false);
                if (m_value_edit_over) { m_addr_editing = 0; m_value_edit_over = false; }
            } else {
                {
                    const bool mono = push_mono();
                    ImGui::TextUnformatted(m_addresses.read_value(r.id, mem, mem_size).c_str());
                    pop_mono(mono);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(ICON_FA_PEN "##ed")) {
                    m_addr_editing         = r.id;
                    // Seed with the EDITABLE form, never with read_value(): that
                    // is a rendering ("32 (0x20)") and cannot be parsed back.
                    m_addr_edit_domain     = r.domain;
                    m_addr_edit_row_domain = r.domain;
                    m_addr_edit_dirty      = false;
                    m_addr_edit_error.clear();
                    m_addr_edit_counterpart.clear();
                    const std::string seed =
                        m_addresses.read_editable(r.id, mem, mem_size, r.domain);
                    std::snprintf(m_addr_edit, sizeof(m_addr_edit), "%s", seed.c_str());
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("edit this value: explicit set / cancel, a live hex "
                                      "counterpart as you type, and a reason if refused");
            }

            // ── why / remove ────────────────────────────────────────────────
            ImGui::TableSetColumnIndex(8);
            if (ImGui::SmallButton("why?")) {
                m_addr_why_id   = r.id;
                m_addr_why_name = r.label();
                m_addr_why_text = m_workspace.explain(r.identity.var_id);
                if (m_addr_why_text.empty())
                    m_addr_why_text = "no explanation recorded for this identity";
                m_addr_why_open = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("show the evidence behind this row's identity and name");
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) m_addresses.remove(r.id);

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!m_addr_status.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_addr_status.c_str());
    }

    // ── "why?" panel ───────────────────────────────────────────────────────
    // A real panel rather than a status line: the explanation is multi-line and
    // is the whole point of the identity model, so it gets room to be read.
    // NOTE: End() is paired with Begin() even when Begin() returns false, so the
    // two are never conditionally crossed.
    if (m_addr_why_open) {
        if (ImGui::Begin("Why this identity?", &m_addr_why_open,
                         ImGuiWindowFlags_AlwaysAutoResize)) {
            if (const AddressEntry* r = m_addresses.find(m_addr_why_id)) {
                ImGui::TextColored(provenance_colour(r->provenance), "%s  %s",
                                   provenance_badge(r->provenance), r->label().c_str());
                ImGui::Separator();
                ImGui::TextDisabled("identity      %s", hex_identity(r->identity.var_id).c_str());
                ImGui::TextDisabled("tier          %s",
                                    r->is_known() ? "known (recovered/confirmed)"
                                    : (r->kind == MemKind::Unknown ? "runtime only (no static anchor)"
                                                                   : "unknown but identifiable"));
                ImGui::TextDisabled("kind          %s", mem_kind_name(r->kind));
                ImGui::TextDisabled("address       %s", hex_identity(r->runtime_va).c_str());
                ImGui::TextDisabled("static site   %s",
                                    m_object_map.modules().describe(r->runtime_va).c_str());
                ImGui::TextDisabled("instance      %s (%s)",
                                    r->tag.suffix_str().empty() ? "\u2014" : r->tag.suffix_str().c_str(),
                                    r->tag.source_name());
                ImGui::Separator();
                ImGui::TextWrapped("%s", m_addr_why_text.c_str());
                ImGui::Separator();
                if (ImGui::Button("close")) m_addr_why_open = false;
            } else {
                ImGui::TextDisabled("that row no longer exists");
                if (ImGui::Button("close")) m_addr_why_open = false;
            }
        }
        ImGui::End();
    }
}

// ---------------------------------------------------------------------------
// draw_value_editor — the only place a value gets typed
//
// The original bug lived here, in two copies: the box was seeded with
// read_value(), which returns a RENDERING ("32 (0x20)"), and the parser then
// rejected that same string — so pressing set always failed, the editor closed,
// and the cell redisplayed the old value.  From the outside that is
// indistinguishable from "writes don't work".
//
// Three rules keep it from coming back:
//   1. Seed from read_editable(): bare text that parses back to itself.
//   2. Never *guess* a domain — the operator picks dec or hex, and the toggle
//      converts the number rather than reinterpreting the digits.
//   3. Ask the same validator the write uses whether set should even be enabled,
//      and show its reason when it shouldn't.  A disabled button with no
//      explanation is the other half of the same UX failure.
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_value_editor(uint64_t row_id, const AddressEntry& e,
                                          const uint8_t* readable, uint8_t* writable,
                                          uint64_t mem_size,
                                          char* buf, size_t buf_size,
                                          ValueDomain* domain, ValueDomain* row_domain,
                                          bool* dirty, std::string* outcome,
                                          std::string* error,
                                          std::string* counterpart,
                                          float field_width, bool compact) {
    ImGui::PushID("vedit");

    // ── decimal / hex toggle ────────────────────────────────────────────────
    // Toggling converts the value; it never reinterprets the digits.  Typing
    // "20" and then flipping to hex and getting 32 would be a silent lie about
    // what is in memory, so the text is re-rendered from the parsed number
    // instead.  An unparseable box is left exactly as typed, with a reason.
    const ValueDomain before = *domain;
    const float toggle_w = compact ? 46.0f : 52.0f;
    if (ImGui::Button(*domain == ValueDomain::Hex ? "hex" : "dec", ImVec2(toggle_w, 0)))
        *domain = (*domain == ValueDomain::Hex) ? ValueDomain::Decimal : ValueDomain::Hex;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("read and write this value in %s\n\ntoggling converts the "
                          "number, it does not reinterpret the digits",
                          *domain == ValueDomain::Hex ? "decimal" : "hex");
    if (*domain != before) {
        const LiteralParse p = parse_literal(buf, before);
        if (p.ok) {
            const bool to_hex = (*domain == ValueDomain::Hex);
            char conv[64];
            const size_t w = value_type_size(e.type);
            const uint64_t u = (!value_type_is_float(e.type) && w && w < 8)
                ? (static_cast<uint64_t>(p.num.i) & ((1ull << (w * 8)) - 1ull))
                : static_cast<uint64_t>(p.num.i);
            if (to_hex && !value_type_is_float(e.type))
                std::snprintf(conv, sizeof(conv), "0x%llX",
                              static_cast<unsigned long long>(u));
            else
                std::snprintf(conv, sizeof(conv), "%g", p.num.as_double());
            std::snprintf(buf, buf_size, "%s", conv);
            error->clear();
        } else {
            *error = std::string("cannot convert '") + buf + "' to " +
                     value_domain_name(*domain) + " \u2014 fix the value first";
        }
        *dirty = true;
    }
    ImGui::SameLine();

    // ── the field ───────────────────────────────────────────────────────────
    // Mono here is not decoration: digits that keep their column while you type
    // are the difference between reading a value and re-reading it.
    const bool mono = push_mono();
    ImGui::SetNextItemWidth(field_width);
    const bool text_edited = ImGui::InputText("##v", buf, buf_size,
                                              ImGuiInputTextFlags_EnterReturnsTrue);
    const bool focused = ImGui::IsItemActive() || ImGui::IsItemFocused();
    if (text_edited) *dirty = true;

    // Live counterpart, computed from the characters currently on screen rather
    // than from the stored value, so it tracks the typing.
    *counterpart = literal_counterpart(buf, *domain, e.type);

    // Live validation against the *same* rules the write will use.
    std::string verdict;
    const bool valid = m_addresses.validate_value(row_id, buf, *domain, &verdict);
    if (!valid) *error = verdict;
    else if (focused || *dirty) error->clear();

    ImGui::SameLine();
    ImGui::BeginDisabled(!valid);
    const bool commit_click = ImGui::SmallButton(ICON_FA_FLOPPY_DISK " set");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(valid ? "write this value into guest memory"
                                : "cannot write yet \u2014 see the reason below");
    ImGui::SameLine();
    const bool cancel = ImGui::SmallButton("cancel");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("close the editor without writing anything");

    const bool commit = (commit_click || text_edited) && valid;

    ImGui::Spacing();
    // Counterpart + verdict: one dim line, because this is the line that tells
    // the operator their 32 is 0x20 and that a Byte field will take it.
    if (!counterpart->empty()) {
        ImGui::TextDisabled("= %s", counterpart->c_str());
        ImGui::SameLine();
    }
    pop_mono(mono);   // the verdict line below reads better proportionally
    if (!valid) {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.42f, 1.0f), "%s", error->c_str());
    } else if (e.field_bound_end != 0 && (focused || commit_click)) {
        // Telling the operator the field extent BEFORE they write is the whole
        // reason the spill rule exists; discovering it by refusal is too late.
        ImGui::TextDisabled("field ends at +0x%llX \u2014 a %zu-byte write fits",
                            static_cast<unsigned long long>(e.field_bound_end),
                            value_type_size(e.type));
    }

    // ── commit / cancel ────────────────────────────────────────────────────
    if (commit) {
        std::string err;
        if (m_addresses.set_value(row_id, buf, writable, mem_size, &err, *domain)) {
            // Commit the base as the row's own, so the read-only cell agrees
            // with what the operator just did.
            m_addresses.set_domain(row_id, *domain);
            if (row_domain) *row_domain = *domain;
            // Report the value as it will actually read back, not as typed, so
            // a written "0x20" for a byte says "32", not "0x20" forever.
            const std::string back = m_addresses.read_editable(row_id, readable, mem_size,
                                                              *domain);
            *outcome = e.label() + " \u2190 " +
                       (back.empty() ? std::string(buf) : back) +
                       " (" + (readable ? m_addresses.read_value(row_id, readable, mem_size)
                                        : std::string()) + ")";
            error->clear();
            m_value_edit_over = true;      // the caller clears its own row handle
        } else {
            // A refusal keeps the editor OPEN.  The old behaviour closed it and
            // reverted the display, which hid the reason completely.
            *error = err.empty() ? std::string("write refused") : err;
        }
    } else if (cancel) {
        *outcome = "edit cancelled \u2014 nothing was written";
        error->clear();
        *dirty = false;
        m_value_edit_over = true;
    }

    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// draw_leads — the ranked list of research leads
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_leads() {
    xpera::section_header("Research leads",
                          "what is left over, ranked by how it moves with the scene");
    ImGui::Spacing();

    ImGui::TextWrapped(
        "Everything below is memory that is NOT covered by any recovered field, live object or "
        "address-list row.  Movement is sampled between visits, so the scene figure is an "
        "association rather than a cause \u2014 both numbers are shown so you can judge.");
    ImGui::Spacing();

    if (!m_explore_enabled) {
        empty_state("Exploration is off.",
                    "Turn it on to sweep the unclaimed address space in small, bounded passes.  "
                    "Set the region first: by default it follows the registered image, which is "
                    "where globally-referenced state lives.");
    }

    // ── Controls ─────────────────────────────────────────────────────────
    bool enabled = m_explore_enabled;
    if (ImGui::Checkbox("explore unclaimed memory", &enabled)) {
        m_explore_enabled = enabled;
        if (enabled && !m_explorer.tracked()) m_explore_dirty = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    if (ImGui::Combo("##etype", &m_explore_type, kValueTypeNames, kValueTypeCount))
        m_explore_started = false;   // reshaping invalidates the sample baseline
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    int every = static_cast<int>(m_explore_every_frames);
    if (ImGui::InputInt("##every", &every, 1, 4)) {
        if (every < 1) every = 1;
        if (every > 600) every = 600;
        m_explore_every_frames = static_cast<uint32_t>(every);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("frames between passes");

    // Region: defaults to the registered image so globally-referenced state is
    // in scope, which is what the module map makes addressable in the first place.
    uint64_t def_begin = 0, def_end = 0;
    const bool have_module_range = module_scan_range(&def_begin, &def_end);
    if (!m_explore_region_begin && have_module_range) {
        m_explore_region_begin = def_begin;
        m_explore_region_end   = def_end;
    }

    if (ImGui::Button("Reset exploration")) {
        m_explorer.reset();
        m_explore_leads.clear();
        m_explore_status.clear();
        m_explore_started = false;
        m_explore_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Recalculate ranking")) m_explore_dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("use image range") && have_module_range) {
        m_explore_region_begin = def_begin;
        m_explore_region_end   = def_end;
        m_explore_started = false;   // reconfigured on the next pass
    }
    ImGui::SameLine();
    if (!have_module_range)
        ImGui::TextDisabled("(no image registered \u2014 \"use image range\" unavailable)");

    ImGui::Spacing();
    ImGui::TextDisabled("%s", m_explorer.summary().c_str());
    if (!m_explore_status.empty()) ImGui::TextDisabled("%s", m_explore_status.c_str());

    // Recompute on demand, never per frame: ranking a large tracked set is not
    // something to do 60 times a second for a list nobody is scrolling.
    if (m_explore_dirty) {
        m_explore_leads = m_explorer.rank(m_object_map.modules(),
                                         "sre13-1.4.13-arm64", 200);
        m_explore_dirty = false;
    }

    ImGui::Spacing();
    xpera::divider();

    if (m_explore_leads.empty()) {
        if (!m_explore_enabled) {
            empty_state("No leads yet.", "Exploration is off \u2014 enable it above.");
        } else if (m_explorer.tracked() == 0) {
            empty_state(
                "No leads yet.",
                "Nothing unclaimed has moved between the visits made so far.  Play the game a "
                "little (open a menu, take damage, change level) and the moving values will "
                "surface; the sweep also needs a full lap of the region before a slot is "
                "visited twice, which is the earliest a change can be seen.");
        } else {
            empty_state("No leads yet.",
                        "Slots are tracked but none has been sampled twice yet.  Let it run.");
        }
        return;
    }

    ImGui::Text("%zu lead(s), best first  |  %zu tracked slot(s)",
                m_explore_leads.size(), m_explorer.tracked());
    ImGui::Spacing();

    if (ImGui::BeginTable("##leads", 6, kGridFlags, ImVec2(0, -1))) {
        ImGui::TableSetupColumn("score", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("identity", ImGuiTableColumnFlags_WidthFixed, 190);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 170);
        ImGui::TableSetupColumn("tier", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("movement", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("why", ImGuiTableColumnFlags_WidthStretch, 320);
        ImGui::TableHeadersRow();

        for (const auto& l : m_explore_leads) {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(l.address & 0x7FFFFFFF));

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%.2f", l.score);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(provenance_colour(l.evidence), "%s %s",
                               provenance_badge(l.evidence), l.display_name.c_str());
            ImGui::TextDisabled(l.mapped ? "%s+0x%llX" : "%s (no static home)",
                                l.mapped ? l.module.c_str() : "session-scoped",
                                static_cast<unsigned long long>(l.mapped ? l.rva : l.address));

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(l.value_text.c_str());
            ImGui::TextDisabled("@ 0x%llX", static_cast<unsigned long long>(l.address));

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(provenance_name(l.evidence));

            ImGui::TableSetColumnIndex(4);
            ImGui::TextDisabled("%u change(s) / %u sample(s)", l.changes, l.samples);
            ImGui::TextDisabled("scene %u   rebuild %u", l.scene_changes, l.rebuild_changes);

            ImGui::TableSetColumnIndex(5);
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 460.0f);
            ImGui::TextWrapped("%s", l.why.c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::SmallButton("add to address list")) {
                StaticRef ref;
                ref.build_id = "sre13-1.4.13-arm64";
                ref.module   = l.module;
                ref.kind     = l.mapped ? MemKind::Global : MemKind::Unknown;
                ref.container_rva = l.mapped ? l.rva : 0;
                // NOTE: the runtime address is deliberately NOT written into
                // `offset`.  Offset is part of the static key, so seeding it with
                // a transient address would make the identity change every boot —
                // the exact failure this whole layer exists to prevent.
                ref.type_name = value_type_name(l.type);
                ref.provenance = l.evidence;
                InstanceTag tag;
                if (!l.mapped) {
                    tag.value  = IdentityResolver::stable_tag_value(std::to_string(l.address));
                    tag.source = TagSource::Session;
                }
                const uint64_t id = m_addresses.add(ref, l.address, l.type,
                                                    "lead (unclaimed)", 0, tag);
                m_addr_status = "lead added to the address list as a " +
                                std::string(provenance_name(l.evidence)) +
                                " candidate" +
                                (l.mapped ? "" : " (no static home \u2014 the name is session-scoped)");
                (void)id;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("open in hex view")) {
                m_hex_target_va = l.address;
                m_active_section = 3;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    m_explore_leads_shown = static_cast<int>(m_explore_leads.size());
}

// ---------------------------------------------------------------------------
// draw_evidence
// ---------------------------------------------------------------------------
void MemoryResearchTab::draw_evidence() {
    xpera::section_header("Instruction evidence",
                          "PC + operand correlation: evidence, never proof");
    ImGui::Spacing();

    if (m_workspace_checked && !m_recovery_banner.empty()) {
        xpera::begin_card("##recovery");
        ImGui::TextColored(ImVec4(0.45f, 0.75f, 0.95f, 1.0f), "WELCOME BACK");
        ImGui::TextWrapped("%s", m_recovery_banner.c_str());
        ImGui::TextDisabled("Identities are unchanged; only runtime tags are session-scoped.");
        xpera::end_card();
        ImGui::Spacing();
    }

    ImGui::TextWrapped("%s", m_access_trace.summary().c_str());
    ImGui::Spacing();

    std::string why;
    const bool installed = install_memory_access_hooks(nullptr, &why);
    xpera::status_pill("per-instruction hooks", installed);
    ImGui::SameLine();
    if (!installed) ImGui::TextDisabled("%s", why.c_str());

    ImGui::Spacing();
    xpera::divider();

    const auto& cands = m_access_trace.candidates();
    if (cands.empty()) {
        ImGui::TextDisabled("No instruction-correlated fields yet.");
        ImGui::TextDisabled("This needs the Dynarmic per-instruction memory hook (see above);");
        ImGui::TextDisabled("until then, the decoder below can be used on a raw instruction.");
    } else if (ImGui::BeginTable("##evi", 5, kGridFlags, ImVec2(0, 240))) {
        ImGui::TableSetupColumn("evidence", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("site", ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableSetupColumn("offset", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("width", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthStretch, 320);
        ImGui::TableHeadersRow();
        for (const auto& c : cands) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(provenance_colour(c.evidence), "%s", provenance_name(c.evidence));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s+0x%llX", c.module.c_str(),
                                static_cast<unsigned long long>(c.pc_rva));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("+0x%llX", static_cast<unsigned long long>(c.offset));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%llu", static_cast<unsigned long long>(c.width));
            ImGui::TableSetColumnIndex(4);
            ImGui::TextWrapped("%s", c.note.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    xpera::section_header("Instruction decoder", "paste a raw AArch64 word");
    static char insn_text[32] = "B9009001";
    ImGui::SetNextItemWidth(140);
    ImGui::InputTextWithHint("##insn", "hex word", insn_text, sizeof(insn_text));
    ImGui::SameLine();
    uint64_t word = 0;
    if (parse_address(insn_text, &word) && word <= 0xFFFFFFFFull) {
        const DecodedAccess d = decode_arm64_access(static_cast<uint32_t>(word));
        if (d.ok) {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.60f, 1.0f), "%s", d.mnemonic.c_str());
            ImGui::TextDisabled("base %s   offset +0x%llX   width %llu B%s%s%s",
                                d.base_reg < 31 ? "Xn" : "sp/xzr",
                                static_cast<unsigned long long>(d.offset),
                                static_cast<unsigned long long>(d.width),
                                d.is_store ? "   STORE" : "",
                                d.is_load ? "   LOAD" : "",
                                d.register_offset ? "   register offset (not an immediate)" : "");
        } else {
            ImGui::TextDisabled("not a load/store instruction we decode \u2014 refusing to guess");
        }
    }

    ImGui::Spacing();
    xpera::divider();
    ImGui::TextDisabled("%s", m_workspace_status.c_str());
}

} // namespace swordfare::research
