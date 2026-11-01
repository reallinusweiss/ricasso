#define FFMPEG_DYN_NO_MACROS
#include "platform/ffmpeg_dyn.h"
#include <iostream>
#include <string>
#include <mutex>
#include <vector>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

struct FFmpegApi ffmpeg_api;

static void* g_lib_avformat = nullptr;
static void* g_lib_avcodec = nullptr;
static void* g_lib_swscale = nullptr;
static void* g_lib_avutil = nullptr;

static bool g_resolved = false;
static bool g_available = false;
static std::string g_fail_reason = "FFmpeg backend not initialized";
static std::mutex g_mutex;

namespace {

std::string exe_directory() {
#ifdef _WIN32
    wchar_t buf[4096];
    DWORD len = GetModuleFileNameW(nullptr, buf, sizeof(buf) / sizeof(buf[0]));
    if (len == 0 || len >= sizeof(buf) / sizeof(buf[0])) return std::string();
    std::wstring ws(buf, len);
    size_t slash = ws.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return std::string();
    std::wstring dir = ws.substr(0, slash + 1);
    int need = WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), (int)dir.size(), nullptr, 0, nullptr, nullptr);
    if (need <= 0) return std::string();
    std::string out(need, '\0');
    WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), (int)dir.size(), &out[0], need, nullptr, nullptr);
    return out;
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return std::string();
    buf[len] = '\0';
    std::string path(buf);
    size_t slash = path.find_last_of('/');
    if (slash == std::wstring::npos) return std::string();
    return path.substr(0, slash + 1);
#endif
}

void* try_load(const char* const names[]) {
    std::string exe_dir = exe_directory();

    // 1. Try next to running executable
    for (int i = 0; names[i]; ++i) {
        if (!exe_dir.empty()) {
            std::string local_path = exe_dir + names[i];
#ifdef _WIN32
            void* h = (void*)LoadLibraryA(local_path.c_str());
#else
            void* h = dlopen(local_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
            if (h) return h;
        }
    }

    // 2. Try standard system library lookup
    for (int i = 0; names[i]; ++i) {
#ifdef _WIN32
        void* h = (void*)LoadLibraryA(names[i]);
#else
        void* h = dlopen(names[i], RTLD_NOW | RTLD_GLOBAL);
#endif
        if (h) return h;
    }

    return nullptr;
}

void* resolve_sym(void* handle, const char* name) {
    if (!handle) return nullptr;
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)handle, name);
#else
    return dlsym(handle, name);
#endif
}

} // anonymous namespace

bool ffmpeg_dyn_init(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_resolved) return g_available;

    g_resolved = true;

#ifdef _WIN32
    const char* avutil_names[]   = { "avutil-59.dll", "avutil-58.dll", "avutil-57.dll", "avutil-56.dll", "avutil.dll", nullptr };
    const char* avcodec_names[]  = { "avcodec-61.dll", "avcodec-60.dll", "avcodec-59.dll", "avcodec-58.dll", "avcodec.dll", nullptr };
    const char* avformat_names[] = { "avformat-61.dll", "avformat-60.dll", "avformat-59.dll", "avformat-58.dll", "avformat.dll", nullptr };
    const char* swscale_names[]  = { "swscale-8.dll", "swscale-7.dll", "swscale-6.dll", "swscale-5.dll", "swscale.dll", nullptr };
#else
    const char* avutil_names[]   = { "libavutil.so.59", "libavutil.so.58", "libavutil.so.57", "libavutil.so.56", "libavutil.so", nullptr };
    const char* avcodec_names[]  = { "libavcodec.so.61", "libavcodec.so.60", "libavcodec.so.59", "libavcodec.so.58", "libavcodec.so", nullptr };
    const char* avformat_names[] = { "libavformat.so.61", "libavformat.so.60", "libavformat.so.59", "libavformat.so.58", "libavformat.so", nullptr };
    const char* swscale_names[]  = { "libswscale.so.8", "libswscale.so.7", "libswscale.so.6", "libswscale.so.5", "libswscale.so", nullptr };
