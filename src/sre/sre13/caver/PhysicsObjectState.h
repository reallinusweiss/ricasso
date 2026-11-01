#ifndef SRE13_PHYSICSOBJECTSTATE_H
#define SRE13_PHYSICSOBJECTSTATE_H

#include "../core/hook.h"
#include "SceneObject.h"

/* Ported from the lawncher reference (lawncher_reference_for_sre13) — the
 * physics state attached to a moving object (hero, monsters). Recovered struct
 * with verified 32/64-bit offsets; the Update hook is intentionally passthrough
 * (telemetry was commented out upstream). */
typedef struct PhysicsObjectState {
	SceneObject* owner;
	float airTime; // Increments when you walk off a ledge
	float airTime2;
	float groundNormalX;
	float groundNormalY;
	bool onSteepGround;
	char _pad0[archSplit(0x3, 0x7)];
	void* groundObject;
	char _pad1[archSplit(0x8, 0x8)];
	float groundFriction;
	float maxSpeedFactor;
	float accel;
	float friction;
	char _pad2[archSplit(0x30, 0x30)];
	float maxSpeed;
	bool enabled;
	char _pad3[archSplit(0x3, 0x3)];
	float forceY;
	bool flag;
	char _pad4[archSplit(0x3, 0x3)];
} PhysicsObjectState; // sizeof(0x74, 0x80)

#endif /* SRE13_PHYSICSOBJECTSTATE_H */