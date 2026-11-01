#include "../caver/CameraController.h"
#include "../caver/Camera.h"
#include "../caver/GameSceneController.h"

/* Live CameraController pointer */
static CameraController *g_cc = NULL;

/* Live instance pointer, refreshed every frame by the Update hook. Host and
 * tools read this (via srehost_get_symbol) to reach the active camera safely.
 * Parity with libsre12's g_sre_camera_ctrl. */
volatile void *g_sre_camera_ctrl = 0;

/* Hero position telemetry — published every frame from the live hero object.
 * Read by the host for GLES2 character lighting and frame debug stats.
 * Parity with libsre12's g_sre_hero_pos_*. */
volatile float g_sre_hero_pos_x = 0.0f;
volatile float g_sre_hero_pos_y = 0.0f;
volatile float g_sre_hero_pos_z = 0.0f;

/* Camera position telemetry — published every frame from the live Camera. */
volatile float g_sre_cam_x = 0.0f;
volatile float g_sre_cam_y = 0.0f;
volatile float g_sre_cam_z = 0.0f;

/* Host camera override variables (read/written by main.cpp & camera_override.cpp) */
int   g_sre_cam_active = 0;
float g_sre_cam_off_x = 0.0f;
float g_sre_cam_off_y = 0.0f;
float g_sre_cam_off_z = 0.0f;
float g_sre_cam_aspect = 1.0f;
int   g_sre_cam_pov_mode = 0;
float g_sre_cam_pov_facing = 1.0f;

/* SRE12 parity camera state (polled / driven by host & Lua) */
volatile float g_sre_cam_zoom = 1.0f;
volatile int   g_sre_cam_follow = 1;  /* 1 = follow hero with offset, 0 = free */
volatile int   g_sre_cam_set_pending = 0;
float g_sre_cam_up_x = 0.0f;
float g_sre_cam_up_y = 1.0f;
float g_sre_cam_up_z = 0.0f;

/* ── Beyond sre12: host-controllable projection & free-look ────────────────
 * g_sre_cam_fov     — vertical FOV override in radians (0.0 = keep preset/vanilla)
 * g_sre_cam_near    — near plane override (0.0 = vanilla 50.0)
 * g_sre_cam_far     — far plane override (0.0 = vanilla 20000.0)
 * g_sre_cam_yaw     — free-look yaw (radians); 0 keeps legacy ±45° facing
 * g_sre_cam_pitch   — free-look pitch (radians)
 * g_sre_cam_roll    — free-look roll (radians)
 */
float g_sre_cam_fov = 0.0f;
float g_sre_cam_near = 0.0f;
float g_sre_cam_far = 0.0f;
float g_sre_cam_yaw = 0.0f;
float g_sre_cam_pitch = 0.0f;
float g_sre_cam_roll = 0.0f;

CameraController *cameraController_from_L(lua_State *L) {
	lua_getglobal(L, "cameraController");
	if (!lua_islightuserdata(L, -1)) return NULL;
	const void *cc = lua_topointer(L, -1);
	lua_pop(L, 1);
	return (CameraController *)cc;
}

CameraController *cameraController_get(void) {
	return g_cc;
}

/* Detect NaN or Inf without pulling in libc math dependencies (parity with
 * libsre12's sre_float_is_bad — a bad camera state cascades into NaN view
 * matrices and eventually a renderer crash). */
static inline int sre_float_is_bad(float x) {
	return (x != x) || (x > 1e30f) || (x < -1e30f);
}

/* ── Minimal self-contained sin/cos (no libm — sre13 builds with -nostdlib) ──
 * Range-reduced Taylor approximation, ~1e-4 accuracy on [-2π, 2π]. More than
 * enough for camera orientation quaternions. */
static float sre_sin_f(float x) {
	/* Reduce to [-π, π] */
	while (x >  3.14159265f) x -= 6.28318531f;
	while (x < -3.14159265f) x += 6.28318531f;
	float sign = 1.0f;
	if (x < 0.0f) { sign = -1.0f; x = -x; }
	/* Taylor around 0, good on [0, π] */
	float x2 = x * x;
	float poly = x * (1.0f
		- x2 / 6.0f
		+ x2 * x2 / 120.0f
		- x2 * x2 * x2 / 5040.0f
		+ x2 * x2 * x2 * x2 / 362880.0f);
	return sign * poly;
}

static float sre_cos_f(float x) {
	return sre_sin_f(x + 1.57079633f);
}

/* Build a quaternion from Euler yaw/pitch/roll (radians). Same convention the
 * engine uses for its own camera orientations: yaw around +Y, then pitch
 * around the local X axis, then roll around the local Z axis. */
