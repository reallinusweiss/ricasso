#define LOG_TAG "PlayerProfile"
#include <stdlib.h>
#include <string.h>
#include "../core/hook.h"
#include "../caver/PlayerProfile.h"
#include "../core/stdstring.h"
#include "../core/log.h"

static char *sre_strdup(const char *s) {
	if (!s) return NULL;
	size_t len = strlen(s);
	char *p = (char*)malloc(len + 1);
	if (!p) return NULL;
	memcpy(p, s, len + 1);
	return p;
}
#define strdup sre_strdup

static char *g_profile_id = NULL;

const char* profile_get_id(void) {
	return g_profile_id;
}

HOOK_SYMBOL(
	LoadGameState_Hook,
	"_ZN5Caver13PlayerProfile13LoadGameStateEv",
	void, (PlayerProfile *profile)
) {
	LOGD("Loaded Game State.");
	if (orig_LoadGameState_Hook) {
		orig_LoadGameState_Hook(profile);
	}
}

HOOK_SYMBOL(
	LoadFromPath_Hook,
	"_ZN5Caver13PlayerProfile12LoadFromPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEb",
	void, (PlayerProfile *profile, String *path, bool unknown)
) {
	LOGD("Path: %s", String_get(path));
	if (orig_LoadFromPath_Hook) {
		orig_LoadFromPath_Hook(profile, path, unknown);
	}
	LOGD("...ID: %s", String_get(&profile->Identifier));
}

HOOK_SYMBOL(
	LoadFromProtobufMessage_Hook,
	"_ZN5Caver13PlayerProfile23LoadFromProtobufMessageERKNS_5Proto13PlayerProfileEb",
	void, (PlayerProfile *profile, void *message, bool unknown)
) {
	LOGD("Loading from a Protobuf Message...");
	if (orig_LoadFromProtobufMessage_Hook) {
		orig_LoadFromProtobufMessage_Hook(profile, message, unknown);
	}
}

HOOK_SYMBOL(
	ProfileSelection_DidStart,
	"_ZN5Caver22MainMenuViewController38ProfileSelectionViewControllerDidStartEPNS_20ProfileSelectionViewERKN5boost10shared_ptrINS_13PlayerProfileEEE",
	void, (void *this_ptr, void *profile_selection_view, void **boost_shared_profile)
) {
	if (boost_shared_profile && *boost_shared_profile) {
		PlayerProfile *profile = (PlayerProfile*)(*boost_shared_profile);
		const char *id = String_get(&profile->Identifier);
		LOGD("Selected Profile ID: '%s'", id ? id : "null");
		if (g_profile_id) {
			free(g_profile_id);
			g_profile_id = NULL;
		}
		if (id) {
			g_profile_id = strdup(id);
		}
	}
	if (orig_ProfileSelection_DidStart) {
		orig_ProfileSelection_DidStart(this_ptr, profile_selection_view, boost_shared_profile);
	}
}

G_DL_SYMBOL(
	PlayerProfile_Load,
	"_ZN5Caver13PlayerProfile4LoadEv",
	void, (PlayerProfile *profile)
);

G_DL_SYMBOL(
	PlayerProfile_Save,
	"_ZN5Caver13PlayerProfile4SaveEb",
	void, (PlayerProfile *profile, bool unknown)
);

G_DL_SYMBOL(
	PlayerProfile_LoadGameState,
	"_ZN5Caver13PlayerProfile13LoadGameStateEv",
	void, (PlayerProfile *profile)
);

G_DL_SYMBOL(
	PlayerProfile_LoadFromPath,
	"_ZN5Caver13PlayerProfile12LoadFromPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEb",
	void, (PlayerProfile *profile, String *path, bool unknown)
);

G_DL_SYMBOL(
	PlayerProfile_UpdateLastPlayedTime,
	"_ZN5Caver13PlayerProfile20UpdateLastPlayedTimeEv",
	void, (PlayerProfile *profile)
);

G_DL_SYMBOL(
	PlayerProfile_ValueForCounter,
	"_ZN5Caver13PlayerProfile15ValueForCounterERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE",
	int, (PlayerProfile *profile, String *name)
);

G_DL_SYMBOL(
	PlayerProfile_SetValueForCounter,
	"_ZN5Caver13PlayerProfile18SetValueForCounterERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEi",
	void, (PlayerProfile *profile, String *name, int value)
);

G_DL_SYMBOL(
	PlayerProfile_IncreaseCounterValue,
	"_ZN5Caver13PlayerProfile20IncreaseCounterValueERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEi",
	void, (PlayerProfile *profile, String *name, int delta)
);
