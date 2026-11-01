#pragma once
#include <string>
#include <vector>

namespace filerift {

    // Decodes a binary protobuf (e.g., "scene", "scl", "gdata", etc.)
    
    std::string decode_protobuf(const std::string& bytes, const std::string& filetype);

    // Encodes FileRift's plain text markup back into binary protobuf.
    //(e.g., "scene", "scl", etc.)
   
    std::string recode_markup(const std::string& text, const std::string& filetype);

    // Check if a filetype or extension is supported by FileRift
    bool is_supported_filetype(const std::string& filetype);

    // Normalize a filetype or extension to its canonical schema name (e.g. "scn" -> "scene", "Map" -> "scmap")
    std::string normalize_filetype(const std::string& filetype);

    // Identify the FileRift filetype from file path and/or content header
    std::string detect_filetype(const std::string& path_or_ext, const std::string& header_or_bytes = "");

    // Returns the list of all canonical FileRift filetypes
    const std::vector<std::string>& supported_filetypes();

    // Our generic lua extractor
    std::string extract_lua_generic(const std::string& bytes);

    // Compiles a plaintext Lua source chunk to Lua 5.1 bytecode with the host
    // runtime embedded in this component (luaL_loadbuffer -> lua_dump). The
    // chunk is compiled, never executed. Returns an empty string on failure and,
    // when `error` is non-null, fills it with the compiler/dump message.
    std::string compile_lua_to_bytecode(const std::string& source,
                                        const std::string& name = "script",
                                        std::string* error = nullptr);

} // namespace IS filerift
