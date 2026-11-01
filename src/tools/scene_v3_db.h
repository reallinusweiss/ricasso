#pragma once
/*
 * scene_v3_db.h — v3 per-biome design database (baked from scene_v3_biomes.json).
 *
 * The JSON is embedded into scene_v3_db.cpp as a raw-string constant so the
 * Ruby runtime can never lose it (no runtime file read). It is parsed once,
 * lazily, into the V3BiomeSpec table below. Every value is derived from real
 * vanilla decoded scenes — see v3_research_notes.md.
 */
#include <string>
#include <vector>

namespace sgen {
namespace v3 {

struct V3BiomeSpec {
    std::string name;
    std::string background;
    std::string tex_top;      // GroundMesh top face (TextureMapping tex1)
    std::string tex_front;    // GroundMesh front/side cliff (TextureMapping tex2)
    std::string tex_bottom;
    std::string tex_wall;     // cave wall texture (may be empty)
    std::string torch_pod;    // "torch" or "grove_torch"

    std::vector<std::string> deco_deep_bg;  // distant parallax design elements
    std::vector<std::string> deco_play;     // walkable-layer decorations
    std::vector<std::string> deco_fg;        // near-camera decorations
    std::vector<std::string> platforms;      // moving/static platform pods
    std::vector<std::string> entities;       // per-biome monster/entity templates

    // ── v3.1: advanced vanilla-matching systems ───────────────────────────
    std::vector<std::string> breakables;    // breakable template pods (pot, board, etc.)
    std::vector<std::string> collectibles;  // collectable templates (nugget_health, etc.)
    std::vector<std::string> particles;     // ambient ParticleEmitter templates
    std::vector<std::string> fire_fx;       // fire emitter templates (fire biome)
    std::string              sound_ambient; // ambient SoundEffect template (e.g. "")
    std::vector<std::string> moving_platforms; // moving/elevator platform templates
    std::vector<std::string> portal_dests;  // default portal destinations for this biome

    // Point-light palette (vanilla scenes have many point lights for atmosphere)
    struct PointLight {
        float x_offset = 0.0f;        // relative to platform center
        float radius   = 250.0f;
        float intensity = 2.0f;
        float color[4] = {1.0f, 0.8f, 0.5f, 1.0f};
    };
    std::vector<PointLight> ambient_lights; // repeating point-light pattern along the level

    float depth_deep_bg[2] = {-200.0f, -50.0f}; // [lo,hi] negative Z band
    float depth_play[2]    = {0.0f, 40.0f};
    float depth_fg[2]      = {60.0f, 200.0f};

    float key_intensity     = 2.0f;
    float ambient_intensity = 0.3f;
    float key_color[4]      = {1.0f, 1.0f, 1.0f, 1.0f};

    float torch_radius    = 350.0f;
    float torch_intensity = 2.0f;
    float torch_glow[4]   = {1.0f, 0.6f, 0.3f, 0.5f};

    bool  water_enabled = false;
    float water_front[4]   = {0.0f, 0.314f, 0.233f, 0.744f};
    float water_surface[4] = {0.0f, 0.376f, 0.256f, 0.744f};

    bool  cave = false;
    bool  cave_top_equals_front = false;
    float shaft_spread[2] = {-400.0f, 700.0f};
};

// Lazily parse the embedded JSON once; returns all 8 biomes in id order
// (Grasslands, Forest, Grove, Wasteland, IceCastle, Cave, Fire, Florennum).
const std::vector<V3BiomeSpec>& v3_biome_db();

// Convenience accessor, clamps id to [0,7].
const V3BiomeSpec& v3_biome(int id);

// The raw embedded JSON text (single source of truth).
const char* v3_biomes_json();

} // namespace v3
} // namespace sgen