static void sre_quat_from_euler(float yaw, float pitch, float roll, Quaternion *out) {
	float cy = sre_cos_f(yaw * 0.5f),  sy = sre_sin_f(yaw * 0.5f);
	float cp = sre_cos_f(pitch * 0.5f), sp = sre_sin_f(pitch * 0.5f);
	float cr = sre_cos_f(roll * 0.5f), sr = sre_sin_f(roll * 0.5f);

	out->w = cr * cp * cy + sr * sp * sy;
	out->x = cr * sp * cy + sr * cp * sy;
	out->y = cr * cp * sy - sr * sp * cy;
	out->z = sr * cp * cy - cr * sp * sy;
}

HOOK_SYMBOL(
	Update,
	"_ZN5Caver16CameraController6UpdateEf",
	void, (CameraController *cc, float dt)
) {
	g_cc = cc;
	g_sre_camera_ctrl = cc;

	/* Publish hero position telemetry for the host (lighting + debug). */
	{
		GameSceneController *gsc = gsc_get();
		if (gsc && gsc->hero) {
			g_sre_hero_pos_x = gsc->hero->Position.x;
			g_sre_hero_pos_y = gsc->hero->Position.y;
			g_sre_hero_pos_z = gsc->hero->Position.z;
		}
	}

	if (orig_Update) {
		orig_Update(cc, dt);
	}

	/* Sanitize CameraController internal state against NaN/Inf (e.g. from
	 * degenerate FocusAtShape or collinear LookAt in cutscenes). */
	if (cc) {
		float *c_ctrl_floats = (float*)&cc->targetPos;
		for (int i = 0; i < 12; i++) { /* targetPos..focusPos + lerpFactor/zoom */
			if (sre_float_is_bad(c_ctrl_floats[i])) {
				c_ctrl_floats[i] = 0.0f;
			}
		}
	}

	/* Sanitize the Camera object's position, quaternion, and view matrix */
	if (cc && cc->camera) {
		Camera *cam = cc->camera;
		int bad = 0;
		float *cpos = (float*)&cam->position;
		for (int i = 0; i < 3; i++) {
			if (sre_float_is_bad(cpos[i])) { bad = 1; cpos[i] = 0.0f; }
		}
		float *cquat = (float*)&cam->rotation;
		for (int i = 0; i < 4; i++) {
			if (sre_float_is_bad(cquat[i])) { bad = 1; }
		}
		if (bad) {
			cquat[0] = 0.0f; cquat[1] = 0.0f; cquat[2] = 0.0f; cquat[3] = 1.0f;
		}
		for (int i = 0; i < 16; i++) {
			if (sre_float_is_bad(cam->view.m[i])) {
				bad = 1;
				break;
			}
		}
		if (bad) {
			Camera_EvaluateViewMatrix(cam);
		}

		/* Camera position telemetry */
		g_sre_cam_x = cam->position.x;
		g_sre_cam_y = cam->position.y;
		g_sre_cam_z = cam->position.z;
	}

	static int s_was_active = 0;
	static float s_vanilla_fov = 0.34906584f;
	static float s_vanilla_aspect = 1.0f;
	static float s_vanilla_near = 50.0f;
	static float s_vanilla_far = 20000.0f;
	static float s_vanilla_offset_x = 0.0f;
	static float s_vanilla_offset_y = 0.0f;
	static float s_vanilla_offset_z = 1000.0f;
	static float s_vanilla_up_x = 0.0f;
	static float s_vanilla_up_y = 1.0f;
	static float s_vanilla_up_z = 0.0f;
	static float s_vanilla_qx = 0.0f;
	static float s_vanilla_qy = 0.0f;
	static float s_vanilla_qz = 0.0f;
	static float s_vanilla_qw = 1.0f;
	static Vector3 s_vanilla_pos = { 0, 0, 0 };
	static Quaternion s_vanilla_rot = { 0, 0, 0, 1 };

	if (g_sre_cam_set_pending && cc && cc->camera) {
		Camera *cam = cc->camera;
		cam->position.x = g_sre_cam_x;
		cam->position.y = g_sre_cam_y;
		cam->position.z = g_sre_cam_z;
		Camera_EvaluateViewMatrix(cam);
		g_sre_cam_set_pending = 0;
	}

	if (g_sre_cam_active && cc && cc->camera) {
		Camera *cam = cc->camera;
		if (!s_was_active) {
			s_vanilla_fov = cam->fov;
			s_vanilla_aspect = cam->aspect;
			s_vanilla_near = cam->nearPlane;
			s_vanilla_far = cam->farPlane;
			s_vanilla_offset_x = *(float*)((char*)cc + 0x04);
			s_vanilla_offset_y = *(float*)((char*)cc + 0x08);
			s_vanilla_offset_z = *(float*)((char*)cc + 0x0c);
			s_vanilla_up_x = *(float*)((char*)cc + 0x48);
			s_vanilla_up_y = *(float*)((char*)cc + 0x4c);
			s_vanilla_up_z = *(float*)((char*)cc + 0x50);
			s_vanilla_pos = cam->position;
			s_vanilla_rot = cam->rotation;
			s_vanilla_qx = cam->rotation.x;
			s_vanilla_qy = cam->rotation.y;
			s_vanilla_qz = cam->rotation.z;
			s_vanilla_qw = cam->rotation.w;
		}
		s_was_active = 1;

		float near_plane = (g_sre_cam_near > 0.0f) ? g_sre_cam_near : 50.0f;
		float far_plane  = (g_sre_cam_far  > 0.0f) ? g_sre_cam_far  : 20000.0f;

		if (g_sre_cam_pov_mode) {
			GameSceneController *gsc = gsc_get();
			float hx = 0.0f, hy = 0.0f, hz = 0.0f;
			if (gsc && gsc->hero) {
				hx = gsc->hero->Position.x;
				hy = gsc->hero->Position.y;
				hz = gsc->hero->Position.z;
			}
			cam->position.x = hx;
			cam->position.y = hy + 75.0f; /* Eye level */
			cam->position.z = hz;

			/* Free-look: yaw/pitch/roll give full 3-axis orientation. When the
			 * host leaves yaw at 0 (the default), fall back to the legacy
			 * ±45° left/right facing so existing behavior is unchanged. */
			if (g_sre_cam_yaw != 0.0f || g_sre_cam_pitch != 0.0f || g_sre_cam_roll != 0.0f) {
				sre_quat_from_euler(g_sre_cam_yaw, g_sre_cam_pitch, g_sre_cam_roll, &cam->rotation);
			} else {
				cam->rotation.x = 0.0f;
				cam->rotation.y = (g_sre_cam_pov_facing > 0.0f) ? -0.70710678f : 0.70710678f;
				cam->rotation.z = 0.0f;
				cam->rotation.w = 0.70710678f;
			}

			float fov = (g_sre_cam_fov > 0.0f) ? g_sre_cam_fov : 1.22173f;
			Camera_SetPerspectiveProjection(cam, fov, g_sre_cam_aspect, near_plane, far_plane);
		} else if (!g_sre_cam_follow) {
			/* Free camera: completely detached from hero tracking */
			cam->position.x = g_sre_cam_off_x;
			cam->position.y = g_sre_cam_off_y;
			cam->position.z = g_sre_cam_off_z;

			if (g_sre_cam_yaw != 0.0f || g_sre_cam_pitch != 0.0f || g_sre_cam_roll != 0.0f) {
				sre_quat_from_euler(g_sre_cam_yaw, g_sre_cam_pitch, g_sre_cam_roll, &cam->rotation);
			}

			float fov = (g_sre_cam_fov > 0.0f) ? g_sre_cam_fov : 0.78539816f;
			Camera_SetPerspectiveProjection(cam, fov, g_sre_cam_aspect, near_plane, far_plane);
		} else {
			/* SRE12 parity + wide range: drives CameraController's native offset
			 * (+0x04/0x08/0x0c) & up-vector (+0x48/0x4c/0x50), preserving engine
			 * tracking and parallax smoothness without jitter. */
			float base_zoom = (g_sre_cam_zoom > 0.0f) ? g_sre_cam_zoom : 1.0f;
			float zoom = base_zoom * (1.0f + g_sre_cam_off_z / 600.0f);
			if (zoom < 0.01f) zoom = 0.01f;
			if (zoom > 50.0f) zoom = 50.0f;

			float offset_x = zoom * g_sre_cam_off_x;
			float offset_y = zoom * (300.0f + g_sre_cam_off_y);
			float offset_z = zoom * 600.0f;

			*(float*)((char*)cc + 0x04) = offset_x;
			*(float*)((char*)cc + 0x08) = offset_y;
			*(float*)((char*)cc + 0x0c) = offset_z;

			*(float*)((char*)cc + 0x48) = g_sre_cam_up_x;
			*(float*)((char*)cc + 0x4c) = g_sre_cam_up_y;
			*(float*)((char*)cc + 0x50) = g_sre_cam_up_z;

			if (g_sre_cam_yaw != 0.0f || g_sre_cam_pitch != 0.0f || g_sre_cam_roll != 0.0f) {
				sre_quat_from_euler(g_sre_cam_yaw, g_sre_cam_pitch, g_sre_cam_roll, &cam->rotation);
			}

			float fov = (g_sre_cam_fov > 0.0f) ? g_sre_cam_fov : 0.78539816f;
			Camera_SetPerspectiveProjection(cam, fov, g_sre_cam_aspect, near_plane, far_plane);
		}
		Camera_EvaluateViewMatrix(cam);
	} else if (s_was_active) {
		s_was_active = 0;
		if (cc && cc->camera) {
			Camera *cam = cc->camera;
			*(float*)((char*)cc + 0x04) = s_vanilla_offset_x;
			*(float*)((char*)cc + 0x08) = s_vanilla_offset_y;
			*(float*)((char*)cc + 0x0c) = s_vanilla_offset_z;

			*(float*)((char*)cc + 0x48) = s_vanilla_up_x;
			*(float*)((char*)cc + 0x4c) = s_vanilla_up_y;
			*(float*)((char*)cc + 0x50) = s_vanilla_up_z;

			cam->position = s_vanilla_pos;
			cam->rotation.x = s_vanilla_qx;
			cam->rotation.y = s_vanilla_qy;
			cam->rotation.z = s_vanilla_qz;
			cam->rotation.w = s_vanilla_qw;

			Camera_SetPerspectiveProjection(cam, s_vanilla_fov, s_vanilla_aspect, s_vanilla_near, s_vanilla_far);
			Camera_EvaluateViewMatrix(cam);
		}
	}
}

