/*
 * ProgramState.c — ProgramState time-scale API + Lua error logging.
 *
 * Ported from the lawncher reference (`core/32patch.c`) — the ARM32
 * LoadIntoState text patch from that file is intentionally NOT ported:
 * it is wrapped in `#ifdef __arm__` and sre13 is aarch64-only, so it would
 * compile to dead code. This file keeps the portable half:
 *
 *   1. ps_setTimeScaleEnabled / ps_isTimeScaleEnabled / ps_remove — a
 *      per-ProgramState time-scale toggle registry (the desktop host can
 *      resolve these via get_symbol_vaddr and pause/dampen a running script
 *      without killing its lua_State).
 *   2. ProgramState::Update hook — when a ProgramState aborts, logs the
 *      pending Lua error string (lua_gettop/lua_isstring/lua_tostring on the
 *      engine's lua_State — the same vendored-Lua pattern sre13_scene_shifter
 *      and the extras stubs already use).
 *   3. ProgramState::~ProgramState — cleans the registry entry.
 *
 * Faithful to the lawncher original except: sre13-style guarded orig_ calls,
 * ../core includes, and exported (default-visibility) ps_* functions declared
 * in sre13.h for the host.
 */

#define LOG_TAG "ProgramState"
#include <stdbool.h>

#include "../core/hook.h"
#include "../core/log.h"
#include "../core/map.h"
#include "lua.h"

/* The aarch64 cross sysroot has no pthread.h, and the guest is built
 * -nostdlib, so guard the registry with a tiny GCC-atomic spinlock instead
 * of pthread_mutex (the lawncher's pthread version does not port 1:1).
 * Critical sections are a handful of instructions. */
static volatile int g_ps_lock = 0;

static void ps_lock_acquire(void) {
	while (__atomic_test_and_set(&g_ps_lock, __ATOMIC_ACQUIRE)) { }
}

static void ps_lock_release(void) {
	__atomic_clear(&g_ps_lock, __ATOMIC_RELEASE);
}

static Map *g_ps_map = NULL;

void ps_setTimeScaleEnabled(void *ps, bool enabled) {
	if (!ps) return;
	ps_lock_acquire();
	if (!g_ps_map) {
		g_ps_map = Map_Create(1);
	}
	Map_Set(g_ps_map, ps, (void *)(uintptr_t)enabled);
	ps_lock_release();
}

bool ps_isTimeScaleEnabled(void *ps) {
	if (!ps) return false;
	ps_lock_acquire();
	void *val = Map_Get(g_ps_map, ps);
	ps_lock_release();
	return (bool)(uintptr_t)val;
}

void ps_remove(void *ps) {
	if (!ps) return;
	ps_lock_acquire();
	if (g_ps_map) {
		Map_Remove(g_ps_map, ps);
	}
	ps_lock_release();
}

HOOK_SYMBOL(
	ProgramState_Update,
	"_ZN5Caver12ProgramState6UpdateEf",
	void, (void *ps, float dt)
) {
	if (!ps) {
		LOGE("ProgramState is NULL?");
		return;
	}

	lua_State *L = *$(lua_State *, ps, 0x0, 0x0);
	char aborted = *$(char, ps, 0x33, 0x5b);
	if (orig_ProgramState_Update) {
		orig_ProgramState_Update(ps, dt);
	}

	aborted = *$(char, ps, 0x33, 0x5b);
	if (aborted && L) {
		if (lua_gettop(L) > 0 && lua_isstring(L, -1)) {
			LOGE("%p encountered error: %s", ps, lua_tostring(L, -1));
		}
	}
}

HOOK_SYMBOL(
	ProgramState_DTor,
	"_ZN5Caver12ProgramStateD1Ev",
	void, (void *ps)
) {
	ps_remove(ps);
	if (orig_ProgramState_DTor) {
		orig_ProgramState_DTor(ps);
	}
}