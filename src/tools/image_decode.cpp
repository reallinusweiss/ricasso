// image_decode.cpp — see image_decode.h.

#include "image_decode.h"

#include <cstring>

#if SWORDIGO_HAVE_WEBP
#  include <webp/decode.h>
#endif

namespace av {

bool webp_decode_available() {
#if SWORDIGO_HAVE_WEBP
    return true;
#else
    return false;
#endif
}

bool bytes_are_webp(const uint8_t* data, size_t size) {
    // RIFF <u32 size> WEBP
    return data && size >= 12 &&
           std::memcmp(data, "RIFF", 4) == 0 &&
           std::memcmp(data + 8, "WEBP", 4) == 0;
}

bool webp_decode_rgba(const uint8_t* data, size_t size,
                      std::vector<uint8_t>& rgba, int& width, int& height) {
    rgba.clear();
    width = height = 0;
    if (!data || size == 0) return false;

#if SWORDIGO_HAVE_WEBP
    // WebPGetInfo validates the header, so a mislabelled payload fails here
    // rather than after a full decode.
    int w = 0, h = 0;
    if (!WebPGetInfo(data, size, &w, &h) || w <= 0 || h <= 0) return false;

    // WebPDecodeRGBA handles VP8 (lossy), VP8L (lossless) and VP8X containers
    // with alpha, all as straight (non-premultiplied) RGBA with the top row
    // first — the same orientation stb_image uses for PNG/JPEG.
    uint8_t* pixels = WebPDecodeRGBA(data, size, &w, &h);
    if (!pixels) return false;

    rgba.assign(pixels, pixels + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    WebPFree(pixels);
    width = w;
    height = h;
    return true;
#else
    (void)size;
    return false;
#endif
}

} // namespace av
