#define LOG_TAG "SRE13Saves"
#include "hook.h"
#include "stdstring.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern const char* sre13_get_active_mod(void);

static void ensure_dir(const char *path) {
	char buf[512];
	snprintf(buf, sizeof(buf), "%s", path);
	for (char *p = buf + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(buf, 0770);
			*p = '/';
		}
	}
	mkdir(buf, 0770);
}

static const char *path_basename(const char *path) {
	const char *s = strrchr(path, '/');
	return s ? s + 1 : path;
}

static int is_save_ext(const char *ext) {
	return ext && strcmp(ext, "gplayer") == 0;
}

static int is_save_path(const char *p) {
	return p && strstr(p, ".gplayer") != NULL;
}

static void redirect_path(String *out, const char *orig) {
	const char *id = sre13_get_active_mod();
	if (!id || !*id) {
		String_create(out, orig);
		return;
	}
	const char *base = path_basename(orig);
	char full[512];
	char dir[512];
	snprintf(dir, sizeof(dir), "mods/%s/saves/", id);
	ensure_dir(dir);
	snprintf(full, sizeof(full), "%s%s", dir, base);
	LOGD("Redirect save %s -> %s", orig, full);
	String_create(out, full);
}

HOOK_SYMBOL(
	GetFilesWithExtension,
	"_ZN5Caver21GetFilesWithExtensionERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_PNS0_6vectorIS6_NS4_IS6_EEEE",
	void, (String *extension, String *path, void *outfiles)
) {
	const char *ext = String_get(extension);
	const char *p = String_get(path);
	const char *id = sre13_get_active_mod();
	if (id && *id && is_save_ext(ext)) {
		char savesdir[512];
		snprintf(savesdir, sizeof(savesdir), "mods/%s/saves/", id);
		ensure_dir(savesdir);
		String modpath;
		String_create(&modpath, savesdir);
		if (orig_GetFilesWithExtension) {
			orig_GetFilesWithExtension(extension, &modpath, outfiles);
		}
		String_destroy(&modpath);
		return;
	}
	if (orig_GetFilesWithExtension) {
		orig_GetFilesWithExtension(extension, path, outfiles);
	}
}

HOOK_SYMBOL(
	NewByteBufferFromFile,
	"_ZN5Caver21NewByteBufferFromFileERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEPj",
	void*, (String *path, unsigned int *out_size)
) {
	const char *p = String_get(path);
	if (!is_save_path(p)) {
		return orig_NewByteBufferFromFile ? orig_NewByteBufferFromFile(path, out_size) : NULL;
	}
	String s;
	redirect_path(&s, p);
	void *ret = orig_NewByteBufferFromFile ? orig_NewByteBufferFromFile(&s, out_size) : NULL;
	String_destroy(&s);
	return ret;
}

HOOK_SYMBOL(
	SaveByteBufferToFile,
	"_ZN5Caver20SaveByteBufferToFileEPKhjRKNSt6__ndk112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEE",
	unsigned int, (const unsigned char *buf, unsigned int size, String *path)
) {
	const char *p = String_get(path);
	if (!is_save_path(p)) {
		return orig_SaveByteBufferToFile ? orig_SaveByteBufferToFile(buf, size, path) : 0;
	}
	String s;
	redirect_path(&s, p);
	unsigned int ret = orig_SaveByteBufferToFile ? orig_SaveByteBufferToFile(buf, size, &s) : 0;
	String_destroy(&s);
	return ret;
}

HOOK_SYMBOL(
	FileExists,
	"_ZN5Caver10FileExistsERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
	bool, (String *path)
) {
	const char *p = String_get(path);
	if (!is_save_path(p)) {
		return orig_FileExists ? orig_FileExists(path) : false;
	}
	String s;
	redirect_path(&s, p);
	bool ret = orig_FileExists ? orig_FileExists(&s) : false;
	String_destroy(&s);
	return ret;
}

HOOK_SYMBOL(
	RemoveFile,
	"_ZN5Caver10RemoveFileERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
	bool, (String *path)
) {
	const char *p = String_get(path);
	if (!is_save_path(p)) {
		return orig_RemoveFile ? orig_RemoveFile(path) : false;
	}
	String s;
	redirect_path(&s, p);
	bool ret = orig_RemoveFile ? orig_RemoveFile(&s) : false;
	String_destroy(&s);
	return ret;
}
