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
// r_main.c

#include "quakedef.h"

entity_t	r_worldentity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

//int			particletexture;	// little dot for particles
int			shinetex_glass, shinetex_chrome, underwatertexture, highlighttexture;
int			crosshair_tex[32];

int			playertextures;		// up to 16 color translated skins

// Used by depth hack
float		gldepthmin, gldepthmax;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

//qmb :texture units
int		gl_textureunits;

void R_MarkLeaves (void);

cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_wateralpha = {"r_wateralpha","1", true};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0", true};

cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_clear = {"gl_clear","1", true};
cvar_t	gl_24bitmaptex = {"gl_24bitmaptex","1",true};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_flashblend = {"gl_flashblend","0", true};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","0"};

//qmb :extra cvars
cvar_t	gl_detail = {"gl_detail","1", true};
cvar_t	gl_shiny = {"gl_shiny","1", true};
cvar_t	gl_caustics = {"gl_caustics","1", true};
cvar_t	gl_dualwater = {"gl_dualwater","1", true};
cvar_t	gl_ammoflash = {"gl_ammoflash","1", true};
// fenix@io.com: model interpolation
cvar_t  r_interpolate_model_animation = { "r_interpolate_model_animation", "1", true };
cvar_t  r_interpolate_model_transform = { "r_interpolate_model_transform", "1", true };
cvar_t  r_wave = {"r_wave", "1", true};
cvar_t  gl_fog = {"gl_fog", "1", true};
cvar_t  gl_fogglobal = {"gl_fogglobal", "1", true};
cvar_t  gl_fogred = {"gl_fogred", "0.5", true};
cvar_t  gl_foggreen = {"gl_foggreen", "0.4", true};
cvar_t  gl_fogblue = {"gl_fogblue", "0.3", true};
cvar_t  gl_fogstart = {"gl_fogstart", "150", true};
cvar_t  gl_fogend = {"gl_fogend", "1536", true};
cvar_t  sv_fastswitch = {"sv_fastswitch", "0", true};

cvar_t  gl_conalpha = {"gl_conalpha", "0.5", true};
cvar_t	gl_checkleak = {"gl_checkleak","0", true};
cvar_t	r_skydetail = {"r_skydetail","1", true};
cvar_t	r_sky_x = {"r_sky_x","0", true};
cvar_t	r_sky_y = {"r_sky_y","0", true};
cvar_t	r_sky_z = {"r_sky_z","0", true};

cvar_t	r_errors = {"r_errors","1", true};
cvar_t	r_fullbright = {"r_fullbright", "0"};

//cel shading & vertex shading in model
cvar_t	r_modeltexture = {"r_modeltexture","1", true};
cvar_t	r_celshading = {"r_celshading","0", true};
cvar_t	r_outline = {"r_outline","0", true};
cvar_t	r_vertexshading = {"r_vertexshading","0", true};

//ati truform
cvar_t	gl_npatches = {"gl_npatches","1", true};

//anisotropy filtering
cvar_t	gl_anisotropic = {"gl_anisotropic","2", true};

//temp var for internal testing of new features
cvar_t  gl_test = {"gl_temp", "1", true};
//qmb :end
//qmb :gamma
extern	cvar_t		v_gamma; // muff

