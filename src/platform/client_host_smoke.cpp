#include "platform/client_host_smoke.h"
#include "platform/display.h"
#include "platform/gl_inc.h"
#include "platform/ffp_forward.h"
#include "platform/fbo_scaler.h"
#include "jni/jni_bridge.h"
#include "platform/data_path.h"
#include "platform/os_external.h"
#include "loader/elf_loader_arm64.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

int client_guest_smoke() {
    constexpr uint64_t size=0x100000000ULL;
    auto release=[](uint8_t* p) { os_external::release_large_region(p,0x100000000ULL); };
    std::unique_ptr<uint8_t,decltype(release)> memory(
        static_cast<uint8_t*>(os_external::reserve_large_region(size)),release);
    if (!memory) return 20;
    // Exercise both high guest trampoline/sentinel addresses, not just low ELF pages.
    volatile uint8_t* bytes=memory.get();
    bytes[0]=0x12; bytes[0xff000000ULL]=0x34; bytes[size-1]=0x56;
    if (bytes[0]!=0x12 || bytes[0xff000000ULL]!=0x34 || bytes[size-1]!=0x56) return 21;
    ElfLoaderArm64 loader(memory.get(),size);
    for (const char* name : {"libs/libsre12.so","libs/libsre13.so"}) {
        so_module_arm64 module{};
        if (loader.load(&module,get_data_path(name),0x2000000)!=0) return 22;
        const bool valid=loader.relocate(&module)==0 &&
            loader.get_symbol_vaddr(&module,"sre_init")!=0 && module.num_dynsym>0;
        delete[] reinterpret_cast<uint8_t*>(module.ehdr);
        delete[] reinterpret_cast<uint8_t*>(module.phdr);
        delete[] reinterpret_cast<uint8_t*>(module.shdr);
        if (!valid) return 23;
        std::cout << "[GuestSmoke] ELF load, internal relocations, sre_init lookup: " << name << '\n';
    }
    std::cout << "[GuestSmoke] 4 GiB guest allocation and high-address writes passed. Game imports/execution untested.\n";
    return 0;
}

int client_host_smoke() {
    Display display;
    if (!display.init(64, 64, "Swordigo host diagnostics", true)) return 10;
    glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    std::array<unsigned char,4> pixel{};
    glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    if (glGetError()!=GL_NO_ERROR || pixel[0]<62 || pixel[0]>66 ||
        pixel[1]<126 || pixel[1]>130 || pixel[2]<189 || pixel[2]>193) return 11;
    std::cout << "[HostSmoke] OpenGL clear/readback passed\n";
    ffp::init_from_env();
    ffp::set_quality(ffp::TRANSLATE);
    if (!ffp::available()) return 24;
    std::cout << "[HostSmoke] GLES1 translation shaders compile and link: " << ffp::status() << '\n';

    FrameStats stats;
    stats.last_bound_texture=123; stats.texture_binds=1; stats.reset();
    if (stats.last_bound_texture!=123 || stats.texture_binds!=0) return 28;
    // The engine can reuse a font binding without glBindTexture next frame.
    // Presentation must preserve each texture unit, including early exits.
    if (!fbo_init(64,64)) return 25;
    GLuint guest_textures[4]{};
    glGenTextures(4,guest_textures);
    for (int unit=0; unit<4; ++unit) {
        glActiveTexture(GL_TEXTURE0+unit);
        glBindTexture(GL_TEXTURE_2D,guest_textures[unit]);
    }
    PostFXState postfx;
    for (auto scale : {FBOScale::NEAREST,FBOScale::SHARP_BILINEAR,FBOScale::CRT_SCANLINE}) {
        fbo_begin_game();
        fbo_end_game_and_blit(128,128,scale,&postfx);
        GLint active=0;
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        if (active!=GL_TEXTURE3) return 26;
        for (int unit=0; unit<4; ++unit) {
            glActiveTexture(GL_TEXTURE0+unit);
            GLint binding=0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
            if ((GLuint)binding!=guest_textures[unit]) {
                std::cerr << "[HostSmoke] Presentation lost guest texture unit " << unit << '\n';
                return 27;
            }
        }
    }
    glDeleteTextures(4,guest_textures);
    glActiveTexture(GL_TEXTURE0);
    fbo_destroy();
    std::cout << "[HostSmoke] Post-processing preserves guest texture bindings across frames\n";

    auto* device=alcOpenDevice(nullptr);
    if (!device) return 12;
    auto* context=alcCreateContext(device,nullptr);
    if (!context || !alcMakeContextCurrent(context)) {
        if (context) alcDestroyContext(context);
        alcCloseDevice(device); return 13;
    }
    ALuint buffer=0, source=0;
    std::array<short,2205> silence{};
    alGenBuffers(1,&buffer); alBufferData(buffer,AL_FORMAT_MONO16,silence.data(),sizeof(silence),22050);
    alGenSources(1,&source); alSourcei(source,AL_BUFFER,buffer); alSourcePlay(source);
    const auto audio_error=alGetError();
    std::cout << "[HostSmoke] OpenAL device: " << alcGetString(device,ALC_DEVICE_SPECIFIER) << '\n';
    alSourceStop(source); alDeleteSources(1,&source); alDeleteBuffers(1,&buffer);
    alcMakeContextCurrent(nullptr); alcDestroyContext(context); alcCloseDevice(device);
    if (audio_error!=AL_NO_ERROR) return 14;
    std::cout << "[HostSmoke] OpenAL silent PCM buffer submission passed (audibility untested)\n";

    SDL_Event event{}; event.type=SDL_EVENT_KEY_DOWN; event.key.scancode=SDL_SCANCODE_SPACE;
    if (!SDL_PushEvent(&event)) return 15;
    bool key=false;
    while (SDL_PollEvent(&event)) if (event.type==SDL_EVENT_KEY_DOWN && event.key.scancode==SDL_SCANCODE_SPACE) key=true;
    if (!key) return 16;
    std::cout << "[HostSmoke] SDL synthetic keyboard event passed\n";

    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path() / fs::u8path(u8"swordigo save ä "+std::to_string(SDL_GetTicksNS()));
    const auto documents=fs::u8path(get_vfs_save_dir(root.u8string()));
    fs::create_directories(documents);
    const auto path=documents / "host-test.bin";
    char resolved[1024]={};
    if (!resolve_vfs_path(path.u8string().c_str(),resolved,sizeof(resolved))) return 19;
    { std::ofstream f(path,std::ios::binary); f << "Swordigo host save IO"; if (!f) return 17; }
    std::string value; { std::ifstream f(path,std::ios::binary); std::getline(f,value); }
    fs::remove(path); fs::remove(documents); fs::remove(root);
    if (value!="Swordigo host save IO") return 18;
    std::cout << "[HostSmoke] VFS save directory and Unicode file roundtrip passed\n";
    return 0;
}
