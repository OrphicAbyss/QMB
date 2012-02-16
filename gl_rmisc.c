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
// r_misc.c

#include "quakedef.h"

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;
	byte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16/*+8*8+4*4+2*2*/, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}

unsigned int celtexture = 0;
unsigned int vertextexture = 0;

void R_InitOtherTextures (void)
{
	float	cellData[32] = {0.2f,0.2f,0.2f,0.2f,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0};
	float	cellFull[32][3];
	float	vertexFull[32][3];
	int		i, found;
	char	*crossfound[32];

	extern int crosshair_tex[32];

	shinetex_glass = GL_LoadTexImage ("textures/shine_glass", false, true, false);
	shinetex_chrome = GL_LoadTexImage ("textures/shine_chrome", false, true, false);
	underwatertexture = GL_LoadTexImage ("textures/water_caustic", false, true, false);
	highlighttexture = GL_LoadTexImage ("gfx/highlight", false, true, false);

	found = COM_MultipleSearch("textures/crosshairs/*", crossfound, 32);

	Con_Printf("\nFound %i crosshairs.\n", found);

	for (i=0;i<found && i<32;i++)
	{
		Con_DPrintf("%i. %s\n",i,crossfound[i]);
		crosshair_tex[i] = GL_LoadTexImage (crossfound[i], false, false, false);
		Z_Free(crossfound[i]);
	}
	Con_Printf("\n");

	for (i=0;i<32;i++)
		cellFull[i][0] = cellFull[i][1] = cellFull[i][2] = cellData[i];

	for (i=0;i<32;i++)
		vertexFull[i][0] = vertexFull[i][1] = vertexFull[i][2] = (i/32.0f);

	//cell shading stuff...
	glGenTextures (1, &celtexture);			// Get A Free Texture ID
	glBindTexture (GL_TEXTURE_1D, celtexture);	// Bind This Texture. From Now On It Will Be 1D
	// For Crying Out Loud Don't Let OpenGL Use Bi/Trilinear Filtering!
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// Upload
	glTexImage1D (GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGB , GL_FLOAT, cellFull);

	//vertex shading stuff...
	glGenTextures (1, &vertextexture);			// Get A Free Texture ID
	glBindTexture (GL_TEXTURE_1D, vertextexture);	// Bind This Texture. From Now On It Will Be 1D
	// For Crying Out Loud Don't Let OpenGL Use Bi/Trilinear Filtering!
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// Upload
	glTexImage1D (GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGB , GL_FLOAT, vertexFull);

}

/*
===============
R_Init
===============
*/
void R_Init (void)
{	
	extern byte *hunk_base;
	extern cvar_t gl_finish;

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand ("loadsky", R_LoadSky_f);
	Cmd_AddCommand ("currentcoord", R_CurrentCoord_f);

	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_shadows);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_novis);
	Cvar_RegisterVariable (&r_speeds);

	Cvar_RegisterVariable (&gl_finish);
	Cvar_RegisterVariable (&gl_clear);

	Cvar_RegisterVariable (&gl_cull);
	Cvar_RegisterVariable (&gl_polyblend);
	Cvar_RegisterVariable (&gl_flashblend);
	Cvar_RegisterVariable (&gl_nocolors);

	Cvar_RegisterVariable (&gl_keeptjunctions);

	//qmb :extra cvars
	Cvar_RegisterVariable (&gl_detail);
	Cvar_RegisterVariable (&gl_shiny);
	Cvar_RegisterVariable (&gl_caustics);
	Cvar_RegisterVariable (&gl_dualwater);
	Cvar_RegisterVariable (&gl_ammoflash);
	// fenix@io.com: register new cvars for model interpolation
	Cvar_RegisterVariable (&r_interpolate_model_animation);
	Cvar_RegisterVariable (&r_interpolate_model_transform);
	Cvar_RegisterVariable (&r_wave);
	Cvar_RegisterVariable (&gl_fog);
	Cvar_RegisterVariable (&gl_fogglobal);
	Cvar_RegisterVariable (&gl_fogred);
	Cvar_RegisterVariable (&gl_foggreen);
	Cvar_RegisterVariable (&gl_fogblue);
	Cvar_RegisterVariable (&gl_fogstart);
	Cvar_RegisterVariable (&gl_fogend);
	Cvar_RegisterVariable (&gl_conalpha);
	Cvar_RegisterVariable (&gl_test);
	Cvar_RegisterVariable (&gl_checkleak);
	Cvar_RegisterVariable (&r_skydetail);
	Cvar_RegisterVariable (&r_sky_x);
	Cvar_RegisterVariable (&r_sky_y);
	Cvar_RegisterVariable (&r_sky_z);

	Cvar_RegisterVariable (&r_errors);
	Cvar_RegisterVariable (&r_fullbright);

	Cvar_RegisterVariable (&r_modeltexture);
	Cvar_RegisterVariable (&r_celshading);
	Cvar_RegisterVariable (&r_outline);
	Cvar_RegisterVariable (&r_vertexshading);
	Cvar_RegisterVariable (&gl_npatches);

	Cvar_RegisterVariable (&gl_anisotropic);

	Cvar_RegisterVariable (&gl_24bitmaptex);
	//qmb :end

	R_InitParticles ();
	//qmb :no longer used
	//R_InitParticleTexture ();
	R_InitTextures ();
	R_InitOtherTextures ();

	playertextures = texture_extension_number;
	texture_extension_number += MAX_SCOREBOARD;
}

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void R_TranslatePlayerSkin (int playernum)
{
	int		top, bottom;
	byte	translate[256];
	unsigned	translate32[256];
	unsigned int		i;

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors &15)<<4;

	for (i=0 ; i<256 ; i++)
		translate[i] = i;

	for (i=0 ; i<16 ; i++)
	{
		if (top < 128)	// the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE+i] = top+i;
		else
			translate[TOP_RANGE+i] = top+15-i;
				
		if (bottom < 128)
			translate[BOTTOM_RANGE+i] = bottom+i;
		else
			translate[BOTTOM_RANGE+i] = bottom+15-i;
	}


	for (i=0 ; i<256 ; i++)
		translate32[i] = d_8to24table[translate[i]];
}


/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int		i;
	
	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;
		 	
	r_viewleaf = NULL;

	R_ClearParticles ();
	R_ClearBeams ();

	GL_BuildLightmaps ();

	//qmb :skybox
	//R_LoadSky ("sky");
}


/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;

	glDrawBuffer  (GL_FRONT);
	glFinish ();

	start = Sys_FloatTime ();
	for (i=0 ; i<128 ; i++)
	{
		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
	}

	glFinish ();
	stop = Sys_FloatTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);

	glDrawBuffer  (GL_BACK);
	GL_EndRendering ();
}
