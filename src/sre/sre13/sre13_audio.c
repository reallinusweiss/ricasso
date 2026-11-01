/*
 * sre13_audio.c — SRE Audio & MusicPlayer hooks for Swordigo 1.4.13
 */

#include "sre13.h"

/* Music command interface (read by host main loop) */
char g_sre_music_load_name[256] = {0};
int  g_sre_music_load_pending = 0;
int  g_sre_music_load_restart = 0;
int  g_sre_music_play_pending = 0;
int  g_sre_music_pause_pending = 0;
int  g_sre_music_stop_pending = 0;
float g_sre_music_volume = 1.0f;
int   g_sre_music_volume_dirty = 0;
int   g_sre_music_looping = 1;
int   g_sre_music_looping_dirty = 0;
float g_sre_music_master_volume = 1.0f;

static float s_fade_volume = 1.0f;
static float s_fade_target = 1.0f;
static float s_fade_speed = 0.0f;
static int   s_enabled = 1;
static int   s_suspended = 0;
static int   s_playing = 0;
static char  s_current_name[256] = {0};

static void sre_strcpy(char* dst, const char* src, int max) {
    if (!dst || !src) return;
    int i;
    for (i = 0; i < max - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

static int sre_strcmp(const char* a, const char* b) {
    if (!a || !b) return (a == b) ? 0 : 1;
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

/* Helper to extract const char* from libc++ std::string */
static const char* libcxx_string_data(const void* str) {
    if (!str) return "";
    const uint8_t* p = (const uint8_t*)str;
    if ((p[0] & 1) != 0) {
        /* Long string: data pointer at +16 */
        const char* ptr = *(const char**)(p + 16);
        return ptr ? ptr : "";
    } else {
        /* Short string: data at +1 */
        return (const char*)(p + 1);
    }
}

static void apply_effective_volume(void) {
    float effective = g_sre_music_master_volume * s_fade_volume;
    if (!s_enabled || s_suspended) {
        effective = 0.0f;
    }
    if (effective != g_sre_music_volume) {
        g_sre_music_volume = effective;
        g_sre_music_volume_dirty = 1;
    }
}

/* MusicPlayer::PlayMusicWithName */
void sre_PlayMusicWithName(void* self, const void* name_str, int restart) {
    (void)self;
    const char* name = libcxx_string_data(name_str);
    if (!name || !name[0]) return;

    if (!restart && s_playing && sre_strcmp(s_current_name, name) == 0) {
        return;
    }

    sre_strcpy(s_current_name, name, sizeof(s_current_name));
    sre_strcpy(g_sre_music_load_name, name, sizeof(g_sre_music_load_name));
    g_sre_music_load_restart = restart;
    g_sre_music_load_pending = 1;
    s_playing = 1;

    s_fade_volume = 1.0f;
    s_fade_target = 1.0f;
    s_fade_speed = 0.0f;
    apply_effective_volume();
}

/* MusicPlayer::FadeIn */
void sre_MusicPlayer_FadeIn(void* self, float duration) {
    (void)self;
    if (duration <= 0.0f) duration = 1.0f;
    s_fade_target = 1.0f;
    s_fade_speed = 1.0f / duration;
}

/* MusicPlayer::FadeOut */
void sre_MusicPlayer_FadeOut(void* self, float duration) {
    (void)self;
    if (duration <= 0.0f) duration = 1.0f;
    s_fade_target = 0.0f;
    s_fade_speed = 1.0f / duration;
}

/* MusicPlayer::Update */
void sre_MusicPlayer_Update(void* self, float dt) {
    (void)self;
    if (s_fade_speed > 0.0f && s_fade_volume != s_fade_target) {
        if (s_fade_volume < s_fade_target) {
            s_fade_volume += s_fade_speed * dt;
            if (s_fade_volume >= s_fade_target) {
                s_fade_volume = s_fade_target;
                s_fade_speed = 0.0f;
            }
        } else {
            s_fade_volume -= s_fade_speed * dt;
            if (s_fade_volume <= s_fade_target) {
                s_fade_volume = s_fade_target;
                s_fade_speed = 0.0f;
            }
        }
        apply_effective_volume();
    }
    sre13_scene_shifter_tick();
}

/* AudioSystem::SetMusicVolume */
void sre_AudioSystem_SetMusicVolume(void* self, float vol) {
    (void)self;
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    g_sre_music_master_volume = vol;
    apply_effective_volume();
}

/* MusicPlayer::SetEnabled */
void sre_MusicPlayer_SetEnabled(void* self, int enabled) {
    (void)self;
    s_enabled = enabled;
    apply_effective_volume();
}

/* MusicPlayer::SetSuspended */
void sre_MusicPlayer_SetSuspended(void* self, int suspended) {
    (void)self;
    s_suspended = suspended;
    apply_effective_volume();
}
