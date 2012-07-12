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

#include <GL/GLee.h>

void checkGLError(const char *text);

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

extern	int		texture_mode;
extern	int		gl_textureunits;	//qmb :multitexture stuff

void	GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap, qboolean grayscale);
int		GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean fullbright);
int		GL_LoadTexture (const char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean fullbright, int bytesperpixel, qboolean grayscale);
int		GL_LoadTexImage (char *filename, qboolean complain, qboolean mipmap, qboolean grayscale);

extern	int glx, gly, glwidth, glheight;

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
extern	texture_t	*r_notexture_mip;
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

extern	int		gl_lightmap_format;

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

void R_TranslatePlayerSkin (int playernum);

// Multitexture
#define GL_MAX_TEXTURE_UNITS_ARB			0x84E2

#define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF

//QMB :overbright stuff
#define GL_COMBINE_EXT						0x8570
#define GL_COMBINE_RGB_EXT					0x8571
#define GL_RGB_SCALE_EXT					0x8573
#define GL_ADD_SIGNED						0x8574
#define GL_SUBTRACT							0x84E7
//QMB :end

//QMB :texture shaders
#define GL_DSDT8_NV							0x8709
#define GL_DSDT_NV							0x86F5
#define GL_TEXTURE_SHADER_NV				0x86DE
#define GL_SHADER_OPERATION_NV				0x86DF
#define GL_OFFSET_TEXTURE_2D_NV				0x86E8
#define GL_PREVIOUS_TEXTURE_INPUT_NV		0x86E4
#define GL_OFFSET_TEXTURE_MATRIX_NV			0x86E1
//QMB :end

//QMB :texture compression
#ifndef GL_ARB_texture_compression
#define GL_COMPRESSED_ALPHA_ARB				0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB			0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB	0x84EB
#define GL_COMPRESSED_INTENSITY_ARB			0x84EC
#define GL_COMPRESSED_RGB_ARB				0x84ED
#define GL_COMPRESSED_RGBA_ARB				0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB		0x84EF
#define GL_TEXTURE_IMAGE_SIZE_ARB			0x86A0
#define GL_TEXTURE_COMPRESSED_ARB			0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB	0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB	0x86A3
#endif
//QMB :end

//QMB :SGIS_generate_mipmap
#define GL_GENERATE_MIPMAP_SGIS				0x8191
//QMB :end

//QMB :NV_point_sprite
#define GL_POINT_SPRITE_NV					0x8861
#define GL_COORD_REPLACE_NV					0x8862
#define GL_POINT_SPRITE_R_MODE_NV			0x8863
//QMB :end

//QMB :points
#ifndef GL_SGIS_point_parameters
#define GL_POINT_SIZE_MIN_EXT				0x8126
#define GL_POINT_SIZE_MIN_SGIS				0x8126
#define GL_POINT_SIZE_MAX_EXT				0x8127
#define GL_POINT_SIZE_MAX_SGIS				0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_EXT	0x8128
#define GL_POINT_FADE_THRESHOLD_SIZE_SGIS	0x8128
#define GL_DISTANCE_ATTENUATION_EXT			0x8129
#define GL_DISTANCE_ATTENUATION_SGIS		0x8129
#endif
//QMB :end

//QMB :npatches
#ifndef GL_ATI_pn_triangles
#define GL_ATI_pn_triangles 1

#define GL_PN_TRIANGLES_ATI							0x87F0
#define GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI	0x87F1
#define GL_PN_TRIANGLES_POINT_MODE_ATI				0x87F2
#define GL_PN_TRIANGLES_NORMAL_MODE_ATI				0x87F3
#define GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI		0x87F4
#define GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI		0x87F5
#define GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI		0x87F6
#define GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI		0x87F7
#define GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI   0x87F8
#endif
//QMB :end

//QMB :NV_point_sprite
typedef void (APIENTRY *pointPramFUNCv) (GLenum pname, const GLfloat *params);
typedef void (APIENTRY *pointPramFUNC) (GLenum pname, const GLfloat params);
//QMB :npatches
typedef void (APIENTRY *pnTrianglesIatiPROC)(GLenum pname, GLint param);
typedef void (APIENTRY *pnTrianglesFaitPROC)(GLenum pname, GLfloat param);

extern pointPramFUNCv qglPointParameterfvEXT;
extern pointPramFUNC qglPointParameterfEXT;
extern pnTrianglesIatiPROC glPNTrianglesiATI;

extern qboolean gl_combine;
extern qboolean gl_stencil;
extern qboolean gl_shader;
extern qboolean gl_sgis_mipmap;
extern qboolean gl_texture_non_power_of_two;
extern qboolean gl_point_sprite;
extern qboolean gl_n_patches;

extern void GL_EnableTMU(int tmu);
extern void GL_DisableTMU(int tmu);

void   Mod_LoadMd3Model (model_t *mod, void *buffer);