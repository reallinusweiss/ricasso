#ifndef SRE13_GAMESTATE_H
#define SRE13_GAMESTATE_H

#include "../core/hook.h"
#include "CharacterState.h"

typedef struct GameState {
	void *GameData;
	void *GameDataRef;
	CharacterState CharacterState;
	char _pad0[archSplit(0x20, 0x30)];
	void *nodesBegin;
	void *nodesEnd;
	char _pad1[archSplit(0x10, 0x20)];
	void *StateProperties;
	char _pad2[archSplit(0x20, 0x40)];
} GameState;

GameState *gameState_get(void);

DL_SYMBOL_DECL(GameState_AllNodesVisited, bool, (GameState *gs));
DL_SYMBOL_DECL(GameState_Clear, void, (GameState *gs));

#endif /* SRE13_GAMESTATE_H */
