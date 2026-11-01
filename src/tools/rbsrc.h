#pragma once
// ============================================================================
// rbsrc.h — the merged .rbsrc document model for ObjectStudio + the visual
// scripter, in Ruby GG.
//
// One format, three tiers (see docs/formats_and_schemas/objectstudio/
// RBSRC_MERGED_DESIGN.md):
//
//   structure  `archetype` + component declarations + `->` wiring
//   behaviour  `on <event>(...) ... end`  — real Lua 5.1, verbatim
//   timeline   `timeline` / `track` / `key` / `marker` / `loop` — what the
//              visual scripter records by dragging a ghost object
//
// The timeline tier is the only thing here that cannot be expressed as Lua by
// hand, and it is the only part that *lowers*: the compiler turns keyframe runs
// into the bounded `for … Program.Wait` loop the game already runs, and turns
// action markers into edge-gated calls. Every generated call is validated
// against the game's real Lua API, extracted from the shipped binary into
// `lua_api_table.h` (37 namespaces / 227 entries).
//
// This header is Qt-free on purpose: `ruby_gg` binds it to Qt widgets in
// `src/ruby/script/`, while `ruby_cli` and the tests use it headless.
// ============================================================================

#include "lua_api_table.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rbsrc {

// ── Diagnostics ──────────────────────────────────────────────────────────────

struct Diagnostic {
    int         line = 0;      // 1-based source line, 0 when not source-bound
    std::string message;
    bool        error = true;  // false == warning
};

// ── Values ───────────────────────────────────────────────────────────────────

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

enum class ValueKind { Scalar, Vector };

struct Value {
    ValueKind kind = ValueKind::Scalar;
    double    scalar = 0.0;
    Vec3      vector;

    static Value of(double s) { Value v; v.kind = ValueKind::Scalar; v.scalar = s; return v; }
    static Value of(const Vec3& p) { Value v; v.kind = ValueKind::Vector; v.vector = p; return v; }

    bool operator==(const Value& o) const;
};

enum class Easing { Linear, EaseIn, EaseOut, EaseInOut, Step };

const char* to_string(Easing e);
std::optional<Easing> easing_from(std::string_view text);

/// The interpolation curve the *preview and the compiler share*. One
/// implementation, so what a modder scrubs is what the emitted Lua computes.
double apply_easing(Easing e, double t);

// ── Timeline tier ────────────────────────────────────────────────────────────

struct Keyframe {
    double time = 0.0;
    Value  value;
    Easing easing = Easing::Linear;
};

struct Track {
    /// `position` | `rotation` | `scaling` | `depth` | `hidden` | `velocity`
    std::string property;
    std::vector<Keyframe> keys;

    bool is_vector() const;
};

/// A discrete function call at a timestamp — `item1`'s
/// `CreateShopItem(self, "healingpotion", 50)` is one of these at t = 0.
struct Marker {
    double                   time = 0.0;
    std::string              call;   // "Namespace.Function" or "recv:member"
    std::vector<std::string> args;   // raw Lua literals, validated separately
};

struct LoopRegion {
    bool   present = false;
    double begin = 0.0;
    double end = 0.0;
    int    repeats = 1;      // used when `forever` is false
    bool   forever = false;
};

struct Timeline {
    std::string name;
    std::string target = "self";     // `self`, or a component slug
    std::string hook = "OnLoad";     // which of the 21 Program slots this binds to

    /// Tick granularity of the generated loop.
    double step = 1.0 / 60.0;

    /// Emit `Program.Wait` only every n-th tick. This is not decoration: the
    /// shipped `blackhole` growth loop waits on a stride, and a timeline that
    /// ignored it would compile to Lua that runs orders of magnitude too fast.
    int wait_stride = 1;

    std::vector<Track>  tracks;
    std::vector<Marker> markers;
    LoopRegion          loop;

    double duration() const;
    bool   is_valid(std::string* why = nullptr) const;
};

// ── Behaviour + structure tiers ──────────────────────────────────────────────

struct BehaviourHook {
    std::string event;    // "load" for `on load(self)`
    std::string params;   // the declared parameter list, verbatim
    std::string body;     // verbatim Lua between the header and `end`
};

struct StructureDecl {
    std::string kind;     // "model", "glow", "light", "emitter", ...
    std::string line;     // the declaration, verbatim
};

struct Document {
    int         version = 1;
    std::string archetype;
    double      scaling = 1.0;

    std::vector<StructureDecl> structure;
    std::vector<Timeline>      timelines;
    std::vector<BehaviourHook> hooks;

    /// Provenance, so a compiled file can be re-opened against what it was
    /// recorded from (the property that makes .rbsrc re-editable rather than a
    /// one-way export).
    std::string source_object;
    std::string source_hook;

    const Timeline* find_timeline(std::string_view name) const;
};

// ── Parse / write (round-trippable) ─────────────────────────────────────────

struct ParseResult {
    Document                 document;
    std::vector<Diagnostic>  diagnostics;

    bool ok() const;
    std::string errors() const;
};

ParseResult parse(std::string_view text);

/// Canonical text form. `parse(write(doc))` must preserve every timeline.
std::string write(const Document& doc);

// ── Lowering: timeline tier → Lua 5.1 chunk ─────────────────────────────────

struct LowerResult {
    std::string             lua;
    std::vector<Diagnostic> diagnostics;

    bool ok() const;
    std::string errors() const;
};

/// Lower one timeline into a single Lua chunk ready for `Program.Bytes`.
LowerResult lower(const Document& doc, const Timeline& timeline);

// ── Game API validation ─────────────────────────────────────────────────────

const ApiFunction* find_api(std::string_view lua_namespace, std::string_view name);

/// Validate a `Namespace.Function` or `receiver:member` call against the
/// extracted table. Returns false and fills `error` when the engine does not
/// publish it.
bool validate_call(std::string_view call, std::string* error, const ApiFunction** found = nullptr);

// ── Hook slots ──────────────────────────────────────────────────────────────
//
// A handler is not a property of the object: it lives on a specific component.
// `OnLoad` is tag 10 of the SceneObject, `OnCollide` is tag 9 of the collision
// shape, and so on. A scripter that assumes one ProgramComponent misses most.

struct HookSlot {
    std::string_view event;       // "OnCollide"
    std::string_view lua_event;   // "collide"
    std::string_view rbsrc_paren; // the `...` preamble parameter name
    std::string_view owner_class; // which component owns the slot
    std::string_view field;       // the payload field carrying the Program
};

const std::vector<HookSlot>& hook_slots();
const HookSlot* find_hook_slot(std::string_view event);

} // namespace rbsrc
