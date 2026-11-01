#define LOG_TAG "SRE13Assets"
#include "hook.h"
#include "stdstring.h"
#include "log.h"
#include "Gloss.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern char g_sre_vfs_mod_name[128];
static char g_active_mod[128] = {0};

void sre13_set_active_mod(const char *mod_id) {
    if (!mod_id) {
        g_active_mod[0] = '\0';
        g_sre_vfs_mod_name[0] = '\0';
        return;
    }
    strncpy(g_active_mod, mod_id, sizeof(g_active_mod) - 1);
    g_active_mod[sizeof(g_active_mod) - 1] = '\0';
    strncpy(g_sre_vfs_mod_name, mod_id, sizeof(g_sre_vfs_mod_name) - 1);
    g_sre_vfs_mod_name[sizeof(g_sre_vfs_mod_name) - 1] = '\0';
    LOGI("sre13_set_active_mod: \"%s\"", g_active_mod);
}

const char* sre13_get_active_mod(void) {
    if (g_active_mod[0]) return g_active_mod;
    if (g_sre_vfs_mod_name[0]) return g_sre_vfs_mod_name;
    return "";
}

static int file_exists(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

extern char g_sre_vfs_data_dir[512];

static int resolve_mod_resource(const char *filename, char *out_buf, size_t out_sz) {
    const char *mod = sre13_get_active_mod();
    if (!mod || !*mod || !filename || !*filename) return 0;

    /* Check ~/.local/share/swordigo-desktop/mods/<mod>/... if g_sre_vfs_data_dir is set */
    if (g_sre_vfs_data_dir[0]) {
        char base[512];
        strncpy(base, g_sre_vfs_data_dir, sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';
        size_t blen = strlen(base);
        while (blen > 0 && (base[blen - 1] == '/' || base[blen - 1] == '\\')) {
            base[--blen] = '\0';
        }

        /* 1. Primary: <data_dir>/mods/<mod>/resources/<file> (standard Lawncher / Raijin layout) */
        snprintf(out_buf, out_sz, "%s/mods/%s/resources/%s", base, mod, filename);
        if (file_exists(out_buf)) return 1;

        /* 2. Secondary: <data_dir>/mods/<mod>/assets/<file> */
        snprintf(out_buf, out_sz, "%s/mods/%s/assets/%s", base, mod, filename);
        if (file_exists(out_buf)) return 1;

        /* 3. Flat: <data_dir>/mods/<mod>/<file> */
        snprintf(out_buf, out_sz, "%s/mods/%s/%s", base, mod, filename);
        if (file_exists(out_buf)) return 1;
    }

    /* Relative fallbacks (CWD / local execution) */
    /* 1. Primary: mods/<mod>/resources/<file> (standard Lawncher / Raijin layout) */
    snprintf(out_buf, out_sz, "mods/%s/resources/%s", mod, filename);
    if (file_exists(out_buf)) return 1;

    /* 2. Secondary: mods/<mod>/assets/<file> */
    snprintf(out_buf, out_sz, "mods/%s/assets/%s", mod, filename);
    if (file_exists(out_buf)) return 1;

    /* 3. Flat: mods/<mod>/<file> */
    snprintf(out_buf, out_sz, "mods/%s/%s", mod, filename);
    if (file_exists(out_buf)) return 1;

    return 0;
}

HOOK_SYMBOL(
	NewByteBufferFromAA,
	"_ZN5Caver29NewByteBufferFromAndroidAssetERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEPj",
	void*, (String *file, unsigned int *out_size)
) {
	const char *filename = String_get(file);
	char respath[512];
	if (resolve_mod_resource(filename, respath, sizeof(respath))) {
		FILE *f = fopen(respath, "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			long sz = ftell(f);
			fseek(f, 0, SEEK_SET);
			if (sz >= 0) {
				void *buf = malloc((size_t)sz);
				if (buf) {
					size_t read_bytes = fread(buf, 1, (size_t)sz, f);
					fclose(f);
					if (out_size) *out_size = (unsigned int)read_bytes;
					LOGI("Loaded modded asset: %s (%zu bytes)", respath, read_bytes);
					return buf;
				}
			}
			fclose(f);
		}
	}

	if (orig_NewByteBufferFromAA) {
		return orig_NewByteBufferFromAA(file, out_size);
	}
	return NULL;
}

/* BinaryFile_Open — loads modded backgrounds and gzip compressed levels */
typedef void* (*gzdopen_fn)(int, const char*);
static gzdopen_fn p_gzdopen = NULL;

HOOK_SYMBOL(
	BinaryFile_Open,
	"_ZN5Caver10BinaryFile4OpenERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEENS0_4ModeEb",
	unsigned int, (void *this_ptr, String *filename, int mode, bool use_asset)
) {
	const char *name = String_get(filename);
	char respath[512];
	if (resolve_mod_resource(name, respath, sizeof(respath))) {
		int fd = open(respath, O_RDONLY);
		if (fd >= 0) {
			if (!p_gzdopen) {
				p_gzdopen = (gzdopen_fn)srehost_get_symbol("gzdopen");
			}
			if (p_gzdopen) {
				const char *m = (mode == 1) ? "wb" : "rb";
				void *gz = p_gzdopen(fd, m);
				if (gz) {
					*(int *)this_ptr = 2;
					*(void **)((char*)this_ptr + 8) = gz;
					*(int *)((char*)this_ptr + 0x10) = 0;
					LOGI("Modded BinaryFile opened: %s", respath);
					return 1;
				}
			}
			close(fd);
		}
	}
	if (orig_BinaryFile_Open) {
		return orig_BinaryFile_Open(this_ptr, filename, mode, use_asset);
	}
	return 0;
}

/* GetAudioFileData — loads modded WAV audio files */
HOOK_SYMBOL(
	GetAudioFileData,
	"_ZN5Caver16GetAudioFileDataERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEPNS_11AudioBuffer12BufferFormatEPPvPiSE_",
	bool, (String *path, int *fmt, void **data, int *size, int *rate)
) {
	const char *name = String_get(path);
	char respath[512];
	if (resolve_mod_resource(name, respath, sizeof(respath))) {
		FILE *f = fopen(respath, "rb");
		if (f) {
			unsigned char hdr[0x2c];
			if (fread(hdr, 1, 0x2c, f) == 0x2c) {
				/* Validate RIFF ... WAVE ... fmt  ... data */
				unsigned int magic_riff = *(unsigned int*)(hdr + 0);
				unsigned int magic_wave = *(unsigned int*)(hdr + 8);
				unsigned int magic_fmt  = *(unsigned int*)(hdr + 12);
				unsigned int magic_data = *(unsigned int*)(hdr + 36);

				if (magic_riff == 0x46464952 && magic_wave == 0x45564157 &&
				    magic_fmt  == 0x20746d66 && magic_data == 0x61746164) {
					unsigned int datasz = *(unsigned int*)(hdr + 40);
					void *buf = malloc(datasz);
					if (buf && fread(buf, 1, datasz, f) == datasz) {
						fclose(f);
						short ch = *(short*)(hdr + 22);
						short bits = *(short*)(hdr + 34);
						int sr = *(int*)(hdr + 24);

						int bf = 0;
						if (bits == 16) bf = (ch == 1) ? 2 : 4;
						else if (bits == 8) bf = (ch == 1) ? 1 : 3;

						if (fmt)  *fmt  = bf;
						if (data) *data = buf;
						if (size) *size = (int)datasz;
						if (rate) *rate = sr;
						LOGI("Loaded modded WAV: %s (%u bytes, %d Hz)", respath, datasz, sr);
						return true;
					}
					if (buf) free(buf);
				}
			}
			fclose(f);
		}
	}

	if (orig_GetAudioFileData) {
		return orig_GetAudioFileData(path, fmt, data, size, rate);
	}
	return false;
}
