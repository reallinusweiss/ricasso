# ============================================================================
# Component: compatibility build target and install rules.
# Dynarmic is an integrated host CMake target; FFmpeg is runtime-loaded.
# ============================================================================

if (SWORDIGO_USE_DYNARMIC)
    add_custom_target(dynarmic-build DEPENDS dynarmic)
endif()

# FFmpeg is dynamically loaded at runtime via dlopen (see platform/ffmpeg_dyn.cpp).
# No in-tree configure/build step is required.


# ---------------------------------------------------------------------------
# Install layout — the build drops products straight into the source tree:
#   bin/swordfare, bin/ruby, bin/ruby_cli  (executables)
#   bin/libs/*.so                          (component libs + libsre.so guest lib)
# These install rules only matter when staging into a packaging prefix.
# ---------------------------------------------------------------------------
if (SWORDIGO_BUILD_SRE)
    add_dependencies(swordfare_boot sre sre13)
    install(FILES "${CMAKE_SOURCE_DIR}/${SWORDIGO_DIST_DIR}/libs/libsre12.so"
        "${CMAKE_SOURCE_DIR}/${SWORDIGO_DIST_DIR}/libs/libsre13.so" DESTINATION bin/libs COMPONENT sre)
endif()
install(TARGETS swordfare_boot DESTINATION bin COMPONENT runtime)
if (SWORDIGO_BUILD_RUBY)
    install(TARGETS ruby ruby_cli DESTINATION bin COMPONENT runtime)
endif()
install(TARGETS swcore swgui swfmt swpod filerift swgfx swemu swordfare LIBRARY DESTINATION bin/libs COMPONENT runtime)
if (WIN32)
    install(TARGETS swcore swgui swfmt swpod filerift swgfx swemu swordfare RUNTIME DESTINATION bin COMPONENT runtime)
endif()
if (SWORDIGO_BUILD_RUBY_GG)
    install(TARGETS ruby_gg DESTINATION bin COMPONENT runtime)
endif()
