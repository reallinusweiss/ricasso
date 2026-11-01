#include "../caver/CaverShell.h"
#include "../sre13_console.h"

static CaverShell *g_shell = NULL;

extern uint64_t g_swordigo_base;

CaverShell *caverShell_get(void) {
	if (g_shell) return g_shell;
	if (!g_swordigo_base) return NULL;
	void **slot = (void **)(g_swordigo_base + 0x00651700ULL);
	void *shell = *slot;
	if (!shell || (uintptr_t)shell < 0x20000000ULL || (uintptr_t)shell >= 0xE0000000ULL) return NULL;
	return (CaverShell *)shell;
}

/* Original CaverShell::Update — filled by TrampolineMgr relay (main.cpp
 * relays_13 + sym_hooks_13). NOTE: HOOK_SYMBOL constructors never run for
 * libsre13 (the custom ELF loader only executes the game binary's
 * .init_array), so this hook MUST go through the hook-table path.
 *
 * This hook does NOT service the console directly — game C++ APIs called
 * from console scripts (Camera.*, Scene.*, etc.) require the valid scene
 * context that only exists during ProgramState execution, not at the shell
 * tail. It only resets the per-frame service guard so the console is
 * serviced exactly once per frame from the ProgramState hooks below. */
uint64_t g_orig_CaverShell_Update = 0;

void sre13_CaverShell_Update(void *shell, float dt) {
	g_shell = shell;
	sre13_console_set_frame_dt(dt);
	sre13_console_frame_guard_reset();
	if (g_orig_CaverShell_Update) {
		typedef void (*pfn_shell_update)(void *, float);
		((pfn_shell_update)g_orig_CaverShell_Update)(shell, dt);
	}
	sre13_console_frame_tick(dt);
}

G_DL_SYMBOL(
	CaverShell_SuspendApplication,
	"_ZN5Caver10CaverShell18SuspendApplicationEb",
	void, (CaverShell *shell, bool unknown)
);

G_DL_SYMBOL(
	CaverShell_ResumeApplication,
	"_ZN5Caver10CaverShell17ResumeApplicationEv",
	void, (CaverShell *shell)
);

G_DL_SYMBOL(
	CaverShell_QuitApplication,
	"_ZN5Caver10CaverShell15QuitApplicationEv",
	void, (CaverShell *shell)
);

G_DL_SYMBOL(
	CaverShell_InitApplication,
	"_ZN5Caver10CaverShell15InitApplicationEv",
	void, (CaverShell *shell)
);
