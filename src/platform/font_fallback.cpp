#include "platform/font_fallback.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <cstring>
#include <vector>

#if !defined(_WIN32) && !defined(__ANDROID__)
#include <dlfcn.h>
#include <fontconfig/fontconfig.h>

namespace font_fallback {

typedef FcBool (*pfn_FcInit)(void);
typedef FcPattern* (*pfn_FcPatternCreate)(void);
typedef void (*pfn_FcPatternDestroy)(FcPattern*);
typedef FcObjectSet* (*pfn_FcObjectSetBuild)(const char*, ...);
typedef void (*pfn_FcObjectSetDestroy)(FcObjectSet*);
typedef FcFontSet* (*pfn_FcFontList)(FcConfig*, FcPattern*, FcObjectSet*);
typedef void (*pfn_FcFontSetDestroy)(FcFontSet*);
typedef FcResult (*pfn_FcPatternGetString)(const FcPattern*, const char*, int, FcChar8**);
typedef FcPattern* (*pfn_FcNameParse)(const FcChar8*);
typedef FcBool (*pfn_FcConfigSubstitute)(FcConfig*, FcPattern*, FcMatchKind);
typedef void (*pfn_FcDefaultSubstitute)(FcPattern*);
typedef FcPattern* (*pfn_FcFontMatch)(FcConfig*, FcPattern*, FcResult*);

static void* g_fc_handle = nullptr;
static pfn_FcInit fn_FcInit = nullptr;
static pfn_FcPatternCreate fn_FcPatternCreate = nullptr;
static pfn_FcPatternDestroy fn_FcPatternDestroy = nullptr;
static pfn_FcObjectSetBuild fn_FcObjectSetBuild = nullptr;
static pfn_FcObjectSetDestroy fn_FcObjectSetDestroy = nullptr;
static pfn_FcFontList fn_FcFontList = nullptr;
static pfn_FcFontSetDestroy fn_FcFontSetDestroy = nullptr;
static pfn_FcPatternGetString fn_FcPatternGetString = nullptr;
static pfn_FcNameParse fn_FcNameParse = nullptr;
static pfn_FcConfigSubstitute fn_FcConfigSubstitute = nullptr;
static pfn_FcDefaultSubstitute fn_FcDefaultSubstitute = nullptr;
static pfn_FcFontMatch fn_FcFontMatch = nullptr;

static bool g_fc_init_done = false;
static bool g_fc_available = false;
static FcFontSet* g_font_set = nullptr;
static std::mutex g_fc_mutex;
static std::unordered_map<std::string, std::string> g_font_cache;

static bool ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool init_fontconfig_symbols() {
    if (g_fc_init_done) return g_fc_available;
    g_fc_init_done = true;

    g_fc_handle = dlopen("libfontconfig.so.1", RTLD_LAZY);
    if (!g_fc_handle) {
        g_fc_handle = dlopen("libfontconfig.so", RTLD_LAZY);
    }
    if (!g_fc_handle) {
        return false;
    }

    fn_FcInit = (pfn_FcInit)dlsym(g_fc_handle, "FcInit");
    fn_FcPatternCreate = (pfn_FcPatternCreate)dlsym(g_fc_handle, "FcPatternCreate");
    fn_FcPatternDestroy = (pfn_FcPatternDestroy)dlsym(g_fc_handle, "FcPatternDestroy");
    fn_FcObjectSetBuild = (pfn_FcObjectSetBuild)dlsym(g_fc_handle, "FcObjectSetBuild");
    fn_FcObjectSetDestroy = (pfn_FcObjectSetDestroy)dlsym(g_fc_handle, "FcObjectSetDestroy");
    fn_FcFontList = (pfn_FcFontList)dlsym(g_fc_handle, "FcFontList");
    fn_FcFontSetDestroy = (pfn_FcFontSetDestroy)dlsym(g_fc_handle, "FcFontSetDestroy");
    fn_FcPatternGetString = (pfn_FcPatternGetString)dlsym(g_fc_handle, "FcPatternGetString");
    fn_FcNameParse = (pfn_FcNameParse)dlsym(g_fc_handle, "FcNameParse");
    fn_FcConfigSubstitute = (pfn_FcConfigSubstitute)dlsym(g_fc_handle, "FcConfigSubstitute");
    fn_FcDefaultSubstitute = (pfn_FcDefaultSubstitute)dlsym(g_fc_handle, "FcDefaultSubstitute");
    fn_FcFontMatch = (pfn_FcFontMatch)dlsym(g_fc_handle, "FcFontMatch");

    if (!fn_FcInit || !fn_FcPatternCreate || !fn_FcPatternDestroy ||
        !fn_FcObjectSetBuild || !fn_FcObjectSetDestroy ||
        !fn_FcFontList || !fn_FcFontSetDestroy || !fn_FcPatternGetString) {
        dlclose(g_fc_handle);
        g_fc_handle = nullptr;
        return false;
    }

    if (!fn_FcInit()) {
        dlclose(g_fc_handle);
        g_fc_handle = nullptr;
        return false;
    }

    FcPattern* pattern = fn_FcPatternCreate();
    FcObjectSet* os = fn_FcObjectSetBuild(FC_FILE, nullptr);
    if (pattern && os) {
        g_font_set = fn_FcFontList(nullptr, pattern, os);
    }
    if (os) fn_FcObjectSetDestroy(os);
    if (pattern) fn_FcPatternDestroy(pattern);

    g_fc_available = (g_font_set != nullptr);
    if (g_fc_available) {
        std::cout << "[FontConfig] Initialized font fallback (" << g_font_set->nfont << " host fonts indexed)" << std::endl;
    }
    return g_fc_available;
}

std::string resolve_font_path(const std::string& font_request) {
    if (font_request.empty()) return "";

    std::lock_guard<std::mutex> lock(g_fc_mutex);
    if (!init_fontconfig_symbols()) return "";

    auto it = g_font_cache.find(font_request);
    if (it != g_font_cache.end()) {
        return it->second;
    }

    std::string filename = font_request;
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        filename = filename.substr(last_slash + 1);
    }

    // 1. Exact or suffix match in host font set
    if (g_font_set) {
        for (int i = 0; i < g_font_set->nfont; i++) {
            FcPattern* font = g_font_set->fonts[i];
            FcChar8* file = nullptr;
            if (fn_FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file) {
                const char* filepath = (const char*)file;
                if (ends_with(filepath, "/" + filename) || ends_with(filepath, filename)) {
                    std::string result(filepath);
                    g_font_cache[font_request] = result;
                    std::cout << "[FontConfig] Resolved '" << font_request << "' -> '" << result << "'" << std::endl;
                    return result;
                }
            }
        }
    }

    // 2. Pattern match via FcFontMatch (e.g. "Roboto", "Droid Sans", "sans-serif")
    if (fn_FcNameParse && fn_FcConfigSubstitute && fn_FcDefaultSubstitute && fn_FcFontMatch) {
        std::string family = filename;
        if (ends_with(family, ".ttf") || ends_with(family, ".otf")) {
            family = family.substr(0, family.length() - 4);
        }
        FcPattern* pat = fn_FcNameParse((const FcChar8*)family.c_str());
        if (pat) {
            fn_FcConfigSubstitute(nullptr, pat, FcMatchPattern);
            fn_FcDefaultSubstitute(pat);
            FcResult res;
            FcPattern* match = fn_FcFontMatch(nullptr, pat, &res);
            if (match) {
                FcChar8* file = nullptr;
                if (fn_FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file) {
                    std::string result((const char*)file);
                    fn_FcPatternDestroy(match);
                    fn_FcPatternDestroy(pat);
                    g_font_cache[font_request] = result;
                    std::cout << "[FontConfig] Pattern match for '" << font_request << "' -> '" << result << "'" << std::endl;
                    return result;
                }
                fn_FcPatternDestroy(match);
            }
            fn_FcPatternDestroy(pat);
        }
    }

    g_font_cache[font_request] = "";
    return "";
}

std::string resolve_android_system_font(const std::string& path) {
    if (path.rfind("/system/fonts/", 0) == 0) {
        std::string filename = path.substr(14); // skip "/system/fonts/"
        return resolve_font_path(filename);
    }
    return "";
}

} // namespace font_fallback

#else

namespace font_fallback {
std::string resolve_font_path(const std::string&) { return ""; }
std::string resolve_android_system_font(const std::string&) { return ""; }
} // namespace font_fallback

#endif

extern "C" const char* font_fallback_resolve(const char* font_path) {
    if (!font_path || font_path[0] == '\0') return nullptr;
    static thread_local std::string s_last_resolved;
    std::string res;
    if (strncmp(font_path, "/system/fonts/", 14) == 0) {
        res = font_fallback::resolve_android_system_font(font_path);
    } else {
        res = font_fallback::resolve_font_path(font_path);
    }
    if (!res.empty()) {
        s_last_resolved = res;
        return s_last_resolved.c_str();
    }
    return nullptr;
}
