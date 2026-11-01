#ifndef SRE13_ENTITYCOMPONENT_H
#define SRE13_ENTITYCOMPONENT_H

#include "../../core/hook.h"
#include "../PhysicsObjectState.h"

/* Ported from the lawncher reference — the base entity component (any living /
 * moving thing in a scene). Embeds the PhysicsObjectState inline; the "unsure
 * about this..." comment on `physics` is upstream's, kept as-is. */
typedef struct EntityComponent {
	void* vtable;
	char _pad0[archSplit(0x34, 0x60)];
	int facingDirection;
	char _pad1[archSplit(0x0, 0x4)];
	PhysicsObjectState physics; // unsure about this...
	char _pad2[archSplit(0x2c, 0x38)];
} EntityComponent; // sizeof(0xdc, 0x128)

#endif /* SRE13_ENTITYCOMPONENT_H */