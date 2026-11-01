#ifndef SRE13_TYPES_H
#define SRE13_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__aarch64__)
#define archSplit(arm_val, arm64_val) (arm64_val)
#elif defined(__arm__)
#define archSplit(arm_val, arm64_val) (arm_val)
#else
#define archSplit(arm_val, arm64_val) (arm64_val)
#endif

typedef struct Vector3 {
	float x;
	float y;
	float z;
} Vector3;

typedef struct Vector2 {
	float x;
	float y;
} Vector2;

typedef struct Rectangle {
	float x;
	float y;
	float width;
	float height;
} Rectangle;

typedef struct FloatColor {
	float r;
	float g;
	float b;
	float a;
} FloatColor;

typedef struct Quaternion {
	float x;
	float y;
	float z;
	float w;
} Quaternion;

typedef struct Matrix4 {
	float m[16];
} Matrix4;

#endif /* SRE13_TYPES_H */
