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
// disable data conversion warnings

#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

// Different Linux OSes seem to put GLee in different spots
#if 1
// Fedora / Windows / Correct position based on GLee documentation
    #include <GL/GLee.h>
#else
// Ubuntu (thanks for being different guys)
    #include <GLee.h>
#endif

void checkGLError(const char *text);

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

extern	int		texture_mode;
extern	int		gl_textureunits;	//qmb :multitexture stuff
extern	int		glx, gly, glwidth, glheight;

#define BACKFACE_EPSILON	0.01

void R_TimeRefresh_f (void);
void R_LoadSky_f (void);		//for loading a new skybox during play
texture_t *R_TextureAnimation (texture_t *base);

//====================================================

extern	entity_t	r_worldentity;

extern	int			r_visframecount;	//used in pvs poly culling
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys; //poly counts for brush and alias models

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	CVar	r_drawentities;
extern	CVar	r_drawworld;
extern	CVar	r_drawviewmodel;
extern	CVar	r_speeds;
extern	CVar	r_wateralpha;
extern	CVar	r_dynamic;
extern	CVar	r_novis;
extern	CVar	gl_clear;
extern	CVar	gl_cull;
extern	CVar	gl_polyblend;
extern	CVar	gl_keeptjunctions;
extern	CVar	gl_flashblend;
extern	CVar	gl_nocolors;
extern  CVar	gl_detail;
extern  CVar	gl_shiny;
extern  CVar	gl_caustics;
extern  CVar	gl_dualwater;
extern  CVar	gl_ammoflash;
extern  CVar	r_interpolate_model_animation;
extern  CVar	r_interpolate_model_transform;
extern	CVar	r_wave;
extern	CVar	gl_fog;
extern	CVar	gl_fogglobal;
extern	CVar	gl_fogred;
extern	CVar	gl_foggreen;
extern	CVar	gl_fogblue;
extern	CVar	gl_fogstart;
extern	CVar	gl_fogend;
extern	CVar	gl_test;
extern	CVar	gl_conalpha;
extern	CVar	gl_checkleak;
extern	CVar	gl_max_size;
extern	CVar	gl_quick_texture_reload;
extern	CVar	r_skydetail;
extern	CVar	r_sky_x;
extern	CVar	r_sky_y;
extern	CVar	r_sky_z;
extern	CVar	r_errors;
extern	CVar	r_fullbright;
extern	CVar	r_modeltexture;
extern	CVar	r_celshading;
extern	CVar	r_vertexshading;
extern	CVar	gl_npatches;
extern	CVar	gl_anisotropic;
extern	CVar	r_outline;
extern	CVar	gl_24bitmaptex;
extern	CVar	gl_sincity;
extern	CVar	hostname;
extern	CVar	sv_stepheight;
extern	CVar	sv_jumpstep;

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

void R_TranslatePlayerSkin (int playernum);

extern bool gl_combine;
extern bool gl_stencil;
extern bool gl_shader;
extern bool gl_sgis_mipmap;
extern bool gl_texture_non_power_of_two;
extern bool gl_point_sprite;
extern bool gl_n_patches;

extern void GL_EnableTMU(int tmu);
extern void GL_DisableTMU(int tmu);

void   Mod_LoadMd3Model (model_t *mod, void *buffer);
