#ifndef SRE13_CONSOLE_H
#define SRE13_CONSOLE_H

#include "lua.h"
#include <stdint.h>

#define CONSOLE_BUF_SIZE   4096
#define CONSOLE_PRINT_SIZE 8192

/* Host-visible shared memory variables for SwordfareGUI / ImGui terminal */
extern char         g_lua_console_buf[CONSOLE_BUF_SIZE];
extern char         g_lua_console_result[CONSOLE_BUF_SIZE];
extern char         g_lua_console_print_buf[CONSOLE_PRINT_SIZE];
extern volatile int g_lua_console_pending;
extern volatile int g_lua_console_status;

/* Host-visible diagnostics */
extern volatile uint64_t g_sre_console_runs;
extern volatile uint64_t g_sre_last_lua_state;   /* newest live game lua_State addr */

/* Service console commands every frame from the live game lua_State */
void sre13_console_frame_tick(float dt);

/* Reset the once-per-frame service guard (called by CaverShell::Update) */
void sre13_console_frame_guard_reset(void);

/* Store the frame dt (called by CaverShell::Update) for the console clock */
void sre13_console_set_frame_dt(float dt);

/* Capture the live game lua_State (called by sre_ProgramState_Execute / Resume) */
void sre13_console_set_root(struct lua_State* L);
void sre13_console_set_root_and_ps(struct lua_State* L, void* ps);

/* Legacy single-shot service point (kept for compatibility) */
void sre13_console_tick(lua_State* L);

#endif /* SRE13_CONSOLE_H */
