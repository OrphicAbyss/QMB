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
// cl_tent.c -- client side temporary entities

#include "quakedef.h"
#include "gl_rpart.h"

int			num_temp_entities;
entity_t	cl_temp_entities[MAX_TEMP_ENTITIES];
beam_t		cl_beams[MAX_BEAMS];

sfx_t			*cl_sfx_wizhit;
sfx_t			*cl_sfx_knighthit;
sfx_t			*cl_sfx_tink1;
sfx_t			*cl_sfx_ric1;
sfx_t			*cl_sfx_ric2;
sfx_t			*cl_sfx_ric3;
sfx_t			*cl_sfx_r_exp3;
#ifdef QUAKE2
sfx_t			*cl_sfx_imp;
sfx_t			*cl_sfx_rail;
#endif

/*
=================
CL_ParseTEnt
=================
*/
void CL_InitTEnts (void)
{
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");
#ifdef QUAKE2
	cl_sfx_imp = S_PrecacheSound ("shambler/sattck1.wav");
	cl_sfx_rail = S_PrecacheSound ("weapons/lstart.wav");
#endif
}

/*
=================
CL_ParseBeam
=================
*/
void CL_ParseBeam ()
{
	extern  vec3_t zerodir;
	int		ent;
	vec3_t	start, end, point;
	beam_t	*b;
	dlight_t	*dl;
	int		i;//, count;
//	vec3_t	last, next;
	vec3_t	colour;
	
	ent = MSG_ReadShort ();
	
	start[0] = MSG_ReadCoord ();
	start[1] = MSG_ReadCoord ();
	start[2] = MSG_ReadCoord ();
	
	end[0] = MSG_ReadCoord ();
	end[1] = MSG_ReadCoord ();
	end[2] = MSG_ReadCoord ();

	//qmb :lighting dlight
	dl = CL_AllocDlight (0);
	VectorCopy (end, dl->origin);
	dl->radius = 300;
	dl->die = cl.time + 0.1;
	dl->decay = 300;
	// CDL - epca@powerup.com.au
	dl->colour[0] = 0.0f; dl->colour[1] = 0.2f; dl->colour[2] = 1.0f; //qmb :coloured lighting
	// CDL

	VectorSubtract(end, start, point);
	VectorScale(point, 0.5f, point);
	VectorAdd(start, point, point);

	dl = CL_AllocDlight (0);
	VectorCopy (point, dl->origin);
	dl->radius = 300;
	dl->die = cl.time + 0.1;
	dl->decay = 300;
	// CDL - epca@powerup.com.au
	dl->colour[0] = 0.0f; dl->colour[1] = 0.2f; dl->colour[2] = 1.0f; //qmb :coloured lighting
	// CDL

	dl = CL_AllocDlight (0);
	VectorCopy (start, dl->origin);
	dl->radius = 300;
	dl->die = cl.time + 0.1;
	dl->decay = 300;
	// CDL - epca@powerup.com.au
	dl->colour[0] = 0.0f; dl->colour[1] = 0.2f; dl->colour[2] = 1.0f; //qmb :coloured lighting
	// CDL

/*

	//set colour for main beam
	colour[0] = 0.7f; colour[1] = 0.7f; colour[2] = 1.0f;

	VectorSubtract(start, end, point);
	//work out the length and therefore the amount of trails
	count = Length(point)/5;
//	count /= 10;

	VectorScale(point, 1.0/count, point);
	VectorCopy(start, last);

	for (i=0; i<count; i++){
		VectorMA (start, -i, point, next);

		AddTrailColor(last, next, p_lightning, 0.15f, 3, colour, zerodir);
		VectorCopy(next, last);
	}*/
	// replaced by particles
// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent && cl.time < b->endtime)
		{
			b->entity = ent;
			b->endtime = cl.time + 0.2;

			VectorCopy(start,b->p1->org);
			VectorCopy(end,b->p1->org2);
			b->p1->start = cl.time;
			b->p1->die = b->endtime;

			VectorCopy(start,b->p2->org);
			VectorCopy(end,b->p2->org2);
			b->p2->start = cl.time;
			b->p2->die = b->endtime;

			VectorCopy(start,b->p3->org);
			VectorCopy(end,b->p3->org2);
			b->p3->start = cl.time;
			b->p3->die = b->endtime;

			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (b->endtime < cl.time)
		{
			colour[0] = 0.3f; colour[1] = 0.6f; colour[2] = 1.0f;

			b->entity = ent;
			b->endtime = cl.time + 0.2;

			b->p1 = AddTrailColor(start, end, p_lightning, 0.2f, 3, colour, zerodir);
			b->p2 = AddTrailColor(start, end, p_lightning, 0.2f, 4, colour, zerodir);
			b->p3 = AddTrailColor(start, end, p_lightning, 0.2f, 5, colour, zerodir);

			VectorCopy (start, b->start);
			VectorCopy (end, b->end);

			return;
		}
	}
	Con_Printf ("beam list overflow!\n");
	//*/
}