// idea originally nicked from LordHavoc
// re-worked + extended - muff 5 Feb 2001
// called inside polyblend
void DoGamma()
{
	//return;
	if (v_gamma.value < 0.2f)
		v_gamma.value = 0.2f;
	if (v_gamma.value >= 1)
	{
		v_gamma.value = 1;
		return;
	}

	//believe it or not this actually does brighten the picture!!
	glBlendFunc (  GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f (1, 1, 1, v_gamma.value);


	glBegin (GL_QUADS);
	glVertex3f (10, 100, 100);
	glVertex3f (10, -100, 100);
	glVertex3f (10, -100, -100);
	glVertex3f (10, 100, -100);

	//if we do this twice, we double the brightening effect for a wider range of gamma's
	glVertex3f (11, 100, 100);
	glVertex3f (11, -100, 100);
	glVertex3f (11, -100, -100);
	glVertex3f (11, 100, -100);

	glEnd ();

}
//qmb :end

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

void R_RotateForEntity (entity_t *e)
{
    glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    glRotatef (e->angles[1],  0, 0, 1);
	glRotatef (-e->angles[0],  0, 1, 0);
	glRotatef (e->angles[2],  1, 0, 0);
}


/*
=============
R_BlendedRotateForEntity

fenix@io.com: model transform interpolation
=============
*/
void R_BlendedRotateForEntity (entity_t *e)
{
	float timepassed; //JHL:FIX; for checking passed time
	float blend;
	vec3_t d;
	int i;

	timepassed = realtime - e->translate_start_time;

	// positional interpolation
	if (e->translate_start_time == 0 || timepassed > 0.5)
	{
		e->translate_start_time = realtime;
		VectorCopy (e->origin, e->origin1);
		VectorCopy (e->origin, e->origin2);
	}

	if (!VectorCompare (e->origin, e->origin2))
	{
		e->translate_start_time = realtime;
		VectorCopy (e->origin2, e->origin1);
		VectorCopy (e->origin,  e->origin2);
		blend = 0;
	}
	else
	{
		blend =  timepassed / 0.1;

		if (cl.paused || blend > 1)
			blend = 1;
	}

	VectorSubtract (e->origin2, e->origin1, d);
	glTranslatef (e->origin1[0] + (blend * d[0]), e->origin1[1] + (blend * d[1]), e->origin1[2] + (blend * d[2]));

	// orientation interpolation (Euler angles, yuck!)
	timepassed = realtime - e->rotate_start_time;

	if (e->rotate_start_time == 0 || timepassed > 0.5)
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles, e->angles1);
		VectorCopy (e->angles, e->angles2);
	}

	if (!VectorCompare (e->angles, e->angles2))
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles2, e->angles1);
		VectorCopy (e->angles,  e->angles2);
		blend = 0;
	}
	else
	{
		blend =  timepassed / 0.1;

		if (cl.paused || blend > 1)
			blend = 1;
	}

	VectorSubtract (e->angles2, e->angles1, d);

	// always interpolate along the shortest path
	for (i = 0; i < 3; i++)
	{
		if (d[i] > 180)
			d[i] -= 360;
		else if (d[i] < -180)
			d[i] += 360;
	}

	glRotatef ( e->angles1[1] + ( blend * d[1]),  0, 0, 1);
	glRotatef (-e->angles1[0] + (-blend * d[0]),  0, 1, 0);
	glRotatef ( e->angles1[2] + ( blend * d[2]),  1, 0, 0);
}



/*
=============
R_DrawEntitiesOnList
=============
*/

//QMB: will fix this function to go through for the alias models and for the brushes so optimize for rendering
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	// draw brushes
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		if (cl_visedicts[i]->model->type == mod_brush)
		{
			R_DrawBrushModel (cl_visedicts[i]);
		}
	}

	// draw models
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		if (cl_visedicts[i]->model->type == mod_alias)
		{
			R_DrawAliasModel (cl_visedicts[i]);
		}
	}

	// draw sprites
	for (i=0 ; i<cl_numvisedicts ; i++)
	{

		if (cl_visedicts[i]->model->type == mod_sprite)
		{
			R_DrawSpriteModel (cl_visedicts[i]);
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/


void R_DrawViewModel (void)
{
	entity_t	*e;

    float old_interpolate_model_transform;

    if (!r_drawviewmodel.value)
        return;

    if (chase_active.value) //make sure we are in 1st person view
        return;

    if (!r_drawentities.value) //make sure we are drawing entities
        return;

    if (cl.items & IT_INVISIBILITY) //make sure we arnt invisable
        return;

    if (cl.stats[STAT_HEALTH] <= 0)	//make sure we arnt dead
        return;

    e = &cl.viewent;
    if (!e->model)	//make sure we have a model to draw
        return;

    // hack the depth range to prevent view model from poking into walls
    glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	//qmb :interpolation
	// fenix@io.com: model transform interpolation
	old_interpolate_model_transform = r_interpolate_model_transform.value;
	r_interpolate_model_transform.value = false;
	R_DrawAliasModel (e);
	r_interpolate_model_transform.value = old_interpolate_model_transform;
	//qmb :end
    glDepthRange (gldepthmin, gldepthmax);
}

/*
============
R_PolyBlend
============
*/
//qmb :gamma
//some code from LordHavoc early DP version
void R_PolyBlend (void)
{
	if (!gl_polyblend.value)
		return;

	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_TEXTURE_2D);

	glLoadIdentity ();

	glRotatef (-90,  1, 0, 0);	    // put Z going up
	glRotatef (90,  0, 0, 1);	    // put Z going up

	glColor4fv (v_blend);

	glBegin (GL_QUADS);

	glVertex3f (10, 100, 100);
	glVertex3f (10, -100, 100);
	glVertex3f (10, -100, -100);
	glVertex3f (10, 100, -100);
	glEnd ();

	//gamma trick based on LordHavoc - muff
	if (v_gamma.value != 1)
		DoGamma();

	glDisable (GL_BLEND); // muff
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_DEPTH_TEST);
	glEnable (GL_ALPHA_TEST); //muff
}

int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90)
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2));
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2);
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2);
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -(90 - r_refdef.fov_y / 2));
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	c_brush_polys = 0;
	c_alias_polys = 0;

}

void qGluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	gluPerspective(fovy, aspect, zNear, zFar);
/*
   GLdouble xmin, xmax, ymin, ymax;

   ymax = zNear * tan(fovy * M_PI / 360.0);
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;

   glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
*/
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

	//
	// set up viewpoint
	//

	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	glViewport (glx + x, gly + y2, w, h);
    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	//gluPerspective (r_refdef.fov_y, screenaspect, 4, 4096);
	qGluPerspective(r_refdef.fov_y, screenaspect, 4, 4096);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
    glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	//
	// set drawing parms
	//
/*
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
*/
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	GLenum error;

	if (r_errors.value && developer.value)
		checkGLError("Error pre-rendering:");

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	if (r_errors.value && developer.value)
		checkGLError("After R_SetupGL:");

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	if (r_errors.value && developer.value)
		checkGLError("After R_DrawWorld:");

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	if (r_errors.value && developer.value)
		checkGLError("After R_DrawEntitiesOnList:");

	R_RenderDlights ();

	R_DrawParticles ();

	if (r_errors.value && developer.value)
		checkGLError("After R_DrawParticles:");

	R_DrawViewModel ();

	if (r_errors.value && developer.value)
		checkGLError("After R_DrawViewModel:");
}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	//qmb :map leak check
	if (gl_checkleak.value){
		gl_clear.value = 1;
		glClearColor (1,0,0,1);
	}else
		glClearColor (0,0,0,0);

	if (gl_stencil){
		glClearStencil(1);

		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);	//stencil bit ignored when no stencil buffer
		else
			glClear (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);	//stencil bit ignored when no stencil buffer
	}else {
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	//stencil bit ignored when no stencil buffer
		else
			glClear (GL_DEPTH_BUFFER_BIT);	//stencil bit ignored when no stencil buffer
	}

	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	GLenum error;
	double	time1, time2;
	float colors[4] = {0.0, 0.0, 0.0, 1.0};

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish ();
		time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish.value)
		glFinish ();

	if (r_errors.value && developer.value)
		checkGLError("Finished:");

	R_Clear ();

	if (r_errors.value && developer.value)
		checkGLError("After R_Clear:");

	// render normal view

	if ((gl_fogglobal.value))//&&(CONTENTS_EMPTY==r_viewleaf->contents))
	{
		glFogi(GL_FOG_MODE, GL_LINEAR);
			colors[0] = gl_fogred.value;
			colors[1] = gl_foggreen.value;
			colors[2] = gl_fogblue.value;
		glFogfv(GL_FOG_COLOR, colors);
		glFogf(GL_FOG_START, gl_fogstart.value);
		glFogf(GL_FOG_END, gl_fogend.value);
		glFogf(GL_FOG_DENSITY, 0.2f);
		glEnable(GL_FOG);

		if (r_errors.value && developer.value)
			checkGLError("After fog setup:");
	}

	R_RenderScene ();

	if (gl_fogglobal.value)
		glDisable(GL_FOG);

	R_PolyBlend ();

	if (r_speeds.value)
	{
		time2 = Sys_FloatTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys);
	}

	if (r_errors.value && developer.value)
		checkGLError("After R_RenderScene and R_PolyBlend:");
}
