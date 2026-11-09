#ifndef SRE13_GAMESCENECONTROLLER_H
#define SRE13_GAMESCENECONTROLLER_H

#include "../core/hook.h"
#include "SceneObject.h"
#include "CharacterState.h"
#include "lua.h"

typedef struct GameSceneController {
	void *vtable;
	char _pad0[archSplit(0x04, 0x08)];
	CharacterState *CharacterState;
	/* 64-bit: one pointer of padding, not two. Upstream's IDA notes
	 * (docs/sre13/GameSceneController.md) place Scene* at +0x20 and the hero
	 * SceneObject* at +0xD8, followed by the CharController, Entity, Health and
	 * Mana component pointers at +0xE0..+0xF8. With 0x10 here every field from
	 * Scene on was read 8 bytes too late, so gsc->hero returned the
	 * CharControllerComponent and the hero position telemetry was garbage. */
	char _pad1[archSplit(0x08, 0x08)];
	void *Scene;
	char _pad2[archSplit(0x90, 0xb0)];
	SceneObject *hero;
	void *CharControllerComponent;
	void *EntityComponent;
	void *HealthComponent;
	void *ManaComponent;
	void *HeroEquipmentManager;
	char _pad3[archSplit(0x24, 0x44)];
	Vector3 spawnPosition;
	char _pad4[archSplit(0x20, 0x30)];
} GameSceneController;

#if defined(__aarch64__)
/* Offsets verified for Swordigo 1.4.13 ARM64; the host's mod tools use the same
 * values (kGscOff* in main.cpp). */
_Static_assert(offsetof(GameSceneController, Scene) == 0x20, "GameSceneController.Scene");
_Static_assert(offsetof(GameSceneController, hero) == 0xD8, "GameSceneController.hero");
_Static_assert(offsetof(GameSceneController, HealthComponent) == 0xF0, "GameSceneController.HealthComponent");
_Static_assert(offsetof(GameSceneController, ManaComponent) == 0xF8, "GameSceneController.ManaComponent");
#endif

GameSceneController *gsc_from_L(lua_State *L);
GameSceneController *gsc_get(void);

DL_SYMBOL_DECL(GameSceneController_Update, void, (GameSceneController *gsc, float dt));
DL_SYMBOL_DECL(GameSceneController_SpawnHeroAt, void, (GameSceneController *gsc, String *id));
DL_SYMBOL_DECL(GameSceneController_UpdateTarget, void, (GameSceneController *gsc));
DL_SYMBOL_DECL(GameSceneController_EquipItem, void, (GameSceneController *gsc, void *shared_item));
DL_SYMBOL_DECL(GameSceneController_UnequipArmor, void, (GameSceneController *gsc));
DL_SYMBOL_DECL(GameSceneController_BeginCasting, void, (GameSceneController *gsc, void *shared_skill));
DL_SYMBOL_DECL(GameSceneController_CanCastSkill, bool, (GameSceneController *gsc, void *shared_skill));
DL_SYMBOL_DECL(GameSceneController_ConsumeItem, void, (GameSceneController *gsc, void *shared_item));
DL_SYMBOL_DECL(GameSceneController_ApplyLevelUp, void, (GameSceneController *gsc, int a, int b, int c));

#endif /* SRE13_GAMESCENECONTROLLER_H */
