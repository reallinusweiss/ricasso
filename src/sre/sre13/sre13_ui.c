/*
 * sre13_ui.c — Desktop UI, text input, and profile management for Swordigo 1.4.13
 */

#include "sre13.h"

/* Text input state */
volatile uint64_t g_sre_text_delegate = 0;
volatile int      g_sre_text_input_active = 0;
char              g_sre_text_input_init_val[256] = {0};

uint64_t g_orig_ProfileSelectionView_LoadProfiles = 0;

static void sre_strcpy(char* dst, const char* src, int max) {
    if (!dst || !src) return;
    int i;
    for (i = 0; i < max - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

static const char* libcxx_string_data(const void* str) {
    if (!str) return "";
    const uint8_t* p = (const uint8_t*)str;
    if ((p[0] & 1) != 0) {
        const char* ptr = *(const char**)(p + 16);
        return ptr ? ptr : "";
    } else {
        return (const char*)(p + 1);
    }
}

/* StartTextInputWithDelegate */
void sre_StartTextInputWithDelegate(void* delegate, void* text) {
    g_sre_text_delegate = (uint64_t)delegate;
    g_sre_text_input_active = 1;

    g_sre_text_input_init_val[0] = '\0';
    if (text) {
        const char* str = libcxx_string_data(text);
        if (str) {
            sre_strcpy(g_sre_text_input_init_val, str, sizeof(g_sre_text_input_init_val));
        }
    }
}

/* StopTextInputWithDelegate */
void sre_StopTextInputWithDelegate(void* delegate) {
    (void)delegate;
    g_sre_text_delegate = 0;
    g_sre_text_input_active = 0;
    g_sre_text_input_init_val[0] = '\0';
}

/* textInputTextDidChange */
void sre_textInputTextDidChange(void* env, void* cls, void* jstr) {
    (void)env; (void)cls; (void)jstr;
}

/* textInputDidFinish */
void sre_textInputDidFinish(void* env, void* cls) {
    (void)env; (void)cls;
    g_sre_text_input_active = 0;
}
