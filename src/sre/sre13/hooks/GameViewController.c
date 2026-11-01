#include "../caver/GameViewController.h"
#include "../caver/GameState.h"
#include "lua.h"

static GameViewController *g_gvc = NULL;
static GameState *g_gameState = NULL;

GameViewController *gvc_from_L(lua_State *L) {
	lua_getglobal(L, "gameController");
	if (!lua_islightuserdata(L, -1)) return NULL;
	const void *gvc = lua_topointer(L, -1);
	lua_pop(L, 1);
	return (GameViewController *)gvc;
}

extern void* sre13_get_active_gvc(void);

GameViewController *gvc_get(void) {
	if (g_gvc) return g_gvc;
	return (GameViewController *)sre13_get_active_gvc();
}

GameState *gameState_get(void) {
	if (g_gameState) return g_gameState;
	GameViewController *gvc = gvc_get();
	return gvc ? gvc->GameState : NULL;
}

extern void sre13_scene_shifter_tick(void);

/* Host-triggered developer overlays & debug toggles */
volatile int g_sre_request_toggle_debug_info = 0;
volatile int g_sre_request_toggle_collision_shapes = 0;
volatile int g_sre_request_toggle_combat_wireframe = 0;

static void sre13_handle_dev_toggles(GameViewController *gvc) {
	if (!gvc) return;

	if (g_sre_request_toggle_debug_info) {
		g_sre_request_toggle_debug_info = 0;
		typedef void (*ToggleDebugInfo_fn)(void*);
		static ToggleDebugInfo_fn s_toggle_fn = NULL;
		if (!s_toggle_fn) {
			s_toggle_fn = (ToggleDebugInfo_fn)swordigo_dlsym("_ZN5Caver13GameSceneView15ToggleDebugInfoEv");
		}
		if (s_toggle_fn && gvc->GameSceneView) {
			s_toggle_fn(gvc->GameSceneView);
		}
	}

	if (g_sre_request_toggle_collision_shapes) {
		g_sre_request_toggle_collision_shapes = 0;
		static uint8_t* s_draw_depth = NULL;
		if (!s_draw_depth) {
			s_draw_depth = (uint8_t*)swordigo_dlsym("_ZN5Caver23CollisionShapeComponent9drawDepthE");
		}
		if (s_draw_depth) {
			*s_draw_depth ^= 1;
		}
		if (gvc->GameSceneController) {
			void* scene = *(void**)((char*)gvc->GameSceneController + 0x20);
			if (scene) {
				*(uint8_t*)((char*)scene + 720) = s_draw_depth ? *s_draw_depth : 1;
			}
		}
	}

	if (g_sre_request_toggle_combat_wireframe) {
		g_sre_request_toggle_combat_wireframe = 0;
		if (gvc->GameSceneView) {
			uint8_t cur = *(uint8_t*)((char*)gvc->GameSceneView + 434) ^ 1;
			*(uint8_t*)((char*)gvc->GameSceneView + 434) = cur;
			if (gvc->GameSceneController) {
				void* scene = *(void**)((char*)gvc->GameSceneController + 0x20);
				if (scene) {
					*(uint8_t*)((char*)scene + 892) = cur;
				}
			}
		}
	}
}

HOOK_SYMBOL(
	GVC_Update_Hook,
	"_ZN5Caver18GameViewController6UpdateEf",
	void, (GameViewController *gvc, float dt)
) {
	g_gvc = gvc;
	if (gvc && gvc->GameState) {
		g_gameState = (GameState *)gvc->GameState;
	}
	if (orig_GVC_Update_Hook) {
		orig_GVC_Update_Hook(gvc, dt);
	}
	sre13_handle_dev_toggles(gvc);
	sre13_scene_shifter_tick();
}

G_DL_SYMBOL(
	GameViewController_Update,
	"_ZN5Caver18GameViewController6UpdateEf",
	void, (GameViewController *gvc, float dt)
);

G_DL_SYMBOL(
	GameViewController_LoadGameState,
	"_ZN5Caver18GameViewController13LoadGameStateEv",
	void, (GameViewController *gvc)
);

G_DL_SYMBOL(
	GameViewController_SaveGameState,
	"_ZN5Caver18GameViewController13SaveGameStateEb",
	void, (GameViewController *gvc, bool unknown)
);

G_DL_SYMBOL(
	GameViewController_GotoLevel,
	"_ZN5Caver18GameViewController9GotoLevelERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_",
	void, (GameViewController *gvc, String *level, String *spawn)
);

G_DL_SYMBOL(
	GameViewController_LoadView,
	"_ZN5Caver18GameViewController8LoadViewEv",
	void, (GameViewController *gvc)
);

G_DL_SYMBOL(
	GameViewController_ResetView,
	"_ZN5Caver18GameViewController9ResetViewEv",
	void, (GameViewController *gvc)
);

G_DL_SYMBOL(
	GameViewController_ResumeView,
	"_ZN5Caver18GameViewController10ResumeViewEv",
	void, (GameViewController *gvc)
);

G_DL_SYMBOL(
	GameViewController_AddItemToCharacter,
	"_ZN5Caver18GameViewController18AddItemToCharacterERKN5boost10shared_ptrINS_4ItemEEE",
	void, (GameViewController *gvc, void *shared_item)
);

G_DL_SYMBOL(
	GameViewController_RemoveItemFromCharacter,
	"_ZN5Caver18GameViewController23RemoveItemFromCharacterERKN5boost10shared_ptrINS_4ItemEEE",
	void, (GameViewController *gvc, void *shared_item)
);

G_DL_SYMBOL(
	GameState_AllNodesVisited,
	"_ZN5Caver9GameState15AllNodesVisitedEv",
	bool, (GameState *gs)
);

G_DL_SYMBOL(
	GameState_Clear,
	"_ZN5Caver9GameState5ClearEv",
	void, (GameState *gs)
);