/* Frustum culling relaxation when freecam is active */
HOOK_SYMBOL(
	SceneGrid_UpdateVisibleAreasWithCamera,
	"_ZN5Caver9SceneGrid28UpdateVisibleAreasWithCameraEPNS_6CameraE",
	void, (void *self, void *camera)
) {
	if (!self || !camera) return;
	uint64_t self_addr = (uint64_t)self;
	uint64_t cam_addr  = (uint64_t)camera;
	if ((self_addr & 7) || self_addr < 0x10000ULL || self_addr >= 0x0000800000000000ULL) return;
	if ((cam_addr  & 7) || cam_addr  < 0x10000ULL || cam_addr  >= 0x0000800000000000ULL) return;

	if (orig_SceneGrid_UpdateVisibleAreasWithCamera) {
		orig_SceneGrid_UpdateVisibleAreasWithCamera(self, camera);
	}

	if (g_sre_cam_active) {
		int layer_count = *(int*)self;
		if (layer_count <= 0 || layer_count > 32) return;

		char **array = *(char***)((char*)self + 8);
		if (!array) return;

		for (int i = 0; i < layer_count; i++) {
			char *layer = array[i * 2];
			if (!layer) continue;
			uint64_t layer_addr = (uint64_t)layer;
			if (layer_addr < 0x10000ULL || layer_addr >= 0x0000800000000000ULL) continue;
			*(float*)(layer + 0x38) = -1000000.0f; /* x */
			*(float*)(layer + 0x3c) = -1000000.0f; /* y */
			*(float*)(layer + 0x40) =  2000000.0f; /* width */
			*(float*)(layer + 0x44) =  2000000.0f; /* height */
		}
	}
}

