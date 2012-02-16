/*************/
/***Defines***/
/*************/

//default max # of particles at one time
#define MAX_PARTICLES			16384
#define MAX_PARTICLE_TYPES		64
#define MAX_PARTICLE_EMITTER	128
//no fewer than this no matter what's on the command line
#define ABSOLUTE_MIN_PARTICLES	512

//types of particles
typedef enum {
	p_sparks, p_smoke, p_fire, p_blood, p_chunks, p_lightning, p_bubble, p_trail, p_decal, p_custom
} part_type_t;

//custom movement effects
typedef enum {
	pm_static, pm_normal, pm_float, pm_bounce, pm_bounce_fast, pm_shrink, pm_die, pm_grow, pm_nophysics, pm_decal
} part_move_t;

//gravity effects
typedef enum {
	pg_none,
	pg_grav_low, pg_grav_belownormal, pg_grav_normal, pg_grav_abovenormal, pg_grav_high,
	pg_rise_low, pg_rise, pg_rise_high
} part_grav_t;


//particle struct
typedef struct particle_s
{
	struct particle_s	*next;		//next particle in the list
	vec3_t		org;				//particles position (start pos for a trail part)
	vec3_t		org2;				//					 (end pos for a trail part)
	vec3_t		colour;				//float color (stops the need for looking up the 8bit to 24bit everytime
	float		rotation;			//current rotation
	float		rotation_speed;		//speed of rotation
	vec3_t		vel;				//particles current velocity
	vec3_t		acc;				//particles acceleration
	float		ramp;				//sort of diffrent purpose
	float		die;				//time that the particle will die
	float		start;				//start time of the particle
	int			hit;				//if the particle has hit the world (used for physics)
	float		size;				//size of the particle
	part_move_t	type;				//gravity for particle (Used for shrink and grow)
									//FIXME: change shrink/grow to a special particle type
	float		dist;				//distance from viewer
	int			decal_texture;
} particle_t;

//particle type struct
typedef struct particle_tree_s
{
	struct		particle_s	*start;		//start of this types particle list
	struct		particle_tree_s *next;	//next particle_type
	int			SrcBlend;				//source blend mode
	int			DstBlend;				//dest blend mode
	part_move_t	move;					//movement effect
	part_grav_t	grav;					//gravity
	part_type_t	id;						//type of particle (builtin or custom qc)
	int			custom_id;				//custom qc particle type id
	int			texture;				//texture particle uses
	float		startalpha;
} particle_tree_t;

//FIXME: should be linked to a model????
typedef struct particle_emitter_s
{
	struct		particle_emitter_s	*next;
	vec3_t		org;				//origin
	int			count;				//per second
	int			type;				//type of particles
	float		size;				//size of particles
	float		time;				//time before dieing
	vec3_t		colour;				//colour of the particles
	vec3_t		dir;				//directon (not used yet)
} particle_emitter_t;

//****New particle creations functions****

//FIXME: these should also accept a qc custom number....
//Add a trail of any particle type
void AddTrail(vec3_t start, vec3_t end, int type, float size, float time, vec3_t dir);
//Add a trail of any particle type and return the particle (for lightning linking up)
particle_t *AddTrailColor(vec3_t start, vec3_t end, int type, float size, float time, vec3_t colour, vec3_t dir);

//Add a particle or two
void AddParticle(vec3_t org, int count, float size, float time, int type, vec3_t dir);
void AddParticleColor(vec3_t org, vec3_t org2, int count, float size, float time, int type, vec3_t colour, vec3_t dir);

//Add a decal :D
void AddDecalColor(vec3_t org, vec3_t normal, float size, float time, int texture, vec3_t colour);

//Add a particle type
void AddParticleType(int src, int dst, part_move_t move, part_grav_t type, part_type_t id, int custom_id, int texture, float startalpha);

//FIXME: change to particle emitters
//Add fire particles
void AddFire(vec3_t org, float size);

//FIXME: add particle emitters here
extern int SV_HullPointContents(hull_t *hull, int num, vec3_t p);