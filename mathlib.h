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
// Tomaz - Whole File Redone

struct	mplane_s;

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

#ifndef M_PI
#define M_PI		3.141592653589793
#endif

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)
#define Length(v) (sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]))
#define DEG2RAD(a) (a * degRadScalar)

#define VectorNegate(a,b) ((b)[0]=-((a)[0]),(b)[1]=-((a)[1]),(b)[2]=-((a)[2]))
#define VectorSet(a,b,c,d) ((a)[0]=(b),(a)[1]=(c),(a)[2]=(d))
#define VectorClear(a) ((a)[0]=(a)[1]=(a)[2]=0)
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VectorSubtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c) ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define CrossProduct(a,b,c) ((c)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1],(c)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2],(c)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0])
#define VectorNormalize2(v,dest) {float ilength = 1.0f / (float) sqrt(DotProduct(v,v));dest[0] = v[0] * ilength;dest[1] = v[1] * ilength;dest[2] = v[2] * ilength;}
#define VectorNormalizeDouble(v) {double ilength = 1.0 / sqrt(DotProduct(v,v));v[0] *= ilength;v[1] *= ilength;v[2] *= ilength;}
#define VectorDistance2(a, b) (((a)[0] - (b)[0]) * ((a)[0] - (b)[0]) + ((a)[1] - (b)[1]) * ((a)[1] - (b)[1]) + ((a)[2] - (b)[2]) * ((a)[2] - (b)[2]))
#define VectorDistance(a, b) (sqrt(VectorDistance2(a,b)))
#define VectorLength(a) sqrt(DotProduct(a, a))
#define VectorScale(in, _scale, out) { float scale = (_scale); (out)[0] = (in)[0] * scale; (out)[1] = (in)[1] * scale; (out)[2] = (in)[2] * scale; }
#define VectorCompare(a,b) (((a)[0]==(b)[0])&&((a)[1]==(b)[1])&&((a)[2]==(b)[2]))
#define VectorMA(a, scale, b, c) ((c)[0] = (a)[0] + (scale) * (b)[0],(c)[1] = (a)[1] + (scale) * (b)[1],(c)[2] = (a)[2] + (scale) * (b)[2])

#define VectorInverse(v)									\
(															\
	v[0]	= -v[0],										\
	v[1]	= -v[1],										\
	v[2]	= -v[2]											\
)

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

int		BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);
void	Math_Init();
void	AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void	VectorVectors(const vec3_t forward, vec3_t right, vec3_t up);
void	RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
float	VectorNormalize(vec3_t v);
float	anglemod(float a);

// LordHavoc: like AngleVectors, but taking a forward vector instead of angles, useful!
void VectorVectors(const vec3_t forward, vec3_t right, vec3_t up);
