#include "../../caver/Component.h"
#include "../../caver/Component/CharControllerComponent.h"
#include "../../core/hook.h"
#include "../../core/log.h"
#include "lua.h"
#include "../../caver/GameSceneController.h"

static CharControllerComponent *g_cc = NULL;

CharControllerComponent *charControllerComponent_get(void) {
	return g_cc;
}

/* Fetch the hero's CharControllerComponent from the live GameSceneController —
 * the same lookup the in-game skill/cast logic uses (component by name). */
CharControllerComponent *charControllerComponent_from_L(lua_State *L) {
	GameSceneController *gsc = gsc_from_L(L);
	if (!gsc || !gsc->hero) return NULL;
	SceneObject *hero = gsc->hero;
	void *cc = component_fetch(hero, "CharControllerComponent");
	return (CharControllerComponent *)cc;
}

HOOK_SYMBOL(
	Update,
	"_ZN5Caver23CharControllerComponent6UpdateEf",
	void, (CharControllerComponent *cc, float dt)
) {
	/* g_cc refreshed here every frame; callers (host tools, Lua) read it via
	 * charControllerComponent_get() instead of resolving per access. */
	g_cc = cc;
	return orig_Update(cc, dt);
}