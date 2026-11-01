// scene_program.cpp — Lua-aware writer for Caver::Program messages.
//
// A Program serializes as { 1: String source, 2: Bytes bytecode, 3: Name }.
// The shipped engine's Program::LoadIntoState() ignores field 1 entirely: it
// checks field 2 and feeds it straight to luaL_loadbuffer(). Writing only the
// source therefore has no runtime effect — the game keeps executing whatever
// bytecode was already embedded (or loads nothing when there is none), which
// silently made every Lua edit in the editors dead on arrival.
//
// This translation unit lives in the filerift component because regenerating
// field 2 needs the host Lua 5.1 runtime (luaL_loadbuffer + lua_dump), which
// filerift embeds and swpod (the scene-format library that owns
// scene_loader.h) must not depend on. The declaration stays in scene_loader.h
// so callers are unchanged.
#include "tools/scene_loader.h"
#include "tools/filerift.h"
#include "platform/protobuf_reader.h"

#include <vector>

namespace av {

bool scene_set_program_source(std::string& program_data, const std::string& source,
                              std::string* error) {
    if (error) error->clear();

    std::vector<proto::Field> fields;
    if (!program_data.empty()) {
        try {
            proto::Reader reader(program_data);
            fields = reader.read_all();
        } catch (...) {
            if (error) *error = "Program message is not valid protobuf";
            return false;
        }
    }

    // Field 1 (String source): replace in place, else append.
    bool source_found = false;
    for (auto& field : fields) {
        if (field.field_number == 1 && field.wire_type == proto::WIRE_LEN) {
            field.bytes_val = source;
            source_found = true;
            break;
        }
    }
    if (!source_found && !source.empty()) {
        proto::Field field{};
        field.field_number = 1;
        field.wire_type = proto::WIRE_LEN;
        field.bytes_val = source;
        fields.push_back(std::move(field));
    }

    // Field 2 (Bytes bytecode): regenerate from the new source, dropping any
    // pre-existing (now stale) chunk when compilation fails.
    std::string bytecode;
    bool compiled = true;
    if (!source.empty()) {
        bytecode = filerift::compile_lua_to_bytecode(source, "program", error);
        compiled = !bytecode.empty();
    }

    bool bytecode_found = false;
    for (auto it = fields.begin(); it != fields.end();) {
        if (it->field_number == 2 && it->wire_type == proto::WIRE_LEN) {
            if (compiled && !bytecode.empty() && !bytecode_found) {
                it->bytes_val = bytecode;
                bytecode_found = true;
                ++it;
            } else {
                it = fields.erase(it);   // stale or duplicate chunk
            }
        } else {
            ++it;
        }
    }
    if (compiled && !bytecode.empty() && !bytecode_found) {
        proto::Field field{};
        field.field_number = 2;
        field.wire_type = proto::WIRE_LEN;
        field.bytes_val = bytecode;
        fields.push_back(std::move(field));
    }

    proto::Writer writer;
    for (const auto& field : fields) writer.write_field(field);
    program_data = writer.to_string();

    // The source is always stored so an edit is never lost; a false return only
    // means the program is not runnable until the Lua error is fixed.
    return compiled;
}

} // namespace av
