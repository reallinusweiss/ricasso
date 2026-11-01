#ifndef SRE13_CAMERACONTROLLER_H
#define SRE13_CAMERACONTROLLER_H

#include "../core/hook.h"
#include "../core/types.h"
#include "Camera.h"
#include "SceneObject.h"
#include "lua.h"

typedef struct CameraController {
	int flags;
	char _pad0[archSplit(0x0c, 0x0c)];
	Vector3 targetPos;
	float lerpFactor;
	Vector3 currentPos;
	float zoom;
	Vector3 focusPos;
	char _pad1[archSplit(0x14, 0x1c)];
	Camera *camera;
	void *cameraRef;
	char _pad2[archSplit(0x00, 0x08)];
	SceneObject *followObject;
	void *followObjectRef;
	Vector3 followOffset;
	char _pad3[archSplit(0x10, 0x18)];
	float rumble;
	char _pad4[archSplit(0x00, 0x04)];
} CameraController;

CameraController *cameraController_from_L(lua_State *L);
CameraController *cameraController_get(void);

/* Host-exposed camera controls & telemetry (exported symbols, written/read by
 * the host through srehost_get_symbol). Parity with libsre12's camera globals
 * plus sre13-only free-look / projection overrides. */
extern volatile void *g_sre_camera_ctrl;  /* live CameraController* every frame */
extern volatile float g_sre_hero_pos_x;   /* live hero position (host lighting) */
extern volatile float g_sre_hero_pos_y;
extern volatile float g_sre_hero_pos_z;
extern volatile float g_sre_cam_x;        /* live camera position */
extern volatile float g_sre_cam_y;
extern volatile float g_sre_cam_z;

extern int   g_sre_cam_active;            /* 1 = camera override on */
extern float g_sre_cam_off_x;             /* freecam offset (units) */
extern float g_sre_cam_off_y;
extern float g_sre_cam_off_z;
extern float g_sre_cam_aspect;            /* aspect ratio (host keeps updated) */
extern int   g_sre_cam_pov_mode;          /* 1 = hero first-person */
extern float g_sre_cam_pov_facing;        /* legacy ±1 facing (free-look fallback) */

/* SRE12 parity camera state (polled / driven by host & Lua) */
extern volatile float g_sre_cam_zoom;     /* zoom multiplier (0.01 - 50.0) */
extern volatile int   g_sre_cam_follow;   /* 1 = follow hero with offset, 0 = free */
extern volatile int   g_sre_cam_set_pending; /* 1 = host/Lua requested explicit position */
extern float g_sre_cam_up_x;              /* camera up vector */
extern float g_sre_cam_up_y;
extern float g_sre_cam_up_z;

extern float g_sre_cam_fov;               /* rad; 0 = preset/vanilla */
extern float g_sre_cam_near;              /* 0 = 50.0 */
extern float g_sre_cam_far;               /* 0 = 20000.0 */
extern float g_sre_cam_yaw;               /* free-look rad; all-zero → legacy facing */
extern float g_sre_cam_pitch;
extern float g_sre_cam_roll;

DL_SYMBOL_DECL(CameraController_Update, void, (CameraController *cc, float dt));
DL_SYMBOL_DECL(CameraController_FollowObject, void, (CameraController *cc, void *intrusive_object, Vector3 *offset));
DL_SYMBOL_DECL(CameraController_StopFollowing, void, (CameraController *cc));
DL_SYMBOL_DECL(CameraController_FocusAtPoint, void, (CameraController *cc, Vector3 *point, bool immediate));
DL_SYMBOL_DECL(CameraController_GotoTargetImmediately, void, (CameraController *cc));
DL_SYMBOL_DECL(CameraController_ResetFocus, void, (CameraController *cc));
DL_SYMBOL_DECL(CameraController_Rumble, void, (CameraController *cc));

#endif /* SRE13_CAMERACONTROLLER_H */
