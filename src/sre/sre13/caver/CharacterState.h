#ifndef SRE13_CHARACTERSTATE_H
#define SRE13_CHARACTERSTATE_H

#include "../core/hook.h"
#include "../core/stdstring.h"

typedef struct CharacterState {
	void *GameData;
	char _pad0[archSplit(0x0c, 0x18)];
	void *itemsBegin;
	void *itemsEnd;
	char _pad1[archSplit(0x08, 0x10)];
	void *skillsBegin;
	void *skillsEnd;
	char _pad2[archSplit(0x40, 0x50)];
	int level;
	int experience;
	int health;
	int mana;
	char _pad3[archSplit(0x10, 0x20)];
} CharacterState;

CharacterState *characterState_get(void);

DL_SYMBOL_DECL(CharacterState_ExperiencePointsRequiredForLevel, int, (CharacterState *cs, int level));

#endif /* SRE13_CHARACTERSTATE_H */
