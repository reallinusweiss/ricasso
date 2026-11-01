#pragma once
// astc_decoder.h — Standalone ASTC block decompressor (LDR)
//
// Based on Google AOSP drawElements Quality Program Tester Core and Rich Geldreich's
// standalone implementation (Apache-2.0).

#include <cstdint>

namespace pvr {

// Unpacks a single ASTC block (16 bytes) to dst_rgba (block_w * block_h * 4 bytes RGBA8888).
// is_srgb controls 8-bit to 16-bit endpoint scaling according to ASTC spec.
bool decompress_astc_block(uint8_t* dst_rgba, const uint8_t* block_16bytes, bool is_srgb, int block_w, int block_h);

} // namespace pvr
