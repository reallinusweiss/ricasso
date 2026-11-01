# ============================================================================
# Component: sre13 â€” ARM64 guest libsre13.so, cross-compiled with aarch64-linux-gnu.
# Produces bin/libs/libsre13.so directly for Swordigo 1.4.13.
#
# Layout:
#   src/sre/base   â€” shared infra (vendored lua, include/, sre_setjmp, ...)
#                    compiled INTO this library (no standalone base .so).
#   src/sre/sre13  â€” Swordigo 1.4.13 SRE code (sre13_*.c/h)
# ============================================================================

if (SWORDIGO_BUILD_SRE)
    include("${CMAKE_SOURCE_DIR}/cmake/SwordigoGuest.cmake")
    if (AARCH64_CC)
        set(SRE_BASE_DIR ${SRC_DIR}/sre/base)
        set(SRE_V13_DIR ${SRC_DIR}/sre/sre13)
        set(SRE13_CORE_SRCS
            sre13_init
            sre13_lua
            sre13_rbmath
            sre13_recovery
            sre13_safety
            sre13_scene
            sre13_scene_shifter
            sre13_console
            sre13_audio
            sre13_ui
            sre13_extras_stubs
            core/hook
            core/stdstring
            core/map
            core/assets
            core/saves
            core/toml
            core/Gloss_compat
            hooks/BindingValue
            hooks/Program
            hooks/Camera
            hooks/CameraController
            hooks/CaverShell
            hooks/Component
            hooks/Component/CharControllerComponent
            hooks/Component/EntityComponent
            hooks/GameSceneController
            hooks/GameViewController
            hooks/ModelLibrary
            hooks/PhysicsObjectState
            hooks/PlayerProfile
            hooks/ProgramState
            hooks/RenderingContext
            hooks/Scene
            hooks/SceneObject
            hooks/TextureLibrary
            ui/sre13_button_controller
        )
        set(SRE13_SRCS)
        foreach(name IN LISTS LUA_SRCS)
            list(APPEND SRE13_SRCS ${SRE_BASE_DIR}/lua/src/${name}.c)
        endforeach()
        foreach(name IN LISTS SRE13_CORE_SRCS)
            list(APPEND SRE13_SRCS ${SRE_V13_DIR}/${name}.c)
        endforeach()
        list(APPEND SRE13_SRCS ${SRE_BASE_DIR}/sre_setjmp.S)

        set(SRE13_OBJECTS)
        foreach(source IN LISTS SRE13_SRCS)
            file(RELATIVE_PATH relative "${SRC_DIR}" "${source}")
            string(REPLACE "/" "_" object_name "${relative}")
            string(REGEX REPLACE "\\.(c|S)$" ".o" object_name "${object_name}")
            set(object "${CMAKE_BINARY_DIR}/sre13/${object_name}")
            add_custom_command(OUTPUT "${object}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/sre13"
                COMMAND ${AARCH64_CC} ${SWORDIGO_GUEST_FLAGS} -fPIC -O2 -nostdlib -fno-builtin -fno-stack-protector -I${SRE_BASE_DIR}/include -I${SRE_BASE_DIR} -I${SRE_V13_DIR} -I${SRE_V13_DIR}/core -I${SRE_V13_DIR}/caver -I${SRE_V13_DIR}/hooks -I${SRE_V13_DIR}/ui -I${SRE_BASE_DIR}/lua/src -c "${source}" -o "${object}"
                DEPENDS "${source}" "${SRE_V13_DIR}/program_state_resume.h" VERBATIM)
            list(APPEND SRE13_OBJECTS "${object}")
        endforeach()
        # A response file avoids cmd.exe command-length limits in long source paths.
        set(_SRE13_OBJECTS_rsp "${CMAKE_BINARY_DIR}/sre13-objects.rsp")
        string(REPLACE ";" "\"\n\"" _SRE13_OBJECTS_lines "${SRE13_OBJECTS}")
        file(GENERATE OUTPUT "${_SRE13_OBJECTS_rsp}" CONTENT "\"${_SRE13_OBJECTS_lines}\"\n")
        add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/${SWORDIGO_DIST_DIR}/libs/libsre13.so"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/${SWORDIGO_DIST_DIR}/libs"
            COMMAND ${AARCH64_CC} ${SWORDIGO_GUEST_FLAGS} ${SWORDIGO_GUEST_LINK_FLAGS} -shared -fPIC -nostdlib -o "${CMAKE_SOURCE_DIR}/${SWORDIGO_DIST_DIR}/libs/libsre13.so" "@${_SRE13_OBJECTS_rsp}"
            DEPENDS ${SRE13_OBJECTS} "${_SRE13_OBJECTS_rsp}" VERBATIM)
        add_custom_target(sre13 ALL DEPENDS "${CMAKE_SOURCE_DIR}/${SWORDIGO_DIST_DIR}/libs/libsre13.so")
    endif()
endif()
