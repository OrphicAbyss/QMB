/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#include <math.h>
#include "quakedef.h"

void Sys_Error(char *error, ...);

vec3_t vec3_origin = {0, 0, 0};
int nanmask = 255 << 23;

/*-----------------------------------------------------------------*/

float degRadScalar;
double angleModScalar1;
double angleModScalar2;

void Math_Init() {
	degRadScalar = (float) (M_PI / 180.0);
	angleModScalar1 = (360.0 / 65536.0);
	angleModScalar2 = (65536.0 / 360.0);
}

void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal) {
	vec3_t n;

	float inv_denom = 1.0F / DotProduct(normal, normal);
	float d = DotProduct(normal, p) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

// assumes "src" is normalized
void PerpendicularVector(vec3_t dst, const vec3_t src) {
	float minelem = 1.0F;
	vec3_t tempvec;
	int pos = 0;

	// find the smallest magnitude axially aligned vector
	for (int i = 0; i < 3; i++) {
		if (fabs(src[i]) < minelem) {
			pos = i;
			minelem = fabs(src[i]);
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	// project the point onto the plane defined by src
	ProjectPointOnPlane(dst, tempvec, src);

	// normalize the result
	VectorNormalize(dst);
}

// LordHavoc: like AngleVectors, but taking a forward vector instead of angles, useful!
void VectorVectors(const vec3_t forward, vec3_t right, vec3_t up) {
	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];

	float d = DotProduct(forward, right);
	right[0] -= d * forward[0];
	right[1] -= d * forward[1];
	right[2] -= d * forward[2];
	VectorNormalize(right);
	CrossProduct(right, forward, up);
}

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees) {
	vec3_t vr, vu, vf;

	float angle = DEG2RAD(degrees);
	float c = cos(angle);
	float s = sin(angle);

	VectorCopy(dir, vf);
	VectorVectors(vf, vr, vu);

	float t0 = vr[0] * c + vu[0] * -s;
	float t1 = vr[0] * s + vu[0] * c;
	dst[0] = (t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0]) * point[0]
			+ (t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1]) * point[1]
			+ (t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2]) * point[2];

	t0 = vr[1] * c + vu[1] * -s;
	t1 = vr[1] * s + vu[1] * c;
	dst[1] = (t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0]) * point[0]
			+ (t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1]) * point[1]
			+ (t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2]) * point[2];

	t0 = vr[2] * c + vu[2] * -s;
	t1 = vr[2] * s + vu[2] * c;
	dst[2] = (t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0]) * point[0]
			+ (t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1]) * point[1]
			+ (t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2]) * point[2];
}

/*-----------------------------------------------------------------*/
float anglemod(float a) {
	a = angleModScalar1 * ((int) (a * angleModScalar2) & 65535); // Tomaz Speed
	return a;
}

/**
 * Returns 1, 2, or 1 + 2
 */
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, mplane_t *p) {
	float dist1 = 0.0f;
	float dist2 = 0.0f;
	int sides;

	// general case
	switch (p->signbits) {
		case 0:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			break;
		case 1:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			break;
		case 2:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;
		case 3:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;
		case 4:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;
		case 5:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;
		case 6:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			break;
		case 7:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			break;
		default:
			Sys_Error("BoxOnPlaneSide:  Bad signbits");
			break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

	return sides;
}

void AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
	float angle = DEG2RAD(angles[YAW]); // Tomaz Speed
	float sy = sinf(angle);
	float cy = cosf(angle);
	angle = DEG2RAD(angles[PITCH]); // Tomaz Speed
	float sp = sinf(angle);
	float cp = cosf(angle);
	angle = DEG2RAD(angles[ROLL]); // Tomaz Speed
	float sr = sinf(angle);
	float cr = cosf(angle);

	forward[0] = cp*cy;
	forward[1] = cp*sy;
	forward[2] = -sp;
	right[0] = (-1 * sr * sp * cy + -1 * cr*-sy);
	right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
	right[2] = -1 * sr*cp;
	up[0] = (cr * sp * cy + -sr*-sy);
	up[1] = (cr * sp * sy + -sr * cy);
	up[2] = cr*cp;
}

float VectorNormalize(vec3_t v) {
	float length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];

	if (length) {
		float ilength;
		length = sqrt(length); // FIXME
		ilength = 1 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

#ifndef WIN32
// min and max math functions (missing on linux?)

float min(float a, float b) {
	return a < b ? a : b;
}

float max(float a, float b) {
	return a > b ? a : b;
}
#endif
