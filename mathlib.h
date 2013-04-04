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
// mathlib.h

struct	mplane_s;

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

#ifndef M_PI
#define M_PI		3.141592653589793
#endif

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)
#define DEG2RAD(a) (a * degRadScalar)

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide(emins, emaxs, p))

extern	int		nanmask;
extern	float	degRadScalar;
extern	double	angleModScalar1;
extern	double	angleModScalar2;
extern	vec3_t	vec3_origin;

void	VectorNegate(const vec3_t in, vec3_t out);
void	VectorSet(vec3_t vec, const float x, const float y, const float z);
void	VectorClear(vec3_t vec);
double	DotProduct(const vec3_t a, const vec3_t b);
void	VectorSubtract(const vec3_t a, const vec3_t b, vec3_t out);
void	VectorAdd(const vec3_t a, const vec3_t b, vec3_t out);
void	VectorCopy(const vec3_t in, vec3_t out);
void	VectorCrossProduct(const vec3_t a, const vec3_t b, vec3_t out);
float	VectorNormalize(vec3_t v);
void	VectorNormalize2(const vec3_t in, vec3_t out);
void	VectorNormalizeDouble(vec3_t v);
float	VectorDistance(const vec3_t a, const vec3_t b);
float	VectorDistance2(const vec3_t a, const vec3_t b);
float	VectorLength(const vec3_t a);
void	VectorScale(const vec3_t in, const float scale, vec3_t out);
bool	VectorCompare(const vec3_t a, const vec3_t b);
void	VectorMA(const vec3_t a, const float scale, const vec3_t b, vec3_t out);

int		BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);
void	Math_Init();
void	AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void	VectorVectors(const vec3_t forward, vec3_t right, vec3_t up);
void	RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
float	VectorNormalize(vec3_t v);
float	anglemod(float a);

// LordHavoc: like AngleVectors, but taking a forward vector instead of angles, useful!
void VectorVectors(const vec3_t forward, vec3_t right, vec3_t up);

#ifndef WIN32
float min(float a, float b);
float max(float a, float b);
#endif
