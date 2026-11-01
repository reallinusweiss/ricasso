# ============================================================================
# Component: filerift — host Lua (Lua 5.x core) for the client.
# In this Windows port the library carries only the host Lua; the scene and
# file-format tooling of upstream SwordigoDesktop is not part of the tree.
# ============================================================================

set(LUA_SRCS lapi lcode ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio lauxlib lbaselib ldblib liolib lmathlib loslib lstrlib ltablib loadlib linit)
set(HOST_LUA_SRCS)
foreach(name IN LISTS LUA_SRCS)
    list(APPEND HOST_LUA_SRCS ${SRC_DIR}/sre/base/lua/src/${name}.c)
endforeach()

swordigo_library(filerift ${HOST_LUA_SRCS})
if (MSVC)
    target_compile_definitions(filerift PRIVATE "LUAI_FUNC=extern __declspec(dllexport)")
else()
    target_compile_definitions(filerift PRIVATE "LUAI_FUNC=extern __attribute__((visibility(\"default\")))")
endif()
target_link_libraries(filerift PRIVATE ${SWORDIGO_LIBM})