#endif

    // Load dependencies in dependency order: avutil -> avcodec -> avformat -> swscale
    g_lib_avutil   = try_load(avutil_names);
    g_lib_avcodec  = try_load(avcodec_names);
    g_lib_avformat = try_load(avformat_names);
    g_lib_swscale  = try_load(swscale_names);

    if (!g_lib_avutil || !g_lib_avcodec || !g_lib_avformat || !g_lib_swscale) {
        g_fail_reason = "Missing one or more required FFmpeg libraries (libavcodec, libavformat, libswscale, libavutil).";
        std::cout << "[VideoBackground] " << g_fail_reason << "\n"
                  << "[VideoBackground] Animated backgrounds (VBG) disabled; falling back to vanilla textures.\n"
                  << "[VideoBackground] To enable animated backgrounds, install FFmpeg on your system:\n"
                  << "    Debian/Ubuntu: sudo apt install ffmpeg\n"
                  << "    Fedora:        sudo dnf install ffmpeg-free\n"
                  << "    Arch Linux:    sudo pacman -S ffmpeg\n" << std::endl;
        g_available = false;
        return false;
    }

    // Resolve libavformat functions
    *(void**)&ffmpeg_api.avformat_open_input       = resolve_sym(g_lib_avformat, "avformat_open_input");
    *(void**)&ffmpeg_api.avformat_close_input      = resolve_sym(g_lib_avformat, "avformat_close_input");
    *(void**)&ffmpeg_api.avformat_find_stream_info = resolve_sym(g_lib_avformat, "avformat_find_stream_info");
    *(void**)&ffmpeg_api.av_read_frame             = resolve_sym(g_lib_avformat, "av_read_frame");
    *(void**)&ffmpeg_api.av_seek_frame             = resolve_sym(g_lib_avformat, "av_seek_frame");

    // Resolve libavutil functions
    *(void**)&ffmpeg_api.av_strerror               = resolve_sym(g_lib_avutil, "av_strerror");
    *(void**)&ffmpeg_api.av_frame_alloc            = resolve_sym(g_lib_avutil, "av_frame_alloc");
    *(void**)&ffmpeg_api.av_frame_free             = resolve_sym(g_lib_avutil, "av_frame_free");

    // Resolve libavcodec functions
    *(void**)&ffmpeg_api.avcodec_find_decoder           = resolve_sym(g_lib_avcodec, "avcodec_find_decoder");
    *(void**)&ffmpeg_api.avcodec_alloc_context3         = resolve_sym(g_lib_avcodec, "avcodec_alloc_context3");
    *(void**)&ffmpeg_api.avcodec_free_context           = resolve_sym(g_lib_avcodec, "avcodec_free_context");
    *(void**)&ffmpeg_api.avcodec_parameters_to_context  = resolve_sym(g_lib_avcodec, "avcodec_parameters_to_context");
    *(void**)&ffmpeg_api.avcodec_open2                  = resolve_sym(g_lib_avcodec, "avcodec_open2");
    *(void**)&ffmpeg_api.avcodec_send_packet            = resolve_sym(g_lib_avcodec, "avcodec_send_packet");
    *(void**)&ffmpeg_api.avcodec_receive_frame          = resolve_sym(g_lib_avcodec, "avcodec_receive_frame");
    *(void**)&ffmpeg_api.avcodec_flush_buffers          = resolve_sym(g_lib_avcodec, "avcodec_flush_buffers");
    *(void**)&ffmpeg_api.av_packet_alloc                = resolve_sym(g_lib_avcodec, "av_packet_alloc");
    *(void**)&ffmpeg_api.av_packet_free                 = resolve_sym(g_lib_avcodec, "av_packet_free");
    *(void**)&ffmpeg_api.av_packet_unref                = resolve_sym(g_lib_avcodec, "av_packet_unref");

    // Resolve libswscale functions
    *(void**)&ffmpeg_api.sws_getContext                 = resolve_sym(g_lib_swscale, "sws_getContext");
    *(void**)&ffmpeg_api.sws_scale                      = resolve_sym(g_lib_swscale, "sws_scale");
    *(void**)&ffmpeg_api.sws_freeContext                = resolve_sym(g_lib_swscale, "sws_freeContext");

    // Check that all critical symbols are resolved
    if (!ffmpeg_api.avformat_open_input || !ffmpeg_api.avcodec_send_packet ||
        !ffmpeg_api.avcodec_receive_frame || !ffmpeg_api.sws_scale) {
        g_fail_reason = "Failed to resolve one or more critical FFmpeg function symbols.";
        std::cerr << "[VideoBackground] " << g_fail_reason << std::endl;
        g_available = false;
        return false;
    }

    std::cout << "[VideoBackground] Successfully dynamically loaded system FFmpeg runtime." << std::endl;
    g_available = true;
    return true;
}

bool ffmpeg_dyn_available(void) {
    return g_available;
}

const char* ffmpeg_dyn_fail_reason(void) {
    return g_fail_reason.c_str();
}
