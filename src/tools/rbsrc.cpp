// ============================================================================
// rbsrc.cpp — see rbsrc.h and docs/formats_and_schemas/objectstudio/
// RBSRC_MERGED_DESIGN.md.
//
// The load-bearing part of this file is `lower()`: it turns a scrubbed timeline
// into the Lua loop the game actually runs. Two shipped patterns define what
// "correct" means and are what the tests pin down:
//
//   blackhole (rlsw.scl)  a bounded `for` loop that increments self:scaling()
//                         every tick but waits on a *stride* — so the ramp takes
//                         the authored time instead of a fraction of it.
//   item1 (town_shop.scene) a ProgramComponent with ExecuteOnce that makes one
//                         call at t = 0 and does no motion at all.
//
// Emitting the wrong shape here is invisible until the script runs at the wrong
// speed in-game, which is why the lowering rules are asserted, not eyeballed.
// ============================================================================

#include "rbsrc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace rbsrc {
namespace {

// ── small text helpers ───────────────────────────────────────────────────────

std::string trim(std::string_view s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

std::vector<std::string> split_ws(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

/// Split on top-level commas (ignoring commas inside the argument parens).
std::vector<std::string> split_args(std::string_view s) {
    std::vector<std::string> out;
    int depth = 0;
    std::string cur;
    for (char c : s) {
        if (c == '(') ++depth;
        if (c == ')') --depth;
        if (c == ',' && depth == 0) { out.push_back(trim(cur)); cur.clear(); }
        else cur.push_back(c);
    }
    const std::string last = trim(cur);
    if (!last.empty()) out.push_back(last);
    return out;
}

/// Compact numeric formatting for generated Lua.
std::string num(double v) {
    if (std::fabs(v) < 1e-12) return "0";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    return buf;
}

std::string indent(int levels) {
    return std::string(static_cast<size_t>(levels) * 4, ' ');
}

// ── hook preamble parameters ─────────────────────────────────────────────────

std::string preamble_params_for(std::string_view event) {
    if (event == "OnCollide" || event == "OnCollisionEnd") return "self, target, normal, groundCollision";
    if (event == "OnHurt")      return "self, attacker, amount";
    if (event == "OnKill")      return "self, killer";
    if (event == "OnReceiveDamage") return "self, amount, source";
    if (event == "OnCollect")   return "self, collector";
    if (event == "OnTouch")     return "self, toucher";
    // OnLoad / OnActivate / OnCast / OnAttack / OnBreak / OnPress / OnRelease /
    // OnItemGet / OnItemGet / Program. Exact arity for the remaining slots is
    // open question #1 in the merged design doc; `self` is correct for all of
    // the ones observed in the corpus.
    return "self";
}

/// Property name -> the SceneObject setter that exists in the extracted API.
const char* setter_for(std::string_view property) {
    if (property == "position") return "SceneObject.setPosition";
    if (property == "rotation") return "SceneObject.setRotation";
    if (property == "scaling")  return "SceneObject.setScaling";
    if (property == "depth")    return "SceneObject.setDepth";
    if (property == "hidden")   return "SceneObject.setHidden";
    if (property == "velocity") return "SceneObject.setVelocity";
    return nullptr;
}

} // namespace

// ── Value / easing ───────────────────────────────────────────────────────────

bool Value::operator==(const Value& o) const {
    if (kind != o.kind) return false;
    if (kind == ValueKind::Scalar) return std::fabs(scalar - o.scalar) < 1e-12;
    return std::fabs(vector.x - o.vector.x) < 1e-12 &&
           std::fabs(vector.y - o.vector.y) < 1e-12 &&
           std::fabs(vector.z - o.vector.z) < 1e-12;
}

const char* to_string(Easing e) {
    switch (e) {
        case Easing::Linear:    return "linear";
        case Easing::EaseIn:    return "ease-in";
        case Easing::EaseOut:   return "ease-out";
        case Easing::EaseInOut: return "ease-in-out";
        case Easing::Step:      return "step";
    }
    return "linear";
}

std::optional<Easing> easing_from(std::string_view text) {
    if (text == "linear")      return Easing::Linear;
    if (text == "ease-in")     return Easing::EaseIn;
    if (text == "ease-out")    return Easing::EaseOut;
    if (text == "ease-in-out") return Easing::EaseInOut;
    if (text == "step")        return Easing::Step;
    return std::nullopt;
}

double apply_easing(Easing e, double t) {
    t = std::max(0.0, std::min(1.0, t));
    switch (e) {
        case Easing::Linear:    return t;
        case Easing::EaseIn:    return t * t;
        case Easing::EaseOut:   return 1.0 - (1.0 - t) * (1.0 - t);
        case Easing::EaseInOut: return t < 0.5 ? 2.0 * t * t
                                               : 1.0 - 2.0 * (1.0 - t) * (1.0 - t);
        case Easing::Step:      return 0.0;
    }
    return t;
}

// ── Timeline helpers ─────────────────────────────────────────────────────────

bool Track::is_vector() const {
    return !keys.empty() && keys.front().value.kind == ValueKind::Vector;
}

double Timeline::duration() const {
    double d = 0.0;
    for (const auto& t : tracks) {
        if (!t.keys.empty()) d = std::max(d, t.keys.back().time);
    }
    for (const auto& m : markers) d = std::max(d, m.time);
    if (loop.present) d = std::max(d, loop.end);
    return d;
}

bool Timeline::is_valid(std::string* why) const {
    auto fail = [&](const char* msg) {
        if (why) *why = msg;
        return false;
    };
    if (step <= 0.0) return fail("step must be > 0");
    if (wait_stride < 1) return fail("wait-stride must be >= 1");
    if (hook.empty()) return fail("timeline has no hook");
    // A marker-only timeline at t = 0 is the `item1` shape: one call, no motion,
    // and a perfectly valid recording. Only a timeline with nothing in it at all
    // is rejected.
    if (duration() <= 0.0 && markers.empty()) {
        return fail("timeline has no keyframes or markers");
    }
    for (const auto& t : tracks) {
        if (!setter_for(t.property)) {
            if (why) *why = "unknown track property '" + t.property + "'";
            return false;
        }
        if (t.keys.size() < 2) {
            if (why) *why = "track '" + t.property + "' needs at least two keyframes";
            return false;
        }
        for (size_t i = 1; i < t.keys.size(); ++i) {
            if (t.keys[i].time <= t.keys[i - 1].time) {
                if (why) *why = "track '" + t.property + "' keyframes are not in ascending time";
                return false;
            }
        }
        const bool vec = t.is_vector();
        for (const auto& k : t.keys) {
            if ((k.value.kind == ValueKind::Vector) != vec) {
                if (why) *why = "track '" + t.property + "' mixes scalar and vector keys";
                return false;
            }
        }
    }
    for (const auto& m : markers) {
        if (m.call.empty()) return fail("marker has no call");
    }
    return true;
}

const Timeline* Document::find_timeline(std::string_view timeline_name) const {
    for (const auto& t : timelines) {
        if (t.name == timeline_name) return &t;
    }
    return nullptr;
}

bool ParseResult::ok() const {
    for (const auto& d : diagnostics) if (d.error) return false;
    return true;
}

std::string ParseResult::errors() const {
    std::ostringstream os;
    for (const auto& d : diagnostics) {
        if (!d.error) continue;
        os << "line " << d.line << ": " << d.message << "\n";
    }
    return os.str();
}

bool LowerResult::ok() const {
    for (const auto& d : diagnostics) if (d.error) return false;
    return true;
}

std::string LowerResult::errors() const {
    std::ostringstream os;
    for (const auto& d : diagnostics) {
        if (!d.error) continue;
        os << d.message << "\n";
    }
    return os.str();
}

// ── Game API lookup ──────────────────────────────────────────────────────────

const ApiFunction* find_api(std::string_view lua_namespace, std::string_view name) {
    for (size_t i = 0; i < kGameLuaApiCount; ++i) {
        const ApiFunction& f = kGameLuaApi[i];
        if (f.lua_namespace == lua_namespace && f.name == name) return &f;
    }
    return nullptr;
}

bool validate_call(std::string_view call, std::string* error, const ApiFunction** found) {
    // Strip a trailing "()" if the caller left it on.
    std::string c(call);
    while (!c.empty() && (c.back() == ')' )) {
        const auto open = c.find('(');
        if (open == std::string::npos) break;
        c.resize(open);
    }

    // Method-call syntax, `v:x` or `spray:setRotation`. The engine resolves this
    // two different ways, and both are real:
    //   * a class metatable member (`Vector3:x`, `Rectangle:top`), or
    //   * a namespace function reached through an installed `__index` —
    //     `SceneObject.__index` is itself in the API table, which is why
    //     `spray:setRotation(v)` works even though `setRotation` is published as
    //     the plain function `SceneObject.setRotation`.
    const auto colon = c.find(':');
    if (colon != std::string::npos) {
        const std::string member = c.substr(colon + 1);
        const std::string member_key = ":" + member;

        for (size_t i = 0; i < kGameLuaApiCount; ++i) {
            if (kGameLuaApi[i].kind == ApiKind::InstanceMember &&
                kGameLuaApi[i].name == member_key) {
                if (found) *found = &kGameLuaApi[i];
                return true;
            }
        }

        for (size_t i = 0; i < kGameLuaApiCount; ++i) {
            const ApiFunction& f = kGameLuaApi[i];
            if (f.kind != ApiKind::NamespaceFn || f.name != member) continue;
            if (find_api(f.lua_namespace, "__index")) {
                if (found) *found = &f;
                return true;
            }
        }

        if (error) *error = "the game publishes no method '" + member + "'";
        return false;
    }

    const auto dot = c.rfind('.');
    if (dot == std::string::npos) {
        if (error) *error = "'" + c + "' is not a Namespace.Function call";
        return false;
    }

    const std::string ns   = c.substr(0, dot);
    const std::string name = c.substr(dot + 1);
    const ApiFunction* f = find_api(ns, name);
    if (!f) {
        if (error) {
            *error = "the game publishes no Lua function '" + ns + "." + name + "'";
        }
        return false;
    }
    if (found) *found = f;
    return true;
}

// ── Hook slots ───────────────────────────────────────────────────────────────

const std::vector<HookSlot>& hook_slots() {
    // A handler belongs to a component, not to the object: `OnLoad` is tag 10 of
    // the SceneObject while `OnCollide` is tag 9 of the collision shape. Writing
    // a compiled chunk into the wrong owner silently disables the script.
    static const std::vector<HookSlot> kSlots = {
        {"OnLoad",          "load",           "self",                            "SceneObject",                       "10 (SceneObject)"},
        {"OnCollide",       "collide",        "self, target, normal, ground",    "CollisionShapeComponent",           "9"},
        {"OnCollisionEnd",  "collision_end",  "self, target, normal, ground",    "CollisionShapeComponent",           "10"},
        {"OnReceiveDamage", "receive_damage", "self, amount, source",             "CollisionShapeComponent",           "11"},
        {"OnKill",          "kill",           "self, killer",                     "MonsterEntityComponent",            "2"},
        {"OnHurt",          "hurt",           "self, attacker, amount",           "MonsterEntityComponent",            "3"},
        {"OnActivate",      "activate",       "self",                            "EntityActionComponent",             "1"},
        {"OnCollect",       "collect",        "self, collector",                  "CollectableItemComponent",          "6"},
        {"OnCast",          "cast",           "self",                            "SpellComponent",                    "2"},
        {"OnAttack",        "attack",         "self",                            "AttackComponent",                   "1"},
        {"OnBreak",         "break",          "self",                            "BreakableObjectComponent",          "1"},
        {"OnPress",         "press",          "self",                            "PressureTriggerComponent",          "1"},
        {"OnRelease",       "release",        "self",                            "PressureTriggerComponent",          "2"},
        {"OnTouch",         "touch",          "self, toucher",                   "TouchableComponent",                "2"},
        {"OnItemGet",       "item_get",       "self, item",                      "HeroEntityComponent",               "4"},
        {"Program",         "program",        "self",                            "ProgramComponent",                  "1"},
    };
    return kSlots;
}

const HookSlot* find_hook_slot(std::string_view event) {
    for (const auto& s : hook_slots()) {
        if (s.event == event) return &s;
    }
    return nullptr;
}

// ── parse ────────────────────────────────────────────────────────────────────

ParseResult parse(std::string_view text) {
    ParseResult r;
    Document& doc = r.document;

    enum class Section { None, Structure, Timeline, Behaviour };
    Section section = Section::None;
    Timeline* cur = nullptr;
    Track* cur_track = nullptr;
    BehaviourHook* cur_hook = nullptr;

    int line_no = 0;
    size_t pos = 0;
    while (pos <= text.size()) {
        const size_t nl = text.find('\n', pos);
        const std::string_view raw =
            text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;
        ++line_no;

        const std::string line = trim(raw);
        if (line.empty()) continue;
        if (line.rfind("--", 0) == 0) {
            // `-- rbsrc: source-object fire_spirit` records provenance.
            const auto marker = line.find("source-object ");
            if (marker != std::string::npos) {
                doc.source_object = trim(line.substr(marker + 14));
            }
            continue;
        }

        auto toks = split_ws(line);
        const std::string& head = toks.front();

        if (head == "archetype") {
            if (toks.size() < 2) {
                r.diagnostics.push_back({line_no, "'archetype' needs a name"});
            } else {
                doc.archetype = toks[1];
            }
            section = Section::Structure;
            cur = nullptr; cur_track = nullptr; cur_hook = nullptr;
            continue;
        }

        if (head == "scaling") {
            if (toks.size() < 2) {
                r.diagnostics.push_back({line_no, "'scaling' needs a value"});
            } else {
                doc.scaling = std::strtod(toks[1].c_str(), nullptr);
            }
            continue;
        }

        if (head == "timeline") {
            if (toks.size() < 2) {
                r.diagnostics.push_back({line_no, "'timeline' needs a name"});
                continue;
            }
            doc.timelines.push_back(Timeline{});
            cur = &doc.timelines.back();
            cur->name = toks[1];
            cur_track = nullptr;
            cur_hook = nullptr;
            section = Section::Timeline;
            continue;
        }

        if (head == "on") {
            // `on load(self)` / `on collide(self, target, normal, ground)`
            std::string rest = trim(line.substr(2));
            if (rest.empty()) {
                r.diagnostics.push_back({line_no, "'on' needs an event name"});
                continue;
            }
            std::string event = rest;
            std::string params;
            const auto paren = rest.find('(');
            if (paren != std::string::npos) {
                event = trim(rest.substr(0, paren));
                const auto close = rest.rfind(')');
                params = (close != std::string::npos && close > paren)
                             ? rest.substr(paren + 1, close - paren - 1)
                             : rest.substr(paren + 1);
            }
            doc.hooks.push_back(BehaviourHook{event, params, {}});
            cur_hook = &doc.hooks.back();
            cur = nullptr; cur_track = nullptr;
            section = Section::Behaviour;
            continue;
        }

        if (section == Section::Behaviour && head == "end") {
            section = Section::Structure;
            cur_hook = nullptr;
            continue;
        }

        if (section == Section::Behaviour && cur_hook) {
            if (!cur_hook->body.empty()) cur_hook->body += "\n";
            cur_hook->body += line;
            continue;
        }

        if (section == Section::Timeline && cur) {
            if (head == "end") { section = Section::Structure; cur = nullptr; continue; }

            if (head == "hook") {
                cur->hook = toks.size() > 1 ? toks[1] : "";
                continue;
            }
            if (head == "target") { cur->target = toks.size() > 1 ? toks[1] : "self"; continue; }
            if (head == "step") {
                cur->step = toks.size() > 1 ? std::strtod(toks[1].c_str(), nullptr) : 0.0;
                continue;
            }
            if (head == "wait-stride") {
                cur->wait_stride = toks.size() > 1 ? std::atoi(toks[1].c_str()) : 1;
                continue;
            }
            if (head == "track") {
                if (toks.size() < 2) {
                    r.diagnostics.push_back({line_no, "'track' needs a property"});
                    continue;
                }
                cur->tracks.push_back(Track{});
                cur_track = &cur->tracks.back();
                cur_track->property = toks[1];
                continue;
            }
            if (head == "key") {
                if (!cur_track) {
                    r.diagnostics.push_back({line_no, "'key' outside a track"});
                    continue;
                }
                if (toks.size() < 3) {
                    r.diagnostics.push_back({line_no, "'key' needs a time and a value"});
                    continue;
                }
                Keyframe k;
                k.time = std::strtod(toks[1].c_str(), nullptr);

                size_t idx = 2;
                if (toks[idx].rfind("(", 0) == 0) {
                    std::string joined;
                    for (; idx < toks.size(); ++idx) {
                        if (!joined.empty()) joined += " ";
                        joined += toks[idx];
                        if (toks[idx].find(')') != std::string::npos) { ++idx; break; }
                    }
                    std::string inner = joined;
                    if (!inner.empty() && inner.front() == '(') inner.erase(0, 1);
                    if (!inner.empty() && inner.back() == ')') inner.pop_back();
                    const auto parts = split_args(inner);
                    if (parts.size() != 3) {
                        r.diagnostics.push_back({line_no, "vector key needs exactly three components"});
                        continue;
                    }
                    Vec3 v;
                    v.x = std::strtod(parts[0].c_str(), nullptr);
                    v.y = std::strtod(parts[1].c_str(), nullptr);
                    v.z = std::strtod(parts[2].c_str(), nullptr);
                    k.value = Value::of(v);
                } else {
                    k.value = Value::of(std::strtod(toks[idx].c_str(), nullptr));
                    ++idx;
                }

                if (idx < toks.size()) {
                    if (auto e = easing_from(toks[idx])) {
                        k.easing = *e;
                    } else {
                        r.diagnostics.push_back(
                            {line_no, "unknown easing '" + toks[idx] + "'"});
                    }
                }
                cur_track->keys.push_back(k);
                continue;
            }
            if (head == "marker") {
                if (toks.size() < 3) {
                    r.diagnostics.push_back({line_no, "'marker' needs a time and a call"});
                    continue;
                }
                Marker m;
                m.time = std::strtod(toks[1].c_str(), nullptr);
                std::string rest = trim(line.substr(line.find(toks[1]) + toks[1].size()));
                const auto paren = rest.find('(');
                if (paren == std::string::npos) {
                    m.call = trim(rest);
                } else {
                    m.call = trim(rest.substr(0, paren));
                    const auto close = rest.rfind(')');
                    const std::string args = (close != std::string::npos && close > paren)
                                                 ? rest.substr(paren + 1, close - paren - 1)
                                                 : rest.substr(paren + 1);
                    m.args = split_args(args);
                }
                cur->markers.push_back(std::move(m));
                continue;
            }
            if (head == "loop") {
                // `loop 0.0 .. 1.2 repeat forever`
                LoopRegion lp;
                const auto dots = line.find("..");
                if (dots == std::string::npos) {
                    r.diagnostics.push_back({line_no, "'loop' needs 'begin .. end'"});
                    continue;
                }
                lp.begin = std::strtod(trim(line.substr(5, dots - 5)).c_str(), nullptr);
                size_t after = dots + 2;
                while (after < line.size() && line[after] == '.') ++after;
                const std::string tail = trim(line.substr(after));
                const auto tail_toks = split_ws(tail);
                lp.end = tail_toks.empty() ? 0.0 : std::strtod(tail_toks[0].c_str(), nullptr);
                lp.present = true;
                if (tail.find("forever") != std::string::npos) {
                    lp.forever = true;
                } else if (tail_toks.size() >= 2 && tail_toks[0] == "repeat") {
                    lp.repeats = std::atoi(tail_toks[1].c_str());
                }
                cur->loop = lp;
                continue;
            }

            r.diagnostics.push_back({line_no, "unknown timeline directive '" + head + "'"});
            continue;
        }

        // Structure tier: keep the declaration verbatim, record its kind.
        doc.structure.push_back(StructureDecl{head, line});
    }

    for (auto& tl : doc.timelines) {
        std::string why;
        if (!tl.is_valid(&why)) {
            r.diagnostics.push_back({0, "timeline '" + tl.name + "': " + why});
        }
    }

    return r;
}

// ── write ────────────────────────────────────────────────────────────────────

std::string write(const Document& doc) {
    std::ostringstream os;
    os << "-- rbsrc version " << doc.version << "\n";
    if (!doc.source_object.empty()) {
        os << "-- rbsrc: source-object " << doc.source_object;
        if (!doc.source_hook.empty()) os << " source-hook " << doc.source_hook;
        os << "\n";
    }
    os << "\n";
    if (!doc.archetype.empty()) os << "archetype " << doc.archetype << "\n";
    if (std::fabs(doc.scaling - 1.0) > 1e-12) os << "  scaling " << num(doc.scaling) << "\n";

    for (const auto& s : doc.structure) {
        os << "  " << s.line << "\n";
    }

    for (const auto& tl : doc.timelines) {
        os << "\n  timeline " << tl.name << "\n";
        os << "    hook   " << tl.hook << "\n";
        if (tl.target != "self") os << "    target " << tl.target << "\n";
        os << "    step   " << num(tl.step) << "\n";
        if (tl.wait_stride != 1) os << "    wait-stride " << tl.wait_stride << "\n";

        for (const auto& t : tl.tracks) {
            os << "    track  " << t.property << "\n";
            for (const auto& k : t.keys) {
                os << "      key  " << num(k.time) << "  ";
                if (k.value.kind == ValueKind::Vector) {
                    os << "(" << num(k.value.vector.x) << ", " << num(k.value.vector.y)
                       << ", " << num(k.value.vector.z) << ")";
                } else {
                    os << num(k.value.scalar);
                }
                if (k.easing != Easing::Linear) os << "  " << to_string(k.easing);
                os << "\n";
            }
        }

        for (const auto& m : tl.markers) {
            os << "    marker " << num(m.time) << "  " << m.call << "(";
            for (size_t i = 0; i < m.args.size(); ++i) {
                if (i) os << ", ";
                os << m.args[i];
            }
            os << ")\n";
        }

        if (tl.loop.present) {
            os << "    loop   " << num(tl.loop.begin) << " .. " << num(tl.loop.end) << " repeat ";
            os << (tl.loop.forever ? "forever" : num(tl.loop.repeats)) << "\n";
        }
    }

    for (const auto& h : doc.hooks) {
        os << "\n  on " << h.event << "(" << h.params << ")\n";
        std::istringstream body(h.body);
        std::string line;
        while (std::getline(body, line)) os << "    " << line << "\n";
        os << "  end\n";
    }

    return os.str();
}

// ── lower ────────────────────────────────────────────────────────────────────

LowerResult lower(const Document& doc, const Timeline& timeline) {
    LowerResult r;

    std::string why;
    if (!timeline.is_valid(&why)) {
        r.diagnostics.push_back({0, "cannot lower timeline '" + timeline.name + "': " + why, true});
        return r;
    }

    const double step  = timeline.step;
    const int    stride = std::max(1, timeline.wait_stride);
    const double total = timeline.duration();
    const int    frames = total > 0.0
                              ? static_cast<int>(std::floor(total / step + 0.5)) + 1
                              : 1;   // marker-only timeline: a single pass

    const std::string target = timeline.target.empty() ? "self" : timeline.target;

    // Validate every call we are about to emit against the game's real API.
    for (const auto& t : timeline.tracks) {
        const char* setter = setter_for(t.property);
        std::string err;
        if (!validate_call(setter, &err)) {
            r.diagnostics.push_back({0, std::string("track '") + t.property + "': " + err, true});
        }
    }
    for (const auto& m : timeline.markers) {
        std::string err;
        const ApiFunction* f = nullptr;
        if (!validate_call(m.call, &err, &f)) {
            r.diagnostics.push_back({0, "marker at t=" + num(m.time) + ": " + err, true});
        } else if (f && !f->mutating) {
            r.diagnostics.push_back(
                {0, "marker at t=" + num(m.time) + ": '" + m.call +
                        "' is not a state-changing call — it may be a getter",
                 false});
        }
    }
    if (!r.ok()) return r;

    std::ostringstream os;
    os << "-- rbsrc v" << doc.version;
    if (!doc.archetype.empty()) os << " · " << doc.archetype;
    os << " · timeline \"" << timeline.name << "\" · hook " << timeline.hook << "\n";
    os << "-- generated: " << frames << " ticks @ " << num(step) << "s (" << num(total)
       << "s)";
    if (timeline.wait_stride != 1) os << ", Program.Wait every " << stride << " ticks";
    if (timeline.loop.present) {
        os << ", loop " << num(timeline.loop.begin) << ".." << num(timeline.loop.end)
           << (timeline.loop.forever ? " forever" : " repeated");
    }
    os << "\n";

    // The chunk's arguments arrive through `...`; which ones depends on the hook.
    os << "local " << preamble_params_for(timeline.hook) << " = ...\n";

    const bool any_easing = [&] {
        for (const auto& t : timeline.tracks) {
            for (size_t i = 1; i < t.keys.size(); ++i) {
                if (t.keys[i].easing != Easing::Linear) return true;
            }
        }
        return false;
    }();

    os << "local function __seg(t, t0, t1) return math.max(0.0, math.min(1.0, (t - t0) / (t1 - t0))) end\n";
    if (any_easing) {
        os << "local function __ease_in(t) return t * t end\n";
        os << "local function __ease_out(t) return 1.0 - (1.0 - t) * (1.0 - t) end\n";
        os << "local function __ease_in_out(t)\n"
              "    if t < 0.5 then return 2.0 * t * t end\n"
              "    return 1.0 - 2.0 * (1.0 - t) * (1.0 - t)\n"
              "end\n";
    }

    auto easing_expr = [](Easing e, const std::string& seg) -> std::string {
        switch (e) {
            case Easing::Linear:    return seg;
            case Easing::EaseIn:    return "__ease_in(" + seg + ")";
            case Easing::EaseOut:   return "__ease_out(" + seg + ")";
            case Easing::EaseInOut: return "__ease_in_out(" + seg + ")";
            case Easing::Step:      return "0.0";
        }
        return seg;
    };

    const int body_indent = timeline.loop.present ? 2 : 1;

    if (timeline.loop.present) {
        if (timeline.loop.forever) {
            os << "while true do\n";
        } else {
            os << "for __loop = 1, " << timeline.loop.repeats << " do\n";
        }
    }

    os << indent(body_indent) << "for __frame = 0, " << (frames - 1) << " do\n";
    os << indent(body_indent + 1) << "local __t = __frame * " << num(step) << "\n";

    // ── tracks ──────────────────────────────────────────────────────────────
    for (const auto& t : timeline.tracks) {
        const char* setter = setter_for(t.property);
        const bool vec = t.is_vector();

        os << "\n" << indent(body_indent + 1) << "-- " << t.property << "\n";

        for (size_t i = 0; i + 1 < t.keys.size(); ++i) {
            const Keyframe& a = t.keys[i];
            const Keyframe& b = t.keys[i + 1];

            os << indent(body_indent + 1) << "if __t >= " << num(a.time)
               << " and __t < " << num(b.time) << " then\n";
            os << indent(body_indent + 2) << "local __k = "
               << easing_expr(b.easing, "__seg(__t, " + num(a.time) + ", " + num(b.time) + ")")
               << "\n";

            if (vec) {
                os << indent(body_indent + 2) << setter << "(" << target
                   << ", Vector3.New(\n";
                auto comp = [&](char axis, double av, double bv) {
                    os << indent(body_indent + 3) << num(av) << " + (" << num(bv) << " - "
                       << num(av) << ") * __k";
                    os << (axis == 'z' ? "))\n" : ",\n");
                };
                comp('x', a.value.vector.x, b.value.vector.x);
                comp('y', a.value.vector.y, b.value.vector.y);
                comp('z', a.value.vector.z, b.value.vector.z);
            } else {
                os << indent(body_indent + 2) << setter << "(" << target << ", "
                   << num(a.value.scalar) << " + (" << num(b.value.scalar) << " - "
                   << num(a.value.scalar) << ") * __k)\n";
            }
            os << indent(body_indent + 1) << "end\n";
        }

        // Hold the final value for the remainder of the timeline, so a track
        // that ends early does not snap back to its first key.
        const Keyframe& last = t.keys.back();
        os << indent(body_indent + 1) << "if __t >= " << num(last.time) << " then\n";
        if (vec) {
            os << indent(body_indent + 2) << setter << "(" << target << ", Vector3.New("
               << num(last.value.vector.x) << ", " << num(last.value.vector.y) << ", "
               << num(last.value.vector.z) << "))\n";
        } else {
            os << indent(body_indent + 2) << setter << "(" << target << ", "
               << num(last.value.scalar) << ")\n";
        }
        os << indent(body_indent + 1) << "end\n";
    }

    // ── markers ─────────────────────────────────────────────────────────────
    // Edge-gated on the tick of every frame, so a marker fires exactly once no
    // matter how the loop body passes over it. This is the discrete counterpart
    // of the interpolation above, and it is what makes `item1`'s single call at
    // t=0 compile to exactly one call.
    if (!timeline.markers.empty()) {
        os << "\n" << indent(body_indent + 1) << "-- markers\n";
        for (const auto& m : timeline.markers) {
            // Arguments are emitted verbatim and the receiver is NOT injected:
            // the shipped corpus writes `CreateShopItem(self, "healingpotion", 50)`
            // and `PhysicsObject.IsEnabled(self)` with `self` spelled out by the
            // author, so injecting one would emit `self, self`. Arity is checked
            // separately once the `luaL_check*` pass (design doc M2) lands.
            os << indent(body_indent + 1) << "if __t >= " << num(m.time)
               << " and __t < " << num(m.time + step) << " then " << m.call << "(";
            for (size_t i = 0; i < m.args.size(); ++i) {
                if (i) os << ", ";
                os << m.args[i];
            }
            os << ") end\n";
        }
    }

    // ── wait ────────────────────────────────────────────────────────────────
    os << "\n" << indent(body_indent + 1);
    if (stride == 1) {
        os << "Program.Wait(" << num(step) << ")\n";
    } else {
        os << "if (__frame % " << stride << ") == 0 then Program.Wait("
           << num(step * stride) << ") end\n";
    }

    os << indent(body_indent) << "end\n";
    if (timeline.loop.present) os << "end\n";

    r.lua = os.str();
    return r;
}

} // namespace rbsrc
