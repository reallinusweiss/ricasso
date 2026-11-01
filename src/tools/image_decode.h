#pragma once
// image_decode.h — WebP, the one image format the rest of the stack cannot read.
//
// Everything else here decodes through stb_image (converter) or QImage
// (viewer), and both cover PNG/JPEG/TGA/BMP/GIF. Neither decodes WebP unless
// the local Qt happens to ship its webp image-format plugin. That matters
// because glTF's EXT_texture_webp is a *replacement* mechanism, not a fallback:
// a file may legitimately declare it in extensionsRequired and ship no PNG or
// JPEG at all. pilot.glb does exactly that — one image, mimeType image/webp,
// and the texture's core `source` field is absent — so the albedo simply never
// arrived, and the converter reported "0 textures" without complaint.
//
// This is deliberately a narrow, standalone module: libwebp is optional at
// configure time, and its absence degrades to the previous behaviour (those
// payloads are skipped) rather than breaking the build.
//
// libwebp is not a decoder option, it is free software under a BSD licence, so
// vendoring is unnecessary; SWORDIGO_HAVE_WEBP is set by cmake/components/
// swfmt_swpod.cmake from a pkg-config / find_library probe.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace av {

// True when this build linked libwebp and can decode VP8/VP8L payloads.
bool webp_decode_available();

// Cheap magic check (RIFF....WEBP) so callers can route a payload before
// spending a decode attempt on it.
bool bytes_are_webp(const uint8_t* data, size_t size);

// Decode a WebP payload to straight RGBA8, row-major, top-left origin. That is
// byte-for-byte the layout stb_image produces for PNG/JPEG with req_comp=4 and
// the layout of Qt's QImage::Format_RGBA8888, so a caller can substitute one
// decoder for the other without touching its pixel handling.
// Returns false when WebP support is absent or the payload will not decode;
// `rgba` is then left empty and width/height are 0.
bool webp_decode_rgba(const uint8_t* data, size_t size,
                      std::vector<uint8_t>& rgba, int& width, int& height);

} // namespace av
