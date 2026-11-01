/* pvr_loader.h — PVR texture file loader for Swordigo Desktop
 *
 * Loads PVR v2/v3 files containing ETC1 compressed textures.
 * Decodes ETC1 → RGBA and uploads to an OpenGL texture.
 *
 * Usage:
 *   GLuint tex = pvr_load_texture("path/to/file.pvr");
 *   if (tex) glBindTexture(GL_TEXTURE_2D, tex);
 */

#ifndef PVR_LOADER_H
#define PVR_LOADER_H

#include <stdint.h>
#include <vector>

#if !defined(__gl_h_) && !defined(__gltypes_h_) && !defined(__gl2_h_) && !defined(__gl3_h_) && !defined(GL_VERSION_1_1)
typedef unsigned int GLuint;
#endif

/* Decode a PVR or gzipped .tex file buffer directly to RGBA8888.
 * Handles automatic gzip decompression.
 * Returns true on success. */
bool pvr_decode_to_rgba(const uint8_t* file_data, size_t file_size, std::vector<uint8_t>& rgba_out, int& width, int& height);
bool pvr_upload_compressed_to_tex(const uint8_t* file_data, size_t file_size, GLuint tex_name, int& out_w, int& out_h);

extern bool g_pvr_software_decode;

GLuint pvr_load_texture(const char* path, int* out_width = nullptr, int* out_height = nullptr);

#endif /* PVR_LOADER_H */
