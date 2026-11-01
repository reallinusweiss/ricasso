#ifndef SRE13_SCENE_SHIFTER_H
#define SRE13_SCENE_SHIFTER_H

#include <stdint.h>
#include <stddef.h>
#include "lua.h"

#define SRE_SCENE_MAX 256
#define SRE_SCENE_NAME_LEN 128

/* Host-visible shared memory variables for SwordfareGUI */
extern int          g_sre_scene_list_count;
extern char         g_sre_scene_list[SRE_SCENE_MAX][SRE_SCENE_NAME_LEN];
extern volatile int g_sre_scene_shift_pending;
extern char         g_sre_scene_shift_target[SRE_SCENE_NAME_LEN];
extern char         g_sre_scene_shift_spawn[SRE_SCENE_NAME_LEN];
extern char         g_sre_scene_shift_last_error[256];
extern volatile int g_sre_scene_shift_active;
extern char         g_sre_current_scene_name[SRE_SCENE_NAME_LEN];

/* Public API */
void* sre13_get_active_gvc(void);
int   sre13_scene_shifter_shift(const char* level, const char* spawn, int forced);
void  sre13_scene_shifter_tick(void);
void  sre_scene_shifter_tick(void);
void  sre_scene_shifter_scan_scenes(void);
void  sre13_register_scene_shifter_lua(lua_State* L);

#endif /* SRE13_SCENE_SHIFTER_H */
