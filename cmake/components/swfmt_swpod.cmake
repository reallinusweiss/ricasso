# ============================================================================
# Components: swfmt (PVR texture decode) and swpod (POD scene loading).
# ============================================================================

swordigo_library(swfmt
    ${SRC_DIR}/platform/pvr_loader.cpp
    # image_decode.cpp: the WebP path, which stb_image and QImage both lack.
    ${SRC_DIR}/tools/image_decode.cpp
    ${SRC_DIR}/platform/pvrtc_decoder.cpp
    ${SRC_DIR}/platform/astc_decoder.cpp)
target_link_libraries(swfmt PRIVATE swcore ZLIB::ZLIB OpenGL::GL)
if (SWORDIGO_HAVE_WEBP)
    target_compile_definitions(swfmt PRIVATE SWORDIGO_HAVE_WEBP=1)
    target_include_directories(swfmt PRIVATE ${SWORDIGO_WEBP_INCLUDE_DIRS})
    target_link_libraries(swfmt PRIVATE ${SWORDIGO_WEBP_LIBRARIES})
endif()

# Model loading and rendering only; the scene tooling of upstream
# SwordigoDesktop is not part of this Windows port.
swordigo_library(swpod
    ${SRC_DIR}/tools/pod_loader.cpp
    ${SRC_DIR}/tools/render_model.cpp
    ${SRC_DIR}/tools/av_renderer.cpp
    # The merged .rbsrc document model: structure + behaviour + timeline tiers,
    # lowerable to Lua. Used by ruby_gg's visual scripter and by ruby_cli.
    ${SRC_DIR}/tools/rbsrc.cpp
    # pod_stamp.cpp lives here because pod_loader.cpp is its only consumer: the
    # sidecar is read at pod_load() time to warn about stale bakes.
    ${SRC_DIR}/tools/pod_stamp.cpp)
target_link_libraries(swpod PRIVATE swcore SDL3::SDL3 OpenGL::GL OpenAL::OpenAL)
