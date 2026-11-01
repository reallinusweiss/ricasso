#pragma once
#include <string>

#ifdef __cplusplus
namespace font_fallback {
    // Resolves a font filename (e.g. "Roboto-Regular.ttf", "DroidSans.ttf", "Inter-Regular.ttf")
    // or pattern against host system fonts using FontConfig.
    // Returns absolute file path, or empty string if not found.
    std::string resolve_font_path(const std::string& font_request);

    // Checks if the given path is an Android system font request (e.g. starts with "/system/fonts/")
    // and maps it to a matching host system font.
    std::string resolve_android_system_font(const std::string& path);
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// C wrapper for VFS / asset manager / bridge calls
const char* font_fallback_resolve(const char* font_path);
#ifdef __cplusplus
}
#endif