G_DL_SYMBOL(
	CameraController_Update,
	"_ZN5Caver16CameraController6UpdateEf",
	void, (CameraController *cc, float dt)
);

G_DL_SYMBOL(
	CameraController_FollowObject,
	"_ZN5Caver16CameraController12FollowObjectERKN5boost13intrusive_ptrINS_11SceneObjectEEERKNS_7Vector3E",
	void, (CameraController *cc, void *intrusive_object, Vector3 *offset)
);

G_DL_SYMBOL(
	CameraController_StopFollowing,
	"_ZN5Caver16CameraController13StopFollowingEv",
	void, (CameraController *cc)
);

G_DL_SYMBOL(
	CameraController_FocusAtPoint,
	"_ZN5Caver16CameraController12FocusAtPointERKNS_7Vector3Eb",
	void, (CameraController *cc, Vector3 *point, bool immediate)
);

G_DL_SYMBOL(
	CameraController_GotoTargetImmediately,
	"_ZN5Caver16CameraController21GotoTargetImmediatelyEv",
	void, (CameraController *cc)
);

G_DL_SYMBOL(
	CameraController_ResetFocus,
	"_ZN5Caver16CameraController10ResetFocusEv",
	void, (CameraController *cc)
);

G_DL_SYMBOL(
	CameraController_Rumble,
	"_ZN5Caver16CameraController6RumbleEv",
	void, (CameraController *cc)
);