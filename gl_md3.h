/* gl_md3.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define MD3IDHEADER		(('3'<<24)+('P'<<16)+('D'<<8)+'I')

typedef float vec2_t[2];

//**** md3 disk format ****//
typedef struct
{
    short vec[3];
    short normal;
} md3vert_t;

typedef struct
{
	char	name[64];
	int		index;		//not used
} md3shader_t;

typedef struct
{
	float	s;
	float	t;
} md3st_t;

typedef struct
{
	int		tri[3];
} md3tri_t;

typedef struct
{
	char	id[4];
	char	name[64];
	int		flags;		//not used
	int		num_surf_frames;
	int		num_surf_shaders;
	int		num_surf_verts;
	int		num_surf_tris;
	int		tris_offs;
	int		shader_offs;
	int		tc_offs;
	int		vert_offs;
	int		end_offs;
} md3surface_t;

typedef struct
{
    char	name[64];
    vec3_t	pos;
    vec3_t	rot[3];
} md3tag_t;

typedef struct
{
    vec3_t		mins;
	vec3_t		maxs;
    vec3_t		pos;
    float		radius;
    char		name[16];
} md3frame_t;

typedef struct
{
	char	id[4];
	int		version;
	char	filename[64];
	int		flags;
	int		num_frames;
	int		num_tags;
	int		num_surfaces;
	int		num_skins;		//not used, older format thing?
	int		frame_offs;
	int		tag_offs;
	int		surface_offs;
	int		eof_offs;		//not used atm
} md3header_t;


//**** md3 memory format ****//

//different size
typedef struct
{
    vec3_t	vec;
	vec3_t	normal;
} md3vert_mem_t;

//same
typedef struct
{
	float	s;
	float	t;
} md3st_mem_t;

//same
typedef struct
{
	int		tri[3];
} md3tri_mem_t;

//extra
typedef struct
{
	char	name[64];
	int		index;		//not used
	int		texnum;
} md3shader_mem_t;

//diffrent
typedef struct md3surface_mem_s
{
	char	id[4];	//should be IDP3
	char	name[64];	//name of this surface
	int		flags;		//not used yet
	int		num_surf_frames;	//number of frames in this surface
	int		num_surf_shaders;	//number of shaders in this surface
	int		num_surf_verts;	//number of vertices in this surface
	int		num_surf_tris;	//number of triangles in this surface
//offsets to the verious parts
	int		offs_shaders;
	int		offs_tris;
	int		offs_tc;
	int		offs_verts;
	int		offs_end;
} md3surface_mem_t;

//same - not used yet
typedef struct
{
    char	name[64];
    vec3_t	pos;
    vec3_t	rot[3];
} md3tag_mem_t;

//same
typedef struct
{
    vec3_t		mins;
	vec3_t		maxs;
    vec3_t		pos;
    float		radius;
    char		name[16];
} md3frame_mem_t;

//same
typedef struct
{
	char	id[4];			//should be MDP3
	int		version;		//should be 15
	char	filename[64];	//the name inside md3
	int		flags;			//not used (will be for particle trails etc)
	int		num_frames;		//number of frames in file
	int		num_tags;		//number of tags
	int		num_surfaces;	//number of surfaces
	int		num_skins;		//old model format leftovers, not used
	//conver these to new offsets
	int		offs_frames;		//offset to frame
	int		offs_tags;			//offset to tags
	int		offs_surfaces;		//offset to surfaces
} md3header_mem_t;

//**** end ****//
void R_DrawQ3Model(entity_t *e, int shell, int outline);
void Mod_LoadQ3Model(model_t *mod, void *buffer);
extern int Q_strlcmp (char *s1, char *s2);