/*
=================
CL_ParseTEnt
=================
*/
void CL_ParseTEnt (void)
{
	extern  vec3_t zerodir;
	int		type, count, velspeed;
	vec3_t	pos, pos2, dir;
#ifdef QUAKE2
	vec3_t	endpos;
#endif
	dlight_t	*dl;
	int		rnd;
	int		colorStart, colorLength;
	unsigned char	*colourByte;

	type = MSG_ReadByte ();
	switch (type)
	{
	case TE_WIZSPIKE:
		// spike hitting wall
		MSG_ReadVector(pos);
		R_RunParticleEffect (pos, vec3_origin, 20, 30);
		S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
		break;
		
	case TE_KNIGHTSPIKE:
		// spike hitting wall
		MSG_ReadVector(pos);
		R_RunParticleEffect (pos, vec3_origin, 226, 20);
		S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
		break;
		
	case TE_SPIKE:			// spike hitting wall
		MSG_ReadVector(pos);
		R_RunParticleEffect (pos, vec3_origin, 0, 10);

		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SPIKEQUAD:
		// quad spike hitting wall
		MSG_ReadVector(pos);
		// LordHavoc: changed to spark shower
		R_RunParticleEffect (pos, vec3_origin, 0, 10);

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 200;
		dl->die = cl.time + 0.2;
		dl->decay = 1000;
		dl->colour[0] = 0.1f; dl->colour[1] = 0.1f; dl->colour[2] = 1.0f; //qmb :coloured lighting

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKE:			// super spike hitting wall
		MSG_ReadVector(pos);
		R_RunParticleEffect (pos, vec3_origin, 0, 20);

		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKEQUAD:
		// quad super spike hitting wall
		MSG_ReadVector(pos);
		// LordHavoc: changed to dust shower
		R_RunParticleEffect (pos, vec3_origin, 0, 20);

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 200;
		dl->die = cl.time + 0.2;
		dl->decay = 1000;
		dl->colour[0] = 0.1f; dl->colour[1] = 0.1f; dl->colour[2] = 1.0f; //qmb :coloured lighting
		
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
		// LordHavoc: added for improved blood splatters
	case TE_BLOOD:
		// blood puff
		MSG_ReadVector(pos);
		MSG_ReadVector(dir);
		count = MSG_ReadByte ();
		AddParticle(pos, count, 2, 1.5f, p_blood, dir);
		break;
	case TE_BLOOD2:
		// blood puff
		MSG_ReadVector(pos);
		AddParticle(pos, 10, 2, 1.5f, p_blood, zerodir);
		break;
	case TE_SPARK:
		// spark shower
		MSG_ReadVector(pos);
		dir[0] = MSG_ReadCoord ();
		dir[1] = MSG_ReadCoord ();
		dir[2] = MSG_ReadCoord ();
		count = MSG_ReadByte ();
		AddParticle(pos, count, 200, 1.5f, p_sparks, dir);
		break;
	case TE_PLASMABURN:
		MSG_ReadVector(pos);
		CL_AllocDlightDP (pos, 200, 1, 1, 1, 1000, 0.2f);
		//CL_PlasmaBurn(pos);
		break;
		// LordHavoc: added for improved gore
	case TE_BLOODSHOWER:
		// vaporized body
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		velspeed = MSG_ReadCoord (); // speed
		count = MSG_ReadShort (); // number of particles
		//CL_BloodShower(pos, pos2, velspeed, count);
		break;

	case TE_GUNSHOT:			// bullet hitting wall
		MSG_ReadVector(pos);
		R_RunParticleEffect (pos, vec3_origin, 0, 15);
		break;
		
	case TE_EXPLOSION:			// rocket explosion
		MSG_ReadVector(pos);
		R_ParticleExplosion (pos);
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		dl->colour[0] = 0.8f; dl->colour[1] = 0.4f; dl->colour[2] = 0.2f; //qmb :coloured lighting

     	S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;
		
	case TE_TAREXPLOSION:			// tarbaby explosion
		MSG_ReadVector(pos);
		R_BlobExplosion (pos);

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_LIGHTNING1:				// lightning bolts
		CL_ParseBeam ();
		break;
	
	case TE_LIGHTNING2:				// lightning bolts
		CL_ParseBeam ();
		break;
	
	case TE_LIGHTNING3:				// lightning bolts
		CL_ParseBeam ();
		break;
	
// PGM 01/21/97 
	case TE_BEAM:				// grappling hook beam
		CL_ParseBeam ();
		break;
// PGM 01/21/97

	case TE_LAVASPLASH:	
		MSG_ReadVector(pos);
		R_LavaSplash (pos);
		break;
	
	case TE_TELEPORT:
		MSG_ReadVector(pos);
		R_TeleportSplash (pos);

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		// CDL - epca@powerup.com.au
		dl->colour[0] = 1.0f; dl->colour[1] = 1.0f; dl->colour[2] = 1.0f; //qmb :coloured lighting
		// CDL
		break;
		
	case TE_EXPLOSION2:				// color mapped explosion
		MSG_ReadVector(pos);
		colorStart = MSG_ReadByte ();
		colorLength = MSG_ReadByte ();
		R_ParticleExplosion2 (pos, colorStart, colorLength);
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		//qmb :coloured lighting
		colourByte = (byte *)&d_8to24table[colorStart];
		dl->colour[0] = (float)colourByte[0]/255.0;
		dl->colour[1] = (float)colourByte[1]/255.0;
		dl->colour[2] = (float)colourByte[2]/255.0; 
		break;
		
#ifdef QUAKE2
	case TE_IMPLOSION:
		MSG_ReadVector(pos);
		S_StartSound (-1, 0, cl_sfx_imp, pos, 1, 1);
		break;

	case TE_RAILTRAIL:
		MSG_ReadVector(pos);
		MSG_ReadVector(pos);
		S_StartSound (-1, 0, cl_sfx_rail, pos, 1, 1);
		S_StartSound (-1, 1, cl_sfx_r_exp3, endpos, 1, 1);
		R_RocketTrail (pos, endpos, 0+128);
		R_ParticleExplosion (endpos);
		dl = CL_AllocDlight (-1);
		VectorCopy (endpos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		break;
#endif

	default:
		Con_Printf ("CL_ParseTEnt: temp ent type not supported: %i", type);
		//Sys_Error ("CL_ParseTEnt: bad type");
	}
}


/*
=================
CL_NewTempEntity
=================
*/
entity_t *CL_NewTempEntity (void)
{
	entity_t	*ent;

	if (cl_numvisedicts == MAX_VISEDICTS)
		return NULL;
	if (num_temp_entities == MAX_TEMP_ENTITIES)
		return NULL;
	ent = &cl_temp_entities[num_temp_entities];
	memset (ent, 0, sizeof(*ent));
	num_temp_entities++;
	cl_visedicts[cl_numvisedicts] = ent;
	cl_numvisedicts++;

	ent->colormap = vid.colormap;
	return ent;
}


/*
=================
CL_UpdateTEnts
=================
*/
void CL_UpdateTEnts (void)
{
	int			i;
	beam_t		*b;
	vec3_t		dist;//, org;
//	float		d;
//	entity_t	*ent;
	float		yaw, pitch;
	float		forward;

	num_temp_entities = 0;

// update lightning
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (b->endtime < cl.time)
			continue;

	// if coming from the player, update the start position
		if (b->entity == cl.viewentity)
		{
			VectorCopy (cl_entities[cl.viewentity].origin, b->start);
			VectorCopy (cl_entities[cl.viewentity].origin, b->p1->org);
			VectorCopy (cl_entities[cl.viewentity].origin, b->p2->org);
			VectorCopy (cl_entities[cl.viewentity].origin, b->p3->org);
		}

	// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(dist[1], dist[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (int) (atan2(dist[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

		/*
	// add new entities for the lightning
		VectorCopy (b->start, org);
		d = VectorNormalize(dist);
		while (d > 0)
		{
			ent = CL_NewTempEntity ();
			if (!ent)
				return;
			VectorCopy (org, ent->origin);
			ent->angles[0] = pitch;
			ent->angles[1] = yaw;
			ent->angles[2] = rand()%360;

			for (i=0 ; i<3 ; i++)
				org[i] += dist[i]*30;
			d -= 30;
		}*/
	}
	
}

void R_ClearBeams (void)
{
	int			i;
	beam_t		*b;

// clear lightning
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		b->endtime = cl.time - 1;
		b->entity = 0;
		b->p1 = NULL;
		b->p2 = NULL;
		b->p3 = NULL;
	}
}