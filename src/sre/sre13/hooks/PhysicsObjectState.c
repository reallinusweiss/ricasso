#include "../caver/PhysicsObjectState.h"

/* Passthrough Update hook (the lawncher's airtime telemetry is commented out —
 * kept for parity so the symbol hook list matches between the trees). */
HOOK_SYMBOL(
	Update,
	"_ZN5Caver18PhysicsObjectState6UpdateEf",
	void, (PhysicsObjectState *state, float dt)
) {
	return orig_Update(state, dt);
}