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

/*******************************************************
QMB: 2.10

  FEATURES:

  Basic physics,  including: world colisions, velocity and constant acceleration.
  Easy extendable particle types.
  Drawing optimised for speed (not correctness).

  THINGS TO DO:
  Particle emitters.
  Depth Sorting (maybe) (uses too much time in current form).
  More advansed physics such as area effects, point gravity etc...
  Add Fuh's enhancments.
  maybe add some more documentation (never too many comments)

  REQUIREMENTS:
  **32bit texture loading (from www.quakesrc.org)**

  *include this file (gl_rpart.c and gl_rpart.h) in your source code
  *remove particle_t etc from glquake.h
  *remove the old r_part.c
  *compile :D
  *customise to taste

*******************************************************/

#include "quakedef.h"
#include "gl_rpart.h"

#ifdef JAVA
#include "java_vm.h"
#endif

//sin and cos tables for the particle triangle fans
static double sint[7] = {0.000000, 0.781832,  0.974928,  0.433884, -0.433884, -0.974928, -0.781832};
static double cost[7] = {1.000000, 0.623490, -0.222521, -0.900969, -0.900969, -0.222521,  0.623490};

//linked lists pointers for the first of the active, free and the a spare list for
//particles
particle_t			*active_particles,			*free_particles,		*particles;
//particle emitters
particle_tree_t		*particle_type_active,		*particle_type_free,	*particle_types;
//particle emitters
particle_emitter_t	*particle_emitter_active,	*particle_emitter_free,	*particle_emitter;

//Holder of the particle texture
//FIXME: wont work for custom particles
//		needs a structure system with {id, custom id, tex num}
int			part_tex, blood_tex, smoke_tex, trail_tex, bubble_tex, lightning_tex, spark_tex;

int			r_numparticles;			//number of particles in total
int			r_numparticletype;		//number of particle types
int			r_numparticleemitter;	//number of particle emitters

int			numParticles;			//current number of alive particles (used in fps display)

double		timepassed, timetemp;	//for use with emitters when programmed right
									//these will allow for calculating how many particle to add

vec3_t		zerodir = {1,1,1};		//particles with no direction bias. (really a const)
vec3_t		zero = {0,0,0};
vec3_t		coord[4];				//used in drawing, saves working it out for each particle

float		grav;
//was to stop particles not draw particles which were too close to the screen currently not used
//may be used again after depth sorting is implemented
CVar	gl_clipparticles("gl_clipparticles","0",true);
CVar	gl_smoketrail("gl_smoketrail","0",true);

//used for nv_pointsprits
//pointsprits dont work right anyway (prob some crazy quake matrix stuff)
pointPramFUNCv qglPointParameterfvEXT;
pointPramFUNC qglPointParameterfEXT;

//internal functions
void TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int accurate); //Physics, checks to see if a particle hit the world
void R_UpdateAll(void);						//used to run the particle update (do accelration and kill of old particles)
void MakeParticleTexure(void);				//loads the builtin textures
void DrawParticles(void);					//draws the particles

int CL_TruePointContents (vec3_t p)
{
	return SV_HullPointContents (&cl.worldmodel->hulls[0], 0, p);
}

__inline void RGBtoGrayscalef(vec3_t rgb){
	float value;

	if (gl_sincity.getBool()){
		//value = min(255,rgb[0] * 0.2125 + rgb[1] * 0.7154 + rgb[2] * 0.0721);
		//value = min(255,max(max(rgb[0],rgb[1]),rgb[2]));
		value = min(1,rgb[0] * 0.299 + rgb[1] * 0.587 + rgb[2] * 0.114);

		rgb[0] = value;
		rgb[1] = value;
		rgb[2] = value;
	}
}

/** R_ClearParticles
 * Reset all the pointers, reset the linklists
 * Clear all the particle data
 * Remake all the particle types
 */
//FIXME: needs to call qc to get custom particle types remade
void R_ClearParticles (void)
{
	int		i;

	numParticles = 0;						//no particles alive
	timepassed = cl.time;					//reset emitter times
	timetemp = cl.time;

	//particles
	free_particles = &particles[0];
	particle_type_active = NULL;			//no active particles

	for (i=0 ;i<r_numparticles ; i++)		//reset all particles
	{
		particles[i].next = &particles[i+1];
	}
	particles[r_numparticles-1].next = NULL;

	//particle types
	particle_type_free = &particle_types[0];
	particle_type_active = NULL;
	for (i=0 ;i<r_numparticletype ; i++)	//reset all particle types
	{
		particle_types[i].start = NULL;
		particle_types[i].next = &particle_types[i+1];
	}
	particle_types[r_numparticletype-1].next = NULL; //no next particle type for last type...

	//particle emitters
	for (i=0 ;i<r_numparticleemitter ; i++)
		particle_emitter[i].next = &particle_emitter[i+1];

	particle_emitter_active = NULL;
	particle_emitter_free = particle_emitter;

	/*
	particles are drawn in order here...
	some orders may look strange when particles overlap

	to add a new type:

	  AddParticleType(int src, int dst, part_move_t move, part_grav_t grav, part_type_t id, int custom_id, int texture, float startalpha)

  //blend mode
  //some examples are (gl_one,gl_one), (gl_src_alpha,gl_one_minus_src_alpha)
					src = GL_SRC_ALPHA;
					dst = GL_ONE;

  //colision&physics:	pm_static		:particle ignroes velocity
  						pm_nophysics	:particle with velocity but ignores the map
						pm_normal		:particle with velocity, stops moving when it hits a wall
						pm_float		:particle with velocity, only alive in the water
						pm_bounce		:particle with velocity, bounces off the world
						pm_bounce_fast	:particle with velocity, bounces off the world (no energy loss)
						pm_shrink		:particle with velocity, shrinks over time
						pm_die			:particle with velocity, dies after touching the map
						pm_grow			:particle with velocity, grows over time
					move = pm_die;

  //gravity effects:	pg_none				:no gravity acceleration
						pg_grav_low			:low gravity acceleration
						pg_grav_belownormal	:below normal gravity  acceleration
						pg_grav_normal		:normal gravity acceleration
						pg_grav_abovenormal	:above normal gravity acceleration
						pg_grav_high		:high gravity acceleration
						pg_rise_low			:low negitive gravity
						pg_rise				:normal negitive gravity
						pg_rise_high		:high negitive gravity
					grav = pg_none;

  //type of particle
  //list of set particles:
			built in to engine:
						p_sparks, p_smoke, p_fire, p_blood, p_chunks, p_lightning, p_bubble, p_trail
			for use with QC customisation
						p_custom
					id = p_fire;

  //will be used for QC custom controled particle types
			this field is ignored unless the 'id' field has p_custom
			used to uniquely define diffrent custom particles
					custom_id = 0;


  //what texture to use
					texture = part_tex;

  //the starting alpha value of the particles
					startalpha = 1;

	*/

	//make new particle types : sparks, blood, fire, chunks, bubble smoke, trail
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_die, pg_none, p_fire, 0, part_tex, 1);//fire
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_bounce, pg_grav_high, p_sparks, 0, spark_tex, 1);//sparks
	AddParticleType(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, pm_normal, pg_grav_normal, p_blood, 0, blood_tex, 1);//blood
	AddParticleType(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, pm_nophysics, pg_rise_low, p_smoke, 0, smoke_tex, 0.75f);//smoke
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_bounce_fast, pg_grav_normal, p_chunks, 0, part_tex, 1);//chunks
	AddParticleType(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, pm_float, pg_rise, p_bubble, 0, bubble_tex, 1);//bubble
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_static, pg_none, p_trail, 0, trail_tex, 1);//trail
	AddParticleType(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, pm_static, pg_none, p_lightning, 0, lightning_tex, 1);//lightning

	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_decal, pg_grav_high, p_decal, 0, part_tex, 1);
	//FIXME: add QC function call to reset the QC Custom particles
}

void AddParticleType(int src, int dst, part_move_t move, part_grav_t grav, part_type_t id, int custom_id, int texture, float startalpha)
{
	particle_tree_t *p;

	p = particle_type_free;
	particle_type_free = p->next;
	p->next = particle_type_active;
	particle_type_active = p;

	particle_type_active->SrcBlend = src;
	particle_type_active->DstBlend = dst;
	particle_type_active->move = move;
	particle_type_active->grav = grav;
	particle_type_active->id = id;
	particle_type_active->custom_id = custom_id;
	particle_type_active->texture = texture;
	particle_type_active->startalpha = startalpha;
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles (void)
{
	extern CVar sv_gravity;
	int		i;

	//check the command line to see if a number of particles was given particle
	i = COM_CheckParm ("-particles");
	if (i){
		r_numparticles = (int)(Q_atoi(com_argv[i+1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
			r_numparticles = ABSOLUTE_MIN_PARTICLES;	//cant have less than set min
	}
	else{
		r_numparticles = MAX_PARTICLES / 2;					//defualt to set half the 'max'
	}
	r_numparticletype = MAX_PARTICLE_TYPES;
	r_numparticleemitter = MAX_PARTICLE_EMITTER;

	//allocate memory for the particles and particle type linked lists
	particles = (particle_t *)Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");
	particle_types = (particle_tree_t *)Hunk_AllocName (r_numparticletype * sizeof(particle_tree_t), "particlestype");
	particle_emitter = (particle_emitter_t *)Hunk_AllocName (r_numparticleemitter * sizeof(particle_emitter_t), "particleemitters");

	//make the particle textures
	MakeParticleTexure();

	//reset the particles
	R_ClearParticles();

	//Regester particle cvars
	CVar::registerCVar(&gl_clipparticles);
	CVar::registerCVar(&gl_smoketrail);

	grav = 9.8*(sv_gravity.getFloat()/800.0f);
}

/*
===============
R_EntityParticles
===============
*/
#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;

void R_EntityParticles (entity_t *ent)
{
	int *colour = (int *)&d_8to24table[0x6f];
	R_ParticleExplosion2(ent->origin, *colour, 1);

	/*
	int			count;
	int			i;
	particle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist;
	byte		*colourByte;

	dist = 64;
	count = 50;

	if (!avelocities[0][0])
	{
	for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
	avelocities[0][i] = (rand()&255) * 0.01;
	}


	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		angle = cl.time * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = cl.time * avelocities[i][2];
		sr = sin(angle);
		cr = cos(angle);

		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->hit = 0;
		p->die = cl.time + 0.01;

		colourByte = (byte *)&d_8to24table[0x6f];
		p->colour[0] = colourByte[0]/255.0;
		p->colour[1] = colourByte[1]/255.0;
		p->colour[2] = colourByte[2]/255.0;

		//p->type = pt_explode;

		p->org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength;
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength;
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength;
	}
	Con_Printf ("EntityParticles");*/
}

//==========================================================
//Particle emitter code
//==========================================================

/** R_AddParticleEmitter
 * Will add a new emitter
 * Emitters will be able to be linked to a entity
 */
void R_AddParticleEmitter (vec3_t org, int count, int type, int size, float time, vec3_t colour, vec3_t dir)
{
	particle_emitter_t	*p;

	if (!particle_emitter_free)
		return;
	p = particle_emitter_free;
	particle_emitter_free = p->next;
	p->next = particle_emitter_active;
	particle_emitter_active = p;

	VectorCopy(org, p->org);
	p->count = count;
	p->type = type;
	p->size = size;
	p->time = time;
	VectorCopy(colour, p->colour);
	VectorCopy(dir, p->dir);
}

/** R_UpdateEmitters
 * Lets the emitters emit the particles :)
 * will be called every frame
 */
void R_UpdateEmitters (void)
{
	particle_emitter_t	*p;
	double				frametime;

	if ((cl.time == cl.oldtime))
		return;

	frametime = (cl.time - cl.oldtime);

	for (p = particle_emitter_active;p;p=p->next)
	{
		AddParticleColor(p->org,zero,max(1,(int)(p->count*frametime)),p->size,p->time,p->type,p->colour,p->dir);
	}
}

//==========================================================================
//Old particle calling code
//Now the functions call the new ones to make the particles
//This saves changing the whole engine, and makes it easy just to drop in
//the particle system.
//==========================================================================

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, count, msgcount, colour;

	for (i=0 ; i<3 ; i++)			//read in org
		org[i] = MSG_ReadCoord ();
	for (i=0 ; i<3 ; i++)			//read in direction
		dir[i] = MSG_ReadChar () * (1.0/16);
	msgcount = MSG_ReadByte ();		//read in number
	colour = MSG_ReadByte ();		//read in 8bit colour

	if (msgcount == 255)	//255 is a special number
		count = 1024;		//its actually a particle explosion
	else
		count = msgcount;

	R_RunParticleEffect (org, dir, colour, count);
}

/*===============
R_ParticleExplosion
===============*/
void R_ParticleExplosion (vec3_t org)
{
	vec3_t black;

	black[0] = 1;
	black[1] = 0;
	black[2] = 0;

	switch (CL_TruePointContents(org)) {
	case CONTENTS_WATER:
	case CONTENTS_SLIME:
	case CONTENTS_LAVA:
		AddParticle(org, 12, 14, 0.8f, p_fire, zerodir);
		AddParticle(org, 6, 3.4f, 2.5, p_bubble, zerodir);
		AddParticle(org, 64, 100, 0.75, p_sparks, zerodir);
		AddParticle(org, 32, 60, 0.75, p_sparks, zerodir);
		break;
	default:
		AddParticle(org, 18, 16, 1, p_fire, zerodir);
		AddParticle(org, 64, 300, 0.925f, p_sparks, zerodir);
		AddParticle(org, 32, 200, 0.925f, p_sparks, zerodir);
	}

	//AddParticle(org, 64, 300, 1.5f, p_sparks, zerodir);
	//AddParticle(org,  32, 200, 1.5f, p_sparks, zerodir);
	//AddParticle(org,  20,  25, 2.0f, p_fire, zerodir);
}

/*===============
R_ParticleExplosion2
===============*/
//Needs to be made to call new functions so that old ones can be removed
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	vec3_t	colour;
	byte	*colourByte;

	colourByte = (byte *)&d_8to24table[colorStart];
	colour[0] = (float)colourByte[0]/255.0;
	colour[1] = (float)colourByte[1]/255.0;
	colour[2] = (float)colourByte[2]/255.0;

	RGBtoGrayscalef(colour);

	AddParticleColor(org, zero, 64, 200, 1.5f, p_sparks, colour, zerodir);
	AddParticle(org, 64, 1, 3, p_smoke, zerodir);
}

/*===============
R_BlobExplosion
===============*/
//also do colored fires
//JHL; tweaked to look better
void R_BlobExplosion (vec3_t org)
{
	vec3_t	colour;
	int		i;

	colour[0] = colour[1] = 0.1f;
	colour[2]=1;

	RGBtoGrayscalef(colour);

	AddParticleColor (org, zero, 20,  2, 2.0f, p_blood, colour, zerodir);

	colour[0] = colour[1] = 0.4f;
	AddParticleColor (org, zero, 444, 200, 1.5f, p_sparks, colour, zerodir);

	for (i=0; i<10; i++)
	{
		colour[0] = colour[1] = (rand()%90)/255.0;
		AddParticleColor(org, zero, 1, 25, 1, p_fire, colour, zerodir);
	}
}

/*===============
R_RunParticleEffect
===============*/
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	byte		*colourByte;
	vec3_t		colour, tempdir;
	int			i;

	if ((dir[0] == 0) && (dir[1] == 0) && (dir[2] == 0))
		VectorCopy(zerodir,tempdir);
	else
		VectorCopy(dir, tempdir);

	colourByte = (byte *)&d_8to24table[color];
	colour[0] = colourByte[0]/255.0;
	colour[1] = colourByte[1]/255.0;
	colour[2] = colourByte[2]/255.0;

	RGBtoGrayscalef(colour);

	//QMB :REMOVE FOR OTHER ENGINES
	//START :REMOVE block comment "/* */" out this whole section
   //JHL:HACK; do qmb specific particles
	if (color > 240 && color < 255 && qmb_mod)
	{
		//JHL:NOTE; ADD THE BUBBLE!!
		if (color == 241)		// water bubbles
			AddParticle (org, count, 2, 6, p_bubble, tempdir);
		else if (color == 242)	// sparks
			AddParticle (org, count, 100, 1.0f, p_sparks, tempdir);
		else if (color == 243)	// chunks
		{
			/*
			for (i=0; i<count; i++)
			{
				colour[0] = colour[1] = colour[2] = (rand()%128)/255.0+64;
				AddParticleColor (org, zero, 1, 1, 4, p_chunks, colour, tempdir);
			}
			*/
		}
		//JHL:NOTE; ADD ELECTRIC BUZZ (p_lightning?)!!
		else if (color == 244)	// electric sparks
		{
			colour[2] = 1.0f;
			for (i=0; i<count; i++)
			{
				colour[0] = colour[1] = 0.4 + ((rand()%90)/255.0);
				AddParticleColor (org, zero, 1, 100, 1.0f, p_sparks, colour, tempdir);
			}
		}
		//JHL:NOTE; ADD WATER DROPS!!
		else if (color == 245)	// rain
			AddParticle (org, count, 100, 1.0f, p_sparks, tempdir);
	}
	else
	//END :REMOVE
	{
		if (count == 15)
		{	//JHL:HACK; better looking gunshot (?)
			colour[0] = colour[1] = colour[2] = 0.6f;
			AddParticleColor(org, zero, 1, 2, 1, p_smoke, colour, tempdir);


			//JHL:HACK; keept comptability with other mods (done QC side in qmb mod)
			if (!qmb_mod) //QMB :REMOVE FOR OTHER ENGINES JUST THIS LINE
			{
				for (i=0; i<8; i++){
					colour[0] = colour[1] = colour[2] = (rand()%90)/255.0;
					AddParticleColor(org, zero, 1, 1, 4, p_chunks, colour, tempdir);
				}
				AddParticle(org, 1, 100, 1.0f, p_sparks, tempdir);
			}
		}else if (count == 10)
		{
			//AddParticleColor(org, zero, 10, 1, 4, p_chunks, colour, tempdir);
			AddParticle(org, 3, 100, 1.0f, p_sparks, tempdir);
		}else if (count == 20)
		{
			//AddParticleColor(org, zero, 10, 1, 4, p_chunks, colour, tempdir);
			AddParticle(org, 10, 100, 1.0f, p_sparks, zerodir);
		}else if (count == 30)
		{
			//AddParticleColor(org, zero, 10, 1, 4, p_chunks, colour, tempdir);
			AddParticle(org, 5, 100, 1.0f, p_sparks, zerodir);
			AddParticle(org, 50, 200, 0.5f, p_sparks, zerodir);
		}else if (count == 1024)
			R_ParticleExplosion(org);
		else
		{
			//JHL:HACK; make blood brighter...
			if (color == 73){
                if (gl_sincity.getBool())
					colour[0] = max(1.0f,colour[0]*2);
				//colour[1] = 0f;
				//colour[2] = 0f;
			}
			AddParticleColor(org, zero, count*2, 3, 3, p_blood, colour, tempdir);
		}
	}
}


/*===============
R_LavaSplash
===============*/
//Need to find out when this is called
//JHL:NOTE; When Chthon sinks to lava...
//QMB: yep i worked that out, thanx (found out its also used in TF for the spy gren...)
void R_LavaSplash (vec3_t org)
{
	AddParticle(org, 1000, 250, 10, p_sparks, zerodir);
	AddParticle(org, 1000, 500, 10, p_sparks, zerodir);
	AddParticle(org, 100, 50, 6, p_fire, zerodir);
}

/*===============
R_TeleportSplash
===============*/
//Need to be changed so that they spin down into the ground
//whould look very cool
//maybe coloured blood (new type?)
void R_TeleportSplash (vec3_t org)
{
	vec3_t	colour;

	colour[0]=0.9f;
	colour[1]=0.9f;
	colour[2]=0.9f;

	AddParticleColor(org, zero, 256, 200, 1.0f, p_sparks, colour, zerodir);
}

//Should be made to call the new functions to keep compatablity
void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t		colour;

	switch (type)
	{
		case 0:	// rocket trail
			AddTrail(start, end, p_fire, 0.1f, 6, zerodir);
			AddTrail(start, end, p_smoke, 1, 6, zerodir);
			AddTrail(start, end, p_sparks, 0.2f, 1, zerodir);
			break;

		case 1:	// smoke smoke
			AddTrail(start, end, p_smoke, 1, 2, zerodir);
			break;

		case 2:	// blood
			AddTrail(start, end, p_blood, 2, 3, zerodir);
			break;

		case 3:	//tracer 1
			colour[0]=0.1f;	colour[1]=0.75f;	colour[2]=0.1f;
			RGBtoGrayscalef(colour);

			AddTrailColor (start, end, p_sparks, 2, 2, colour, zerodir);
			break;

		case 5:	// tracer 2
			colour[0]=1;	colour[1]=0.85f;	colour[2]=0;
			RGBtoGrayscalef(colour);

			AddTrailColor (start, end, p_sparks, 2, 2, colour, zerodir);
			break;

		case 4:	// slight blood
			AddParticle(end, 10, 1, 2, p_blood, zerodir);
			break;

		case 6:	// voor trail
			colour[0]=1;	colour[1]=0;	colour[2]=0.75f;
			RGBtoGrayscalef(colour);

			AddTrailColor (start, end, p_sparks, 2, 2, colour, zerodir);
			break;
	}
}

//============================================================
//Particle drawing code
//============================================================

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	if (cl.time != cl.oldtime)
		R_UpdateAll();

	glDepthMask(0);							//not depth sorted so dont let particles block other particles...
	glEnable (GL_BLEND);					//all particles are blended
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	DrawParticles();

	glDepthMask(1);
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

#ifdef JAVA
void Java_DrawParticle(JNIEnv *env, jclass this, float x, float y, float z, float size, float r, float g, float b, float a, float rotation){
	vec3_t	org;
	vec3_t	distance;

	org[0] = x;
	org[1] = y;
	org[2] = z;

	//test to see if particle is too close to the screen (high fill rate usage)
	if (gl_clipparticles.value) {
		VectorSubtract(org, r_origin, distance);
		if (distance[0] * distance[0] + distance[1] * distance[1] + distance[2] * distance[2] < 3200)
			return;
	}

	glColor4f (r,g,b,a);

	glPushMatrix ();
		glTranslatef(org[0], org[1], org[2]);
		glScalef (size, size, size);

		glRotatef (rotation,  vpn[0], vpn[1], vpn[2]);

		glBegin (GL_QUADS);
			glTexCoord2f (0, 1);	glVertex3fv (coord[0]);
			glTexCoord2f (0, 0);	glVertex3fv (coord[1]);
			glTexCoord2f (1, 0);	glVertex3fv (coord[2]);
			glTexCoord2f (1, 1);	glVertex3fv (coord[3]);
		glEnd ();

	glPopMatrix();
}
#endif

/** GL_QuadPointsForBeam
 * Draws a beam sprite between 2 points
 * LH's code
 */
void GL_QuadPointsForBeam(vec3_t start, vec3_t end, vec3_t offset, float t1, float t2)
{
	vec3_t temp;

	VectorAdd(start, offset, temp);
	glTexCoord2f(1+t1,0);
	glVertex3fv(temp);
	VectorSubtract(start, offset, temp);
	glTexCoord2f(1+t1,1);
	glVertex3fv(temp);
	VectorSubtract(end, offset, temp);
	glTexCoord2f(0+t2,1);
	glVertex3fv(temp);
	VectorAdd(end, offset, temp);
	glTexCoord2f(0+t2,0);
	glVertex3fv(temp);
}

/** DrawParticles
 * Main drawing code...
 */
void DrawParticles(void)
{
	particle_tree_t	*pt;
	particle_t		*p;

	vec3_t			decal_vec[4], decal_up, decal_right, v, normal;

	vec3_t			distance;
	int				drawncount;
	int				lasttype=0;
	//point sprite thingo
	static GLfloat constant[3] = { 1.0f, 0.0f, 0.0f };
	static GLfloat linear[3] = { 0.0f, 1.0f, 0.0f };
	static GLfloat quadratic[3] = { 0.25f, 0.0f, -1.0f };

	if (gl_point_sprite){
		qglPointParameterfvEXT( GL_DISTANCE_ATTENUATION_EXT, constant);
		//qglPointParameterfEXT( GL_POINT_SIZE_MAX_EXT, 260.0f);
		//qglPointParameterfEXT( GL_POINT_SIZE_MIN_EXT, 1.0f);
	}

	VectorAdd (vup, vright, coord[2]);
	VectorSubtract (vright, vup, coord[3]);
	VectorNegate (coord[2], coord[0]);
	VectorNegate (coord[3], coord[1]);

	for (pt=particle_type_active; pt ; pt=pt->next){
		glBlendFunc(pt->SrcBlend, pt->DstBlend);
		if (pt->texture!=0&&pt->id != p_sparks &&pt->move!=pm_decal)
		{
			glBindTexture(GL_TEXTURE_2D,pt->texture);

			if ((pt->id != p_trail)&&(pt->id != p_lightning)){
				//all textured particles except trails and lightning
				drawncount = 0;

				for (p=pt->start; p ; p=p->next){

					//test to see if particle is too close to the screen (high fill rate usage)
					if (gl_clipparticles.getBool() && drawncount >= 4) {
						VectorSubtract(p->org, r_origin, distance);
						if (distance[0] * distance[0] + distance[1] * distance[1] + distance[2] * distance[2] < 3200)
							continue;
					}
					drawncount++;

					glColor4f (p->colour[0],p->colour[1],p->colour[2],p->ramp);

					if (p->hit == 0){
			/***NV_point_sprits***/
			//Should work but is fucked
			//They scale the wrong way
			//Shrinking as they get close insted of growing
						if (gl_point_sprite&&false){
							glTexEnvf(GL_POINT_SPRITE_NV, GL_COORD_REPLACE_NV, GL_TRUE);
							glEnable(GL_POINT_SPRITE_NV);
							//glEnable(GL_POINT_SMOOTH);

							glPointSize(100);//p->size);
							glColor4f (p->colour[0],p->colour[1],p->colour[2],p->ramp);

							glBegin(GL_POINTS);
								glVertex3fv(p->org);
							glEnd();

							glDisable(GL_POINT_SPRITE_NV);
						}else{
							glPushMatrix ();
								glTranslatef(p->org[0], p->org[1], p->org[2]);
								glScalef (p->size, p->size, p->size);

								if (p->rotation_speed)
									glRotatef (p->rotation,  vpn[0], vpn[1], vpn[2]);

								glBegin (GL_QUADS);
									glTexCoord2f (0, 1);	glVertex3fv (coord[0]);
									glTexCoord2f (0, 0);	glVertex3fv (coord[1]);
									glTexCoord2f (1, 0);	glVertex3fv (coord[2]);
									glTexCoord2f (1, 1);	glVertex3fv (coord[3]);
								glEnd ();

							glPopMatrix();
						}
					}else{
						//check what side the of the decal we are drawing
						VectorNormalize2(p->vel, normal);
						if (DotProduct(normal, r_origin) > DotProduct(normal, p->org))
						{
							VectorNegate(normal, v);
							VectorVectors(v, decal_right, decal_up);
						}
						else
							VectorVectors(normal, decal_right, decal_up);

						VectorScale(decal_right, p->size, decal_right);
						VectorScale(decal_up, p->size, decal_up);

						decal_vec[0][0] = p->org[0] - decal_right[0] - decal_up[0];
						decal_vec[0][1] = p->org[1] - decal_right[1] - decal_up[1];
						decal_vec[0][2] = p->org[2] - decal_right[2] - decal_up[2];
						decal_vec[1][0] = p->org[0] - decal_right[0] + decal_up[0];
						decal_vec[1][1] = p->org[1] - decal_right[1] + decal_up[1];
						decal_vec[1][2] = p->org[2] - decal_right[2] + decal_up[2];
						decal_vec[2][0] = p->org[0] + decal_right[0] + decal_up[0];
						decal_vec[2][1] = p->org[1] + decal_right[1] + decal_up[1];
						decal_vec[2][2] = p->org[2] + decal_right[2] + decal_up[2];
						decal_vec[3][0] = p->org[0] + decal_right[0] - decal_up[0];
						decal_vec[3][1] = p->org[1] + decal_right[1] - decal_up[1];
						decal_vec[3][2] = p->org[2] + decal_right[2] - decal_up[2];

						glBegin (GL_QUADS);
							glTexCoord2f (0, 1);	glVertex3fv (decal_vec[0]);
							glTexCoord2f (0, 0);	glVertex3fv (decal_vec[1]);
							glTexCoord2f (1, 0);	glVertex3fv (decal_vec[2]);
							glTexCoord2f (1, 1);	glVertex3fv (decal_vec[3]);
						glEnd ();
					}

				}
			}else{
				//trails and lightning
				int		lengthscale;
				float	t1, t2, scrollspeed, radius, length;
				vec3_t	temp, offset;

				glDisable(GL_CULL_FACE);
				glBegin(GL_QUADS);
				for (p=pt->start ; p ; p=p->next)
				{
					glColor4f (p->colour[0],p->colour[1],p->colour[2],p->ramp);

					VectorSubtract(p->org2, p->org, temp);
					length = sqrt(DotProduct(temp, temp)); // pythagoran theorm for 3D distance

					// configurable numbers
					radius = p->size; // thickness of beam
					scrollspeed = -3.0; // scroll speed, 1 means it scrolls the entire height of the texture each second
					lengthscale = 40; // how much distance in quake units it takes for the texture to repeat once

					t1 = cl.time * scrollspeed + p->size;
					t1 -= (int)t1; // remove the unnecessary integer portion of the number
					t2 = t1 + (length / lengthscale);

					VectorMA(vright, radius, vright, offset);
					GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2);

					VectorAdd(vright, vup, offset);
					VectorNormalize(offset);
					VectorScale(offset, radius, offset);
					GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2);

					VectorSubtract(vright, vup, offset);
					VectorNormalize(offset);
					VectorScale(offset, radius, offset);
					GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2);

					// configurable numbers
					radius = p->size*1.5; // thickness of beam
					scrollspeed = -1.5; // scroll speed, 1 means it scrolls the entire height of the texture each second
					lengthscale = 80; // how much distance in quake units it takes for the texture to repeat once

					t1 = cl.time * scrollspeed + p->size;
					t1 -= (int)t1; // remove the unnecessary integer portion of the number
					t2 = t1 + (length / lengthscale);

					VectorMA(vright, radius, vright, offset);
					GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2);

					VectorAdd(vright, vup, offset);
					VectorNormalize(offset);
					VectorScale(offset, radius, offset);
					GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2);

					VectorSubtract(vright, vup, offset);
					VectorNormalize(offset);
					VectorScale(offset, radius, offset);
					GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2);
				}

				glEnd();
				if (gl_cull.getBool())
					glEnable(GL_CULL_FACE);
			}
		}else{
			if (pt->id!=p_decal){
				//Sparks...
				glDisable(GL_CULL_FACE);

				glBindTexture(GL_TEXTURE_2D,pt->texture);

				for (p=pt->start; p ; p=p->next){
					vec3_t dup, offset;

					VectorScale(p->vel, 0.125f, dup);
					VectorSubtract(p->org, dup, dup);

					glBegin(GL_QUADS);
						glColor4f (p->ramp*p->colour[0],p->ramp*p->colour[1],p->ramp*p->colour[2],p->ramp);
						//glColor4f (0,0,p->ramp*1.0f,p->ramp);

						VectorMA(vright, p->size, vright, offset);
						GL_QuadPointsForBeam(p->org, dup, offset, 1.0f, 1.0f);

						VectorAdd(vright, vup, offset);
						VectorNormalize(offset);
						VectorScale(offset, p->size, offset);
						GL_QuadPointsForBeam(p->org, dup, offset, 1.0f, 1.0f);

						VectorSubtract(vright, vup, offset);
						VectorNormalize(offset);
						VectorScale(offset, p->size, offset);
						GL_QuadPointsForBeam(p->org, dup, offset, 1.0f, 1.0f);

					glEnd();
				}

				if (gl_cull.getBool())
					glEnable(GL_CULL_FACE);
			} else {
				glBindTexture(GL_TEXTURE_2D,pt->texture);

				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(-1,1);

				for (p=pt->start ; p ; p=p->next)
				{
					if (p->hit == 0)
						continue;

					//check what side the of the decal we are drawing
					VectorNormalize2(p->vel, normal);
					if (DotProduct(normal, r_origin) > DotProduct(normal, p->org))
					{
						VectorNegate(normal, v);
						VectorVectors(v, decal_right, decal_up);
					}
					else
						VectorVectors(normal, decal_right, decal_up);

					VectorScale(decal_right, p->size, decal_right);
					VectorScale(decal_up, p->size, decal_up);

					decal_vec[0][0] = p->org[0] - decal_right[0] - decal_up[0];
					decal_vec[0][1] = p->org[1] - decal_right[1] - decal_up[1];
					decal_vec[0][2] = p->org[2] - decal_right[2] - decal_up[2];
					decal_vec[1][0] = p->org[0] - decal_right[0] + decal_up[0];
					decal_vec[1][1] = p->org[1] - decal_right[1] + decal_up[1];
					decal_vec[1][2] = p->org[2] - decal_right[2] + decal_up[2];
					decal_vec[2][0] = p->org[0] + decal_right[0] + decal_up[0];
					decal_vec[2][1] = p->org[1] + decal_right[1] + decal_up[1];
					decal_vec[2][2] = p->org[2] + decal_right[2] + decal_up[2];
					decal_vec[3][0] = p->org[0] + decal_right[0] - decal_up[0];
					decal_vec[3][1] = p->org[1] + decal_right[1] - decal_up[1];
					decal_vec[3][2] = p->org[2] + decal_right[2] - decal_up[2];

					glBegin (GL_QUADS);
						glColor4f (p->colour[0],p->colour[1],p->colour[2],p->ramp);

						glTexCoord2f (0, 1);	glVertex3fv (decal_vec[0]);
						glTexCoord2f (0, 0);	glVertex3fv (decal_vec[1]);
						glTexCoord2f (1, 0);	glVertex3fv (decal_vec[2]);
						glTexCoord2f (1, 1);	glVertex3fv (decal_vec[3]);
					glEnd ();
				}

				glPolygonOffset(0,0);
				glDisable(GL_POLYGON_OFFSET_FILL);
			}
		}
	}
}

//================================================================
//Particle physics code
//================================================================
/** R_UpdateAll
 * Do the physics, kill off old particles
 */
void R_UpdateAll(void)
{
	extern CVar sv_gravity;
	particle_tree_t	*pt;
	particle_t		*p, *kill;
	float			frametime, dist;
//	double			tempVelocity;
	int				contents;

	vec3_t			oldOrg, stop, normal;//, tempVec;

	//Work out gravity and time
	grav = 9.8*(sv_gravity.getFloat()/800.0f);	//just incase sv_gravity has changed
	frametime = (cl.time - cl.oldtime);

	//for each particle type
	for(pt=particle_type_active ; pt ; pt=pt->next){
		//if there are actually particles
		if (pt->start){
			//clear out dead particles
			if (pt->start->next){
				for(p=pt->start; (p)&&(p->next) ; p=p->next){
					kill = p->next;
					//check if it should be removed
					if (kill->die <= cl.time)
					{
						p->next = kill->next;
						kill->next = free_particles;
						free_particles = kill;
						numParticles--;
					//otherwise change the order, (test code not sure how well it will work)
					}/* else if (kill->next && (kill->next->dist > kill->dist)){
						//particles further away should be drawn first
						//move them up the list

						p->next = kill->next;
						kill->next = kill->next->next;
						p->next->next = kill;
					}*/
					if (!p) break;
				}
			}

			//This code here removes the first particle from a list if its dead
			//the generic code for removing them from the list cant remove them
			//from the front, and so with out this you end up always having
			//one particle for each list.
			if (pt->start->die <= cl.time){
				kill = pt->start;
				pt->start = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				numParticles--;
			}

			/*
			//reorder particles
			//need better sorting algro
			if (pt->start){
				if (pt->start->next){
					int changed, i;
					for (i=0, changed = 1; changed==1 && i<128; i++){
						for(p=pt->start; (p)&&(p->next)&&(p->next->next) ; p=p->next){
							kill = p->next;
							//change the order, (test code not sure how well it will work)
							if (kill->next->dist > kill->dist){
								//particles further away should be drawn first
								//move them up the list

								p->next = kill->next;
								kill->next = kill->next->next;
								p->next->next = kill;

								changed = 1;
							}
							if (!p) break;
						}
					}
				}
			}
			*/
		}

		//loop through for physic code as well
		for(p=pt->start; p ; p=p->next){
			vec3_t	temp;

			VectorSubtract(p->org, r_origin, temp);
			p->dist = VectorNormalize(temp);	//code to work out distance from viewer

			p->ramp = (1-(cl.time-p->start)/(p->die-p->start)) * pt->startalpha;

			if (p->hit)
				continue;	//if hit is set dont do any physics code

			VectorCopy(p->org, oldOrg);

			if (pt->move != pm_static && p->hit == 0){
				//find new position
				p->org[0] += p->vel[0] * frametime + 0.5 * p->acc[0] * frametime * frametime;
				p->org[1] += p->vel[1] * frametime + 0.5 * p->acc[1] * frametime * frametime;
				p->org[2] += p->vel[2] * frametime + 0.5 * p->acc[2] * frametime * frametime;

				//calculate new velocity
				p->vel[0] += p->acc[0] * frametime;
				p->vel[1] += p->acc[1] * frametime;
				p->vel[2] += p->acc[2] * frametime;
			}

			switch (pt->move)
			{
			case (pm_static):
			case (pm_nophysics):
				//do nothing :)
				break;
			case (pm_decal):
				//if the particle hits a wall stop physics and setup as decal
				//could use this to kill of particle and call decal code
				//to seperate decal and particle systems
				if (CONTENTS_SOLID == CL_TruePointContents (p->org))
				{
					TraceLineN(oldOrg, p->org, stop, normal,1);
					if ((stop != p->org)&&(Length(stop)!=0))
					{
						p->hit = 1;
						//work out exactly where we hit and what the normal was


						//copy our hit position to our position
						VectorCopy(stop, p->org);
						//VectorMA(stop,-1,normal,p->org);

						//set our velocity to be the normal
						//this is used by the rendering code to work out what direction to face
						VectorCopy(normal,p->vel);

						p->die = p->start + (p->die - p->start)*(rand())/10.0f;
						//that is all
					}
				}
				break;
			case (pm_normal):
				//if the particle hits a wall stop physics
				if (CONTENTS_SOLID == CL_TruePointContents (p->org))
				{
					p->hit = 1;

					TraceLineN(oldOrg, p->org, stop, normal,0);
					//VectorCopy(oldOrg, p->org);
					VectorCopy(stop, p->org);
					VectorCopy(normal,p->vel);
				}
				break;
			case (pm_float):
				//if the particle leaves water/slime/lava kill it off
				contents = CL_TruePointContents (p->org);
				if (contents != CONTENTS_WATER && contents != CONTENTS_SLIME && contents != CONTENTS_LAVA)
					p->die = 0;
				break;
			case (pm_bounce):
				//make the particle bounce off walls
				if (CONTENTS_SOLID == CL_TruePointContents (p->org))
				{
					TraceLineN(oldOrg, p->org, stop, normal,0);
					if ((stop != p->org)&&(Length(stop)!=0))
					{
						dist = DotProduct(p->vel, normal) * -1.3;

						VectorMA(p->vel, dist, normal, p->vel);
						VectorCopy(stop, p->org);
					}
				}
				break;
			case (pm_bounce_fast):
				//make it bounce higher
				if (CONTENTS_SOLID == CL_TruePointContents (p->org))
				{
					TraceLineN(oldOrg, p->org, stop, normal,0);
					if ((stop != p->org)&&(Length(stop)!=0))
					{
						dist = DotProduct(p->vel, normal) * -1.4;

						VectorMA(p->vel, dist, normal, p->vel);
						VectorCopy(stop, p->org);
					}
				}

				break;
			case (pm_die):
				//acceleration should take care of this
				//tempVelocity = max(0.001,min(1,frametime*3));
				//VectorScale(p->vel, (1-tempVelocity), p->vel);
  				if (CONTENTS_SOLID == CL_TruePointContents (p->org))
					p->die=0;

				break;
			case (pm_shrink):
				//this should never happen
				p->size -= 6*(cl.time-cl.oldtime);
				break;
			}

			if (p->type == pm_shrink)
				p->size -= 6*(cl.time-cl.oldtime);
			if (p->type == pm_grow)
				p->size += 6*(cl.time-cl.oldtime);

			//might be faster just to rotate on a p4 (stop pipeline stalls)
			if (p->rotation_speed){
				p->rotation += p->rotation_speed * frametime;
			}

			if (pt->id == p_lightning){
				AddParticleColor(p->org2, zero, 1, 100.0f, 0.5f, p_sparks, p->colour, zerodir);
			}
		}
	}
}

/** TraceLineN
 * same as the TraceLine but returns the normal as well
 * which is needed for bouncing particles
 */
void TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int accurate)
{
	trace_t	trace;
	vec3_t	temp;

	Q_memset (&trace, 0, sizeof(trace));

	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

	if (accurate) {
		//calculate actual impact
		VectorSubtract(end, start, temp);
		VectorScale(temp, trace.realfraction, temp);
		VectorAdd(start, temp, impact);
	}else{
		VectorCopy (trace.endpos, impact);
	}

	VectorCopy (trace.plane.normal, normal);
}

//=================================================================
//New particle adding code
//=================================================================

void addGrav(particle_tree_t *pt, particle_t *p)
{
//add gravity to acceleration
	switch (pt->grav) {
//fall
	case (pg_grav_low):
		//p->vel[2] -= grav*4; old value
		p->acc[2] += -grav * 0.125; //fuh value
		break;
	case (pg_grav_belownormal):
		p->acc[2] += -grav;
		break;
	case (pg_grav_normal):
		p->acc[2] += -grav * 8;
		break;
	case (pg_grav_abovenormal):
		p->acc[2] += -grav * 16;
		break;
	case (pg_grav_high):
		p->acc[2] += -grav * 32;
		break;
//rise
	case (pg_rise_low):
		p->acc[2] += grav * 4;
		break;
	case (pg_rise):
		p->acc[2] += grav * 8;
		break;
	case (pg_rise_high):
		p->acc[2] += grav * 16;
		break;
//none
	case (pg_none):
		//do nothing
		break;
	}
}

/** AddParticle
 * Just calls AddParticleColor with the default color variable for that particle
 */
void AddParticle(vec3_t org, int count, float size, float time, int type, vec3_t dir)
{
	//Times: smoke=3, spark=2, blood=3, chunk=4
	vec3_t	colour;

	switch (type)
	{
	case (p_smoke):
		colour[0] = colour[1] = colour[2] = (rand()%255)/255.0;
		break;
	case (p_sparks):
		colour[0] = 1;
		colour[1] = 0.5;
		colour[2] = 0;
		RGBtoGrayscalef(colour);
		break;
	case (p_fire):
		//JHL; more QUAKE palette alike color (?)
		colour[0] = 1;
		colour[1] = 0.62f;
		colour[2] = 0.3f;
		RGBtoGrayscalef(colour);
		break;
	case (p_blood):
		if (gl_sincity.getBool()){
			colour[0] = (rand()%128)/256.0+0.45f;;
		}else{
			colour[0] = (rand()%128)/256.0+0.25f;;
		}
		colour[1] = 0;
		colour[2] = 0;
		//colour[0] = colour[1] = colour[2] = 1.0f;
		break;
	case (p_chunks):
		colour[0] = colour[1] = colour[2] = (rand()%182)/255.0;
	}

	AddParticleColor(org, zero, count, size, time, type, colour, dir);
}

//same as addparticle with a colour var

/** AddParticleColor
 * This is where it all happends, well not really this is where
 * most of the particles (and soon all) are added execpt for trails
 */
void AddParticleColor(vec3_t org, vec3_t org2, int count, float size, float time, int type, vec3_t colour, vec3_t dir)
{
	particle_t		*p;
	particle_tree_t	*pt;
	vec3_t			stop, normal;
	int				i;
	float			tempSize;

	if (dir[0]==0&&dir[1]==0&&dir[2]==0)
		VectorCopy(zerodir, dir);

	if (type == p_smoke)
	{
		//test to see if its in water
		switch (CL_TruePointContents(org))
		{
		case CONTENTS_WATER:
		case CONTENTS_SLIME:
		case CONTENTS_LAVA:
			for (pt = particle_type_active; (pt) && (pt->id != p_bubble); pt = pt->next);
			break;
		default:	//not in water do it normally
			for (pt = particle_type_active; (pt) && (pt->id != type); pt = pt->next);
		}
	}else
		for (pt = particle_type_active; (pt) && (pt->id != type); pt = pt->next);
	//find correct particle tree to add too...


	for (i=0 ; (i<count)&&(free_particles)&&(pt) ; i++)
	{
		//get the next free particle off the list
		p = free_particles;
		//reset the next free particle to the next on the list
		free_particles = p->next;
		//add the particle to the correct one
		p->next = pt->start;
		pt->start = p;

		p->size = size;
		p->rotation_speed = 0;
		switch (type)
		{
		case (p_decal):
			VectorCopy(org, p->org);
			//VectorCopy(dir, p->vel);
			//velocity
			p->vel[0] = ((rand()%20)-10)/5.0f*size;
			p->vel[1] = ((rand()%20)-10)/5.0f*size;
			p->vel[2] = ((rand()%20)-10)/5.0f*size;

			break;
		case (p_fire):
			//pos
			if (VectorCompare(zero,org2)) {
				VectorCopy(org, p->org);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}			//velocity
			p->vel[0] = ((rand()%200)-100)*(size/25)*dir[0];
			p->vel[1] = ((rand()%200)-100)*(size/25)*dir[1];
			p->vel[2] = ((rand()%200)-100)*(size/25)*dir[2];
			break;

		case (p_sparks):
			p->size = 1;
			//pos
			if (VectorCompare(zero,org2)) {
				VectorCopy(org, p->org);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}
			//velocity
			tempSize = size * 2;
			p->vel[0] = (rand()%(int)tempSize)-((int)tempSize/2)*dir[0];
			p->vel[1] = (rand()%(int)tempSize)-((int)tempSize/2)*dir[1];
			p->vel[2] = (rand()%(int)tempSize)-((int)tempSize/3)*dir[2];
			break;

		case (p_smoke):
			//pos
			if (VectorCompare(zero,org2)) {
				p->org[0] = org[0] + ((rand()%30)-15)/2;
				p->org[1] = org[1] + ((rand()%30)-15)/2;
				p->org[2] = org[2] + ((rand()%30)-15)/2;
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}
			//make sure the particle is inside the world
			TraceLineN(org, p->org, stop, normal,0);
			if (Length(stop) != 0)
				VectorCopy(stop, p->org);

			//velocity
			p->vel[0] = ((rand()%10)-5)/20*dir[0];
			p->vel[1] = ((rand()%10)-5)/20*dir[1];
			p->vel[2] = ((rand()%10)-5)/20*dir[2];

			//smoke should rotate
			p->rotation_speed = (rand() & 31) + 32;
			p->rotation = rand()%90;
			break;

		case (p_blood):
			p->size = size * (rand()%20)/10;
			p->type = pm_grow;
			//pos
			if (VectorCompare(zero,org2)) {
				VectorCopy(org, p->org);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}

			//velocity
			p->vel[0] = (rand()%40)-20*dir[0];
			p->vel[1] = (rand()%40)-20*dir[1];
			p->vel[2] = (rand()%40)-20*dir[2];
			break;

		case (p_chunks):
			//pos
			if (VectorCompare(zero,org2)) {
				VectorCopy(org, p->org);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}
			//velocity
			p->vel[0] = (rand()%40)-20*dir[0];
			p->vel[1] = (rand()%40)-20*dir[1];
			p->vel[2] = (rand()%40)-5*dir[2];

			break;

		case (p_bubble):
			//pos
			if (VectorCompare(zero,org2)) {
				p->org[0] = org[0] + ((rand() & 31) - 16);
				p->org[1] = org[1] + ((rand() & 31) - 16);
				p->org[2] = org[2] + ((rand() & 63) - 32);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand()*dir[0];
				p->org[1] = org[1] + (org2[0] - org[1])*rand()*dir[1];
				p->org[2] = org[2] + (org2[0] - org[2])*rand()*dir[2];
			}
			//velocity
			p->vel[0] = 0;
			p->vel[1] = 0;
			p->vel[2] = 0;
			break;
		}

		p->decal_texture = 0;
		p->hit = 0;
		p->start = cl.time;
		p->die = cl.time + time;
		p->type = pm_static;

		if (pt->move == pm_die){
			VectorNegate(p->vel, p->acc);
			VectorScale(p->acc, 1/time, p->acc);
		} else {
			VectorCopy(zero, p->acc);
		}

		addGrav(pt, p);

		VectorCopy(colour, p->colour);

		{
			vec3_t	temp;

			VectorSubtract(p->org, r_origin, temp);
			p->dist = VectorNormalize(temp);	//code to work out distance from viewer
		}

		numParticles++;
	}
}

/** AddDecalColor
 * This is where it all happends, well not really this is where
 * most of the particles (and soon all) are added execpt for trails
 */
void AddDecalColor(vec3_t org, vec3_t normal, float size, float time, int texture, vec3_t colour)
{
	particle_t		*p;
	particle_tree_t	*pt;

	for (pt = particle_type_active; (pt) && (pt->id != p_decal); pt = pt->next);
	//find correct particle tree to add too...


	//get the next free particle off the list
	p = free_particles;
	//reset the next free particle to the next on the list
	free_particles = p->next;
	//add the particle to the correct one
	p->next = pt->start;
	pt->start = p;

	p->size = size;
	p->rotation_speed = 0;

	VectorCopy(org, p->org);	//set position
	VectorCopy(normal, p->vel);	//set direction

	p->hit = 1;					//turn off physics and tell renderer to draw it

	p->start = cl.time;			//current time
	p->die = cl.time + time;	//time it will have fully faded by
	p->type = pm_static;
	p->decal_texture = texture;	//set the texture

	VectorCopy(zero, p->acc);	//no acceleration
	VectorCopy(colour, p->colour);	//copy across colour

	//for particle sort code (currently disabled)
	{
		vec3_t	temp;

		VectorSubtract(p->org, r_origin, temp);
		p->dist = VectorNormalize(temp);	//code to work out distance from viewer
	}

	numParticles++;
}

/** AddFire
 * This is used for the particle fires
 * Somewhat reliant on framerate.
 */
//FIXME: will be replaced when emitters work right
void AddFire(vec3_t org, float size)
{
	particle_t		*p;
	particle_tree_t *pt;
	vec3_t			colour;
	int				i, count;


	if ((cl.time == cl.oldtime))
		return;

	for (pt = particle_type_active; (pt) && (pt->id != p_fire); pt = pt->next);

	count = 1;//(int)((cl.time-timepassed)*1.5);

	if (timetemp <= cl.oldtime){
		timepassed = cl.time;
		timetemp = cl.time;
	}

	colour[0] = 0.75f;
	colour[1] = 0.45f;
	colour[2] = 0.15f;

	for (i=0 ; (i<count)&&(free_particles) ; i++)
	{
		//get the next free particle off the list
		p = free_particles;
		//reset the next free particle to the next on the list
		free_particles = p->next;
		//add the particle to the correct one
		p->next = pt->start;
		pt->start = p;

		//pos
		VectorCopy(org, p->org);

		//velocity
		p->vel[0] = (rand()%30)-15;
		p->vel[1] = (rand()%30)-15;
		p->vel[2] = (rand()%8*size);

		VectorCopy(zero, p->acc);
		p->decal_texture = 0;

		if (pt->move == pm_die){
			VectorNegate(p->vel, p->acc);
			VectorScale(p->acc, 0.90f, p->acc);
		} else {
			VectorCopy(zero, p->acc);
		}

		p->hit = 0;
		p->start = cl.time - 0.5f;
		p->die = cl.time + 1;
		p->size = size;
		p->type = pm_shrink;
		VectorCopy(colour, p->colour);

		{
			vec3_t	temp;

			VectorSubtract(p->org, r_origin, temp);
			p->dist = VectorNormalize(temp);	//code to work out distance from viewer
		}

		numParticles++;
	}
}

/** AddTrail
 * Calls AddTrailColor with the default colours
 */
void AddTrail(vec3_t start, vec3_t end, int type, float time, float size, vec3_t dir)
{
	vec3_t colour;

	//colour set when first update called (check if needs to be fixed)
	switch (type)
	{
	case (p_smoke):
		colour[0] = colour[1] = colour[2] = (rand()%128)/255.0;
		break;
	case (p_sparks):
		colour[0] = 1;
		colour[1] = 0.5;
		colour[2] = 0;
		RGBtoGrayscalef(colour);
		break;
	case (p_fire):
		colour[0] = 0.75f;
		colour[1] = 0.45f;
		colour[2] = 0.15f;
		RGBtoGrayscalef(colour);
		break;
	case (p_blood):
		if (gl_sincity.getBool()){
			colour[0] = 0.9f;
		}else{
			colour[0] = 0.75f;
		}
		colour[1] = 0;
		colour[2] = 0;
		//colour[0] = colour[1] = colour[2] = 1.0f;
		break;
	case (p_chunks):
		colour[0] = colour[1] = colour[2] = (rand()%182)/255.0;
		RGBtoGrayscalef(colour);
	}
	AddTrailColor(start, end, type, time, size, colour, zerodir);
}

/** AddTrailColor
 * This will add a trail of particles of the specified type.
 * and return a lightning particle (for player movement updates)
 */
particle_t *AddTrailColor(vec3_t start, vec3_t end, int type, float time, float size, vec3_t color, vec3_t dir)
{
	particle_tree_t	*pt, *bubbles;
	particle_t		*p;
	int				i, typetemp, bubble = 0;
	double			count;
	vec3_t			point;
	double			frametime;
	vec3_t			StartVelocity;

	//work out vector for trail
	VectorSubtract(start, end, point);
	//work out the length and therefore the amount of particles
	count = Length(point);
	//make sure its at least 1 long
	//quater the amount of particles (speeds it up a bit)
	if (count == 0)
		return NULL;
	else
		count = count/8;

	//find correct particle tree to add too...
	if (type == p_smoke)
		typetemp = p_trail;
	else
		typetemp = type;

	//work out Velocity
	frametime = -1/(cl.time - cl.oldtime)/8;
	VectorMA(zerodir,frametime,point,StartVelocity);

	for (pt = particle_type_active; (pt) && (pt->id != typetemp); pt = pt->next);
	for (bubbles = particle_type_active; (bubbles) && (bubbles->id != p_bubble); bubbles = bubbles->next);

	if (((typetemp == p_trail)||(typetemp == p_lightning))&&(free_particles)&&(pt)){
		if (type == p_smoke)
		{
			//test to see if its in water
			switch (CL_TruePointContents(start)) {
			case CONTENTS_WATER:
			case CONTENTS_SLIME:
			case CONTENTS_LAVA:
				bubble = 1;
				break;
			default:	//not in water do it normally
				;
			}
		}

		if ((type == p_smoke && !bubble && gl_smoketrail.getBool() != 0) || type != p_smoke){
			p = free_particles;
			free_particles = p->next;
			p->next = pt->start;
			pt->start = p;

			VectorCopy(start, p->org);
			VectorCopy(end, p->org2);

			p->type = pm_static;
			p->die = cl.time + time;
			p->size = size;

			VectorCopy(color, p->colour);
			p->hit = 0;
			p->start = cl.time;
			p->vel[0] =	p->vel[1] = p->vel[2] = 0;

			{
				vec3_t	temp;

				VectorSubtract(p->org, r_origin, temp);
				p->dist = VectorNormalize(temp);	//code to work out distance from viewer
			}

			numParticles++;

			if (type != p_smoke || gl_smoketrail.getBool())
				return p;
		}
		for (pt = particle_type_active; (pt) && (pt->id != type); pt = pt->next);
	}

	//the vector from the current pos to the next particle
	VectorScale(point, 1.0/count, point);

	if ((pt)&&(bubbles))		//only need to test once....
		for (i=0 ; (i<count)&&(free_particles); i++)
		{
			//set the current particle to the next free particle
			p = free_particles;
			//reset the free particle pointer to the now first free particle
			free_particles = p->next;

			//work out the pos
			VectorMA (end, i, point, p->org);

			//make it a bit more random
			p->org[0] += ((rand()%16)-8)/4;
			p->org[1] += ((rand()%16)-8)/4;
			p->org[2] += ((rand()%16)-8)/4;

			if (type == p_smoke)
			{
				//test to see if its in water
				switch (CL_TruePointContents(p->org)) {
				case CONTENTS_WATER:
				case CONTENTS_SLIME:
				case CONTENTS_LAVA:
					//in water change smoke to bubbles
					p->next = bubbles->start;
					bubbles->start = p;
					bubble = 1;
					break;
				default:	//not in water do it normally
					p->next = pt->start;
					pt->start = p;
					bubble = 0;
				}
			}else{
				p->next = pt->start;
				pt->start = p;
			}

			//reset the particle vars
			p->hit = 0;
			p->start = cl.time;
			p->die = cl.time + time;
			p->decal_texture = 0;
			VectorCopy(color, p->colour);

			//small starting velocity
			if (VectorCompare(dir, zerodir)){
				p->vel[0]=((rand()%16)-8)/2;
				p->vel[1]=((rand()%16)-8)/2;
				p->vel[2]=((rand()%16)-8)/2;
			}else{
				p->vel[0]=dir[0] + ((rand()%16)-8)/16;
				p->vel[1]=dir[1] + ((rand()%16)-8)/16;
				p->vel[2]=dir[2] + ((rand()%16)-8)/16;
			}
			VectorAdd(p->vel,StartVelocity,p->vel);

			p->type = pm_static;
			p->size = size;
			p->rotation_speed = 0;

			//add the particle to the correct one
			switch (type)
			{
			case (p_sparks):
				//need bigger starting velocity (affected by grav)
				p->vel[0]=((rand()%32)-16)*2;
				p->vel[1]=((rand()%32)-16)*2;
				p->vel[2]=((rand()%32))*3;
				break;
			case (p_smoke):
				//smoke should rotate
				if (!bubble){
					p->rotation_speed = (rand() & 31) + 32;
					p->rotation = rand()%20;
					p->type = pm_grow;
					p->rotation_speed = (rand() & 31) + 32;
					p->rotation = rand()%90;
				} else {
					//velocity
					p->vel[0] = 0;
					p->vel[1] = 0;
					p->vel[2] = 0;
					break;
				}
				break;
			case (p_blood):
				p->size = size * (rand()%20)/10;
				p->type = pm_grow;
				p->start = cl.time - time * 0.5f;
				break;
			case (p_chunks):
			case (p_fire):
				break;
			}

			if (pt->move == pm_die){
				VectorNegate(p->vel, p->acc);
				VectorScale(p->acc, 1/time, p->acc);
			} else {
				VectorCopy(zero, p->acc);
			}

			addGrav(pt, p);
			if (bubble)
				addGrav(bubbles,p);

			{
				vec3_t	temp;

				VectorSubtract(p->org, r_origin, temp);
				p->dist = VectorNormalize(temp);	//code to work out distance from viewer
			}

			numParticles++;
		}

	return p;
}

//==================================================
//Particle texture code
//==================================================
int LoadParticleTexture (char *texture)
{
	return GL_LoadTexImage (texture, false, true, gl_sincity.getBool());
}

/** MakeParticleTexture
 * Makes the particle textures only 2 the
 * smoke texture (which could do with some work and
 * the others which is just a round circle.
 *
 * I should really make it an alpha texture which would save 32*32*3
 * bytes of space but *shrug* it works so i wont stuff with it
 */
void MakeParticleTexure(void)
{
    int i, j, k, centreX, centreY, separation, max;
    byte	data[128][128][4];

	//Normal texture (0->256 in a circle)
	//If you change this texture you will change the textures of
	//all particles except for smoke (other texture) and the
	//sparks which dont use textures
    for(j=0;j<128;j++){
        for(i=0;i<128;i++){
			data[i][j][0]	= 255;
			data[i][j][1]	= 255;
			data[i][j][2]	= 255;
            separation = (int) sqrt((64-i)*(64-i) + (64-j)*(64-j));
            if(separation<63)
                data[i][j][3] = 255 - separation * 256/63;
            else data[i][j][3] =0;
        }
    }
	//Load the texture into vid mem and save the number for later use
	part_tex = GL_LoadTexture ("particle", 128, 128, &data[0][0][0], true, false, 4, gl_sincity.getBool());

	//Clear the data
	max=64;
    for(j=0;j<128;j++){
        for(i=0;i<128;i++){
			data[i][j][0]	= 255;
			data[i][j][1]	= 255;
			data[i][j][2]	= 255;
			separation = (int) sqrt((i - 64)*(i - 64));
			data[i][j][3] = (max - separation)*2;
		}
    }

	//Add 128 random 4 unit circles
	for(k=0;k<128;k++){
		centreX = rand()%122+3;
		centreY = rand()%122+3;
		for(j=-3;j<3;j++){
			for(i=-3;i<3;i++){
				separation = (int) sqrt((i*i) + (j*j));
				if (separation <= 5)
					data[i+centreX][j+centreY][3]	+= (5-separation);
			}
	    }
	}
	trail_tex = GL_LoadTexture ("trail_part", 128, 128, &data[0][0][0], false, false, 4, gl_sincity.getBool());

	blood_tex = LoadParticleTexture	("textures/particles/blood");
	bubble_tex =LoadParticleTexture	("textures/particles/bubble");
	smoke_tex = LoadParticleTexture	("textures/particles/smoke");
	lightning_tex =LoadParticleTexture ("textures/particles/lightning");
	spark_tex = LoadParticleTexture ("textures/particles/spark");
}
//Yep thats all only 1597 lines of code, down a few hundred from the last system.