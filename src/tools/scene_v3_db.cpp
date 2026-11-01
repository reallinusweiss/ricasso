/*
 * scene_v3_db.cpp — embedded v3 biome database + minimal JSON reader.
 *
 * The JSON below (kSceneV3BiomesJson) is a verbatim copy of
 * src/tools/scene_v3_biomes.json, baked in as a raw string so the runtime
 * never depends on an external file. A tiny, dependency-free JSON parser
 * (sufficient for this specific document) turns it into the V3BiomeSpec table.
 */
#include "tools/scene_v3_db.h"

#include <cstdlib>
#include <cctype>
#include <cstring>

namespace sgen {
namespace v3 {

// ── Embedded JSON (verbatim scene_v3_biomes.json) ───────────────────────────
static const char* kSceneV3BiomesJson =
R"V3JSON({
  "schema": "swordigo.scene_gen.v3",
  "version": 4,
  "notes": "Per-biome design DB derived from real vanilla decoded scenes + .scl template component analysis. deco_* lists contain ONLY pure Model decorations (verified Model-only in the .scl libs) — NOT breakables (pot/stonepile), NOT physics/ground props, NOT monsters (carniplant is a MonsterEntity). 'bush' is the canonical vanilla scatter bush and is kept. tex_top = GroundMesh top face, tex_front = cliff/side. See v3_research_notes.md.",
  "common": {
    "bg_plane_depth": 1.72038269,
    "ground_depth_band": { "min_depth": -112.0, "max_depth": 112.0 },
    "terrain_seed_default": 1291618994
  },
  "biomes": [
    {
      "id": 0, "name": "Grasslands",
      "background": "grasslandsbackground_day",
      "background_alt": ["grassbg_night"],
      "tex_top": "grass_grass", "tex_front": "maybegood", "tex_bottom": "maybegood",
      "deco_deep_bg": ["tree", "tree2", "hugerock1"],
      "deco_play": ["bush", "rock1", "smallrock1", "pot"],
      "deco_fg": ["bush", "smallrock1"],
      "torch_pod": "torch",
      "entities": ["Stag Beetle", "bat_blue_strike", "bandit", "varibandit_thrower"],
      "light": { "key_intensity": 1.2, "ambient_intensity": 0.3, "key_color": [1,1,1,1] },
      "torch": { "radius": 350, "intensity": 2.0, "glow": [1.0, 0.85, 0.5, 0.5] },
      "water": { "enabled": true, "front_rgba": [0.0,0.314,0.233,0.744], "surface_rgba": [0.0,0.376,0.256,0.744] },
      "cave": false,
      "breakables": ["pot", "board"],
      "collectibles": ["nugget_health", "nugget_mana", "sshard_blue"],
      "particles": [],
      "fire_fx": [],
      "sound_ambient": "",
      "moving_platforms": ["platformwood0", "platformwood1"],
      "portal_dests": ["plains_part2", "plains_part3", "forest_cave0"],
      "ambient_lights": [
        { "x_offset": 0, "radius": 350, "intensity": 2.0, "color": [1.0,0.85,0.5,1] },
        { "x_offset": 700, "radius": 250, "intensity": 1.5, "color": [1.0,0.8,0.5,1] }
      ]
    },
    {
      "id": 1, "name": "Forest",
      "background": "forest_background",
      "tex_top": "forest_grass", "tex_front": "forest_ground", "tex_bottom": "forest_ground",
      "deco_deep_bg": ["grove_tree1", "grove_tree2", "grove_tree3"],
      "deco_play": ["bush", "rock1", "smallrock1"],
      "deco_fg": ["bush", "smallrock1"],
      "torch_pod": "torch",
      "entities": ["rolling_spirit", "piikkikonna", "dwarf", "carniplant"],
      "light": { "key_intensity": 3.0, "ambient_intensity": 0.3, "key_color": [1,1,1,1] },
      "torch": { "radius": 350, "intensity": 2.0, "glow": [1.0, 0.6, 0.3, 0.5] },
      "water": { "enabled": false },
      "cave": false,
      "breakables": ["pot", "board", "stonepile"],
      "collectibles": ["nugget_health", "nugget_mana"],
      "particles": [],
      "fire_fx": [],
      "sound_ambient": "",
      "moving_platforms": ["platformwood0", "dropping_groundpiece"],
      "portal_dests": ["forest_cave2", "forest_cave3", "grove_part1"],
      "ambient_lights": [
        { "x_offset": 0, "radius": 350, "intensity": 2.0, "color": [1.0,0.6,0.3,1] },
        { "x_offset": 600, "radius": 250, "intensity": 1.5, "color": [1.0,0.6,0.3,1] }
      ]
    },
    {
      "id": 2, "name": "Grove",
      "background": "grove_bg",
      "tex_top": "forest_ground", "tex_front": "forest_grass", "tex_bottom": "grove_ground",
      "deco_deep_bg": ["grove_tree1", "grove_tree2", "grove_tree3", "grove_pole2"],
      "deco_play": ["bush", "grove_hang1", "grove_hang3"],
      "deco_fg": ["bush", "grove_hang3"],
      "torch_pod": "grove_torch",
      "entities": ["rolling_spirit", "forest_spirit", "piikkikonna"],
      "light": { "key_intensity": 1.0, "ambient_intensity": 0.2, "key_color": [1,1,1,1] },
      "torch": { "radius": 350, "intensity": 2.0, "glow": [0.55, 0.7, 1.0, 0.5] },
      "water": { "enabled": false },
      "cave": false,
      "breakables": ["pot"],
      "collectibles": ["nugget_health", "nugget_mana", "sshard_red"],
      "particles": [],
      "fire_fx": [],
      "sound_ambient": "",
      "moving_platforms": ["grove_platform1"],
      "portal_dests": ["grove_crypt1", "lowergrove_part1", "forest_part1"],
      "ambient_lights": [
        { "x_offset": 0, "radius": 350, "intensity": 2.0, "color": [0.55,0.7,1.0,1] },
        { "x_offset": 800, "radius": 350, "intensity": 2.0, "color": [0.55,0.7,1.0,1] }
      ]
    },
    {
      "id": 3, "name": "Wasteland",
      "background": "wasteland_bg",
      "tex_top": "wasteland_ground", "tex_front": "grass_orange", "tex_bottom": "grass_orange",
      "deco_deep_bg": ["deadtree1", "deadtree2"],
      "deco_play": ["rock1", "rock2", "smallrock1"],
      "deco_fg": ["smallrock1"],
      "torch_pod": "torch",
      "entities": ["beetle_wasteland", "grasswalker_wasteland"],
      "light": { "key_intensity": 3.0, "ambient_intensity": 0.5, "key_color": [1,1,1,1] },
      "torch": { "radius": 180, "intensity": 2.0, "glow": [1.0, 0.5, 0.2, 0.5] },
      "water": { "enabled": false },
      "cave": false,
      "breakables": ["pot", "stonepile"],
      "collectibles": ["nugget_health", "nugget_mana", "sshard_yellow"],
      "particles": [],
      "fire_fx": [],
      "sound_ambient": "",
      "moving_platforms": ["platformwood0", "dropping_groundpiece"],
      "portal_dests": ["wasteland_cave", "wasteland_town", "wasteland_part2"],
      "ambient_lights": [
        { "x_offset": 0, "radius": 180, "intensity": 2.0, "color": [1.0,0.5,0.2,1] },
        { "x_offset": 600, "radius": 180, "intensity": 2.0, "color": [1.0,0.5,0.2,1] }
      ]
    },
    {
      "id": 4, "name": "IceCastle",
      "background": "atlon",
      "tex_top": "icecastle_ground", "tex_front": "icicle", "tex_bottom": "icecastle_floor",
      "deco_deep_bg": ["snowy_tree", "snowy_smalltree"],
      "deco_play": ["icicles1", "icicles"],
      "deco_fg": ["icicles1"],
      "torch_pod": "torch",
      "entities": ["skeleton_spell_cast_frost"],
      "light": { "key_intensity": 2.0, "ambient_intensity": 0.2, "key_color": [0.8,0.9,1.0,1] },
      "torch": { "radius": 350, "intensity": 2.0, "glow": [0.6, 0.8, 1.0, 0.5] },
      "water": { "enabled": false },
      "cave": false,
      "breakables": ["pot", "stonepile"],
      "collectibles": ["nugget_health", "nugget_mana"],
      "particles": [],
      "fire_fx": [],
      "sound_ambient": "",
      "moving_platforms": ["flying_platform", "dropping_groundpiece"],
      "portal_dests": ["snowy_cave1", "snowy_cave2", "worldsend_part1"],
      "ambient_lights": [
        { "x_offset": 0, "radius": 350, "intensity": 2.0, "color": [0.6,0.8,1.0,1] },
        { "x_offset": 500, "radius": 250, "intensity": 1.5, "color": [0.6,0.8,1.0,1] }
      ]
    },
    {
      "id": 5, "name": "Cave",
      "background": "cavesbackground2",
      "tex_top": "brown128", "tex_front": "cavewalls", "tex_bottom": "cavewalls",
      "tex_wall": "cavewalls", "tex_wall_alt": "cavewalls2",
      "deco_deep_bg": ["board"],
      "deco_play": ["rock1", "smallrock1", "pot"],
      "deco_fg": ["board"],
      "torch_pod": "torch",
      "entities": ["dire_cavelurker", "cavelurker", "beetle_wasteland", "bat"],
      "light": { "key_intensity": 2.0, "ambient_intensity": 0.3, "key_color": [1,1,1,1] },
      "torch": { "radius": 350, "intensity": 2.0, "glow": [1.0, 0.6, 0.3, 0.5] },
      "water": { "enabled": true, "front_rgba": [0.0,0.314,0.233,0.744], "surface_rgba": [0.0,0.376,0.256,0.744] },
      "cave": true,
      "cave_rules": { "enclosed": true, "ceiling": true, "floor_tex_top": "brown128", "wall_texture": "cavewalls", "wall_texture_alt": "cavewalls2" },
      "breakables": ["pot", "board"],
      "collectibles": ["nugget_health", "nugget_mana"],
      "particles": [],
      "fire_fx": [],
      "sound_ambient": "",
      "moving_platforms": [],
      "portal_dests": ["forest_cave0", "forest_cave4", "thecave_part1"],
      "ambient_lights": [
        { "x_offset": 0, "radius": 350, "intensity": 2.0, "color": [1.0,0.6,0.3,1] }
      ]
    },
    {
      "id": 6, "name": "Fire",
      "background": "cavesbackground2",
      "tex_top": "fire_grass", "tex_front": "graveyard_ground", "tex_bottom": "graveyard_ground",
      "deco_deep_bg": ["stonepillairs3", "stonepillairs4"],
      "deco_play": ["stonepillairs", "rock1"],
      "deco_fg": ["smallrock1"],
      "torch_pod": "torch",
      "entities": ["magmamonster"],
      "light": { "key_intensity": 3.0, "ambient_intensity": 0.4, "key_color": [1.0,0.7,0.5,1] },
      "torch": { "radius": 350, "intensity": 2.0, "glow": [1.0, 0.4, 0.1, 0.5] },
      "water": { "enabled": true, "front_rgba": [0.6,0.1,0.0,0.744], "surface_rgba": [0.8,0.2,0.0,0.744] },
      "cave": false,
      "breakables": ["pot", "stonepile"],
      "collectibles": ["nugget_health", "nugget_mana", "sshard_red"],
      "particles": ["shadowblob_fire_little"],
      "fire_fx": ["shadowblob_fire_little"],
      "sound_ambient": "",
      "moving_platforms": [],
      "portal_dests": ["fire_part1", "fire_part2", "fire_part31"],
      "ambient_lights": [
        { "x_offset": 0, "radius": 250, "intensity": 2.5, "color": [1.0,0.4,0.1,1] },
        { "x_offset": 500, "radius": 250, "intensity": 2.0, "color": [1.0,0.3,0.05,1] }
      ]
    },
    {
      "id": 7, "name": "Florennum",
      "background": "florennum_night_bg",
      "tex_top": "florennum_ground", "tex_front": "florennum_ground", "tex_bottom": "florennum_ground",
      "deco_deep_bg": ["grove_tree1", "grove_tree3"],
      "deco_play": ["bush", "rock1"],
      "deco_fg": ["bush"],
      "torch_pod": "torch",
      "entities": [],
      "light": { "key_intensity": 1.2, "ambient_intensity": 0.3, "key_color": [1.0,0.95,0.8,1] },
      "torch": { "radius": 150, "intensity": 2.0, "glow": [1.0, 0.85, 0.6, 0.5] },
      "water": { "enabled": false },
      "cave": false,
      "breakables": ["pot", "stonepile"],
      "collectibles": ["nugget_health", "nugget_mana", "sshard_yellow"],
      "particles": [],
      "fire_fx": [],
      "sound_ambient": "",
      "moving_platforms": ["elevator1"],
      "portal_dests": ["florennum_part1", "florennum_tower1", "florennum_jail_part1"],
      "ambient_lights": [
        { "x_offset": 0, "radius": 150, "intensity": 2.0, "color": [1.0,0.85,0.6,1] },
        { "x_offset": 400, "radius": 150, "intensity": 1.5, "color": [1.0,0.8,0.5,1] }
      ]
    }
  ]
}
)V3JSON";

const char* v3_biomes_json() { return kSceneV3BiomesJson; }

// ── Minimal JSON parser (objects/arrays/strings/numbers/bools/null) ─────────
namespace {

struct JParser {
    const char* p;
    explicit JParser(const char* s) : p(s) {}

    void ws() { while (*p && (*p==' '||*p=='\t'||*p=='\n'||*p=='\r')) ++p; }

    void skip_value() {
        ws();
        if (*p == '{') { skip_object(); }
        else if (*p == '[') { skip_array(); }
        else if (*p == '"') { skip_string(); }
        else { while (*p && *p!=','&&*p!='}'&&*p!=']') ++p; }
    }
    void skip_string() {
        // assumes *p == '"'
        ++p;
        while (*p && *p != '"') { if (*p=='\\'&&p[1]) ++p; ++p; }
        if (*p == '"') ++p;
    }
    void skip_object() {
        ++p; ws();
        while (*p && *p != '}') {
            ws(); if (*p=='"') skip_string(); ws();
            if (*p==':') ++p;
            skip_value(); ws();
            if (*p==',') { ++p; ws(); }
        }
        if (*p=='}') ++p;
    }
    void skip_array() {
        ++p; ws();
        while (*p && *p != ']') { skip_value(); ws(); if (*p==',') { ++p; ws(); } }
        if (*p==']') ++p;
    }

    std::string parse_string() {
        std::string out;
        if (*p != '"') return out;
        ++p;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) {
                ++p;
                switch (*p) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    default: out += *p; break;
                }
                ++p;
            } else { out += *p++; }
        }
        if (*p == '"') ++p;
        return out;
    }
    double parse_number() {
        char* end = nullptr;
        double v = std::strtod(p, &end);
        if (end) p = end;
        return v;
    }
    bool parse_bool() {
        if (!std::strncmp(p, "true", 4)) { p += 4; return true; }
        if (!std::strncmp(p, "false", 5)) { p += 5; return false; }
        // null or unknown
        while (*p && *p!=','&&*p!='}'&&*p!=']') ++p;
        return false;
    }

    // Fill a string vector from the current array value.
    void parse_string_array(std::vector<std::string>& out) {
        ws(); if (*p != '[') { skip_value(); return; }
        ++p; ws();
        while (*p && *p != ']') {
            ws();
            if (*p == '"') out.push_back(parse_string());
            else skip_value();
            ws(); if (*p==',') { ++p; ws(); }
        }
        if (*p==']') ++p;
    }
    // Fill floats from a numeric array [a,b,...] up to n.
    void parse_float_array(float* out, int n) {
        ws(); if (*p != '[') { skip_value(); return; }
        ++p; ws(); int i = 0;
        while (*p && *p != ']') {
            ws();
            double v = parse_number();
            if (i < n) out[i] = (float)v;
            ++i; ws(); if (*p==',') { ++p; ws(); }
        }
        if (*p==']') ++p;
    }
};

// Parse one biome object into spec. Assumes parser positioned at '{'.
void parse_biome(JParser& jp, V3BiomeSpec& b) {
    jp.ws(); if (*jp.p != '{') { jp.skip_value(); return; }
    ++jp.p; jp.ws();
    while (*jp.p && *jp.p != '}') {
        jp.ws();
        std::string key = jp.parse_string();
        jp.ws(); if (*jp.p==':') ++jp.p; jp.ws();

        if (key == "name") b.name = jp.parse_string();
        else if (key == "background") b.background = jp.parse_string();
        else if (key == "tex_top") b.tex_top = jp.parse_string();
        else if (key == "tex_front") b.tex_front = jp.parse_string();
        else if (key == "tex_bottom") b.tex_bottom = jp.parse_string();
        else if (key == "tex_wall") b.tex_wall = jp.parse_string();
        else if (key == "torch_pod") b.torch_pod = jp.parse_string();
        else if (key == "deco_deep_bg") jp.parse_string_array(b.deco_deep_bg);
        else if (key == "deco_play") jp.parse_string_array(b.deco_play);
        else if (key == "deco_fg") jp.parse_string_array(b.deco_fg);
        else if (key == "platforms") jp.parse_string_array(b.platforms);
        else if (key == "entities") jp.parse_string_array(b.entities);
        else if (key == "cave") b.cave = jp.parse_bool();
        else if (key == "depth_bands") {
            // nested object { deep_bg:[..], play:[..], fg:[..] }
            jp.ws(); if (*jp.p=='{') {
                ++jp.p; jp.ws();
                while (*jp.p && *jp.p != '}') {
                    jp.ws(); std::string k2 = jp.parse_string();
                    jp.ws(); if (*jp.p==':') ++jp.p; jp.ws();
                    if (k2=="deep_bg") jp.parse_float_array(b.depth_deep_bg, 2);
                    else if (k2=="play") jp.parse_float_array(b.depth_play, 2);
                    else if (k2=="fg") jp.parse_float_array(b.depth_fg, 2);
                    else jp.skip_value();
                    jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
                }
                if (*jp.p=='}') ++jp.p;
            } else jp.skip_value();
        }
        else if (key == "light") {
            jp.ws(); if (*jp.p=='{') {
                ++jp.p; jp.ws();
                while (*jp.p && *jp.p != '}') {
                    jp.ws(); std::string k2 = jp.parse_string();
                    jp.ws(); if (*jp.p==':') ++jp.p; jp.ws();
                    if (k2=="key_intensity") b.key_intensity = (float)jp.parse_number();
                    else if (k2=="ambient_intensity") b.ambient_intensity = (float)jp.parse_number();
                    else if (k2=="key_color") jp.parse_float_array(b.key_color, 4);
                    else jp.skip_value();
                    jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
                }
                if (*jp.p=='}') ++jp.p;
            } else jp.skip_value();
        }
        else if (key == "torch") {
            jp.ws(); if (*jp.p=='{') {
                ++jp.p; jp.ws();
                while (*jp.p && *jp.p != '}') {
                    jp.ws(); std::string k2 = jp.parse_string();
                    jp.ws(); if (*jp.p==':') ++jp.p; jp.ws();
                    if (k2=="radius") b.torch_radius = (float)jp.parse_number();
                    else if (k2=="intensity") b.torch_intensity = (float)jp.parse_number();
                    else if (k2=="glow") jp.parse_float_array(b.torch_glow, 4);
                    else jp.skip_value();
                    jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
                }
                if (*jp.p=='}') ++jp.p;
            } else jp.skip_value();
        }
        else if (key == "water") {
            jp.ws(); if (*jp.p=='{') {
                ++jp.p; jp.ws();
                while (*jp.p && *jp.p != '}') {
                    jp.ws(); std::string k2 = jp.parse_string();
                    jp.ws(); if (*jp.p==':') ++jp.p; jp.ws();
                    if (k2=="enabled") b.water_enabled = jp.parse_bool();
                    else if (k2=="front_rgba") jp.parse_float_array(b.water_front, 4);
                    else if (k2=="surface_rgba") jp.parse_float_array(b.water_surface, 4);
                    else jp.skip_value();
                    jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
                }
                if (*jp.p=='}') ++jp.p;
            } else jp.skip_value();
        }
        else if (key == "cave_rules") {
            jp.ws(); if (*jp.p=='{') {
                ++jp.p; jp.ws();
                while (*jp.p && *jp.p != '}') {
                    jp.ws(); std::string k2 = jp.parse_string();
                    jp.ws(); if (*jp.p==':') ++jp.p; jp.ws();
                    if (k2=="top_equals_front") b.cave_top_equals_front = jp.parse_bool();
                    else if (k2=="shaft_depth_spread") jp.parse_float_array(b.shaft_spread, 2);
                    else jp.skip_value();
                    jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
                }
                if (*jp.p=='}') ++jp.p;
            } else jp.skip_value();
        }
        else if (key == "breakables") jp.parse_string_array(b.breakables);
        else if (key == "collectibles") jp.parse_string_array(b.collectibles);
        else if (key == "particles") jp.parse_string_array(b.particles);
        else if (key == "fire_fx") jp.parse_string_array(b.fire_fx);
        else if (key == "sound_ambient") b.sound_ambient = jp.parse_string();
        else if (key == "moving_platforms") jp.parse_string_array(b.moving_platforms);
        else if (key == "portal_dests") jp.parse_string_array(b.portal_dests);
        else if (key == "ambient_lights") {
            jp.ws(); if (*jp.p=='[') {
                ++jp.p; jp.ws();
                while (*jp.p && *jp.p != ']') {
                    jp.ws();
                    if (*jp.p == '{') {
                        ++jp.p; jp.ws();
                        V3BiomeSpec::PointLight pl;
                        while (*jp.p && *jp.p != '}') {
                            jp.ws(); std::string k2 = jp.parse_string();
                            jp.ws(); if (*jp.p==':') ++jp.p; jp.ws();
                            if (k2=="x_offset") pl.x_offset = (float)jp.parse_number();
                            else if (k2=="radius") pl.radius = (float)jp.parse_number();
                            else if (k2=="intensity") pl.intensity = (float)jp.parse_number();
                            else if (k2=="color") jp.parse_float_array(pl.color, 4);
                            else jp.skip_value();
                            jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
                        }
                        if (*jp.p=='}') ++jp.p;
                        b.ambient_lights.push_back(pl);
                    } else jp.skip_value();
                    jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
                }
                if (*jp.p==']') ++jp.p;
            } else jp.skip_value();
        }
        else {
            jp.skip_value();
        }
        jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
    }
    if (*jp.p=='}') ++jp.p;
    // Fallbacks so a biome is never fully empty.
    if (b.tex_bottom.empty()) b.tex_bottom = b.tex_front;
}

std::vector<V3BiomeSpec> build_table() {
    std::vector<V3BiomeSpec> out;
    JParser jp(kSceneV3BiomesJson);
    jp.ws();
    if (*jp.p != '{') return out;
    ++jp.p; jp.ws();
    // Find the top-level "biomes" array.
    while (*jp.p && *jp.p != '}') {
        jp.ws();
        std::string key = jp.parse_string();
        jp.ws(); if (*jp.p==':') ++jp.p; jp.ws();
        if (key == "biomes") {
            jp.ws();
            if (*jp.p == '[') {
                ++jp.p; jp.ws();
                while (*jp.p && *jp.p != ']') {
                    jp.ws();
                    if (*jp.p == '{') {
                        V3BiomeSpec b;
                        parse_biome(jp, b);
                        out.push_back(b);
                    } else jp.skip_value();
                    jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
                }
                if (*jp.p==']') ++jp.p;
            } else jp.skip_value();
        } else {
            jp.skip_value();
        }
        jp.ws(); if (*jp.p==',') { ++jp.p; jp.ws(); }
    }
    return out;
}

} // anonymous namespace

const std::vector<V3BiomeSpec>& v3_biome_db() {
    static const std::vector<V3BiomeSpec> kTable = build_table();
    return kTable;
}

const V3BiomeSpec& v3_biome(int id) {
    const std::vector<V3BiomeSpec>& t = v3_biome_db();
    static V3BiomeSpec fallback;
    if (t.empty()) return fallback;
    if (id < 0) id = 0;
    if (id >= (int)t.size()) id = (int)t.size() - 1;
    return t[id];
}

} // namespace v3
} // namespace sgen
