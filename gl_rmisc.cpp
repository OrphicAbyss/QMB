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
#include "quakedef.h"
#include "Texture.h"

void R_Init(void) {
	extern CVar gl_finish;

	Cmd::addCmd("timerefresh", R_TimeRefresh_f);
	Cmd::addCmd("loadsky", R_LoadSky_f);
	Cmd::addCmd("currentcoord", R_CurrentCoord_f);

	CVar::registerCVar(&r_drawentities);
	CVar::registerCVar(&r_drawviewmodel);
	CVar::registerCVar(&r_wateralpha);
	CVar::registerCVar(&r_dynamic);
	CVar::registerCVar(&r_novis);
	CVar::registerCVar(&r_speeds);
	CVar::registerCVar(&gl_finish);
	CVar::registerCVar(&gl_clear);
	CVar::registerCVar(&gl_cull);
	CVar::registerCVar(&gl_polyblend);
	CVar::registerCVar(&gl_flashblend);
	CVar::registerCVar(&gl_nocolors);
	CVar::registerCVar(&gl_keeptjunctions);
	CVar::registerCVar(&gl_detail);
	CVar::registerCVar(&gl_shiny);
	CVar::registerCVar(&gl_caustics);
	CVar::registerCVar(&gl_dualwater);
	CVar::registerCVar(&gl_ammoflash);
	CVar::registerCVar(&r_interpolate_model_animation);
	CVar::registerCVar(&r_interpolate_model_transform);
	CVar::registerCVar(&r_wave);
	CVar::registerCVar(&gl_fog);
	CVar::registerCVar(&gl_fogglobal);
	CVar::registerCVar(&gl_fogred);
	CVar::registerCVar(&gl_foggreen);
	CVar::registerCVar(&gl_fogblue);
	CVar::registerCVar(&gl_fogstart);
	CVar::registerCVar(&gl_fogend);
	CVar::registerCVar(&gl_conalpha);
	CVar::registerCVar(&gl_test);
	CVar::registerCVar(&gl_checkleak);
	CVar::registerCVar(&r_skydetail);
	CVar::registerCVar(&r_sky_x);
	CVar::registerCVar(&r_sky_y);
	CVar::registerCVar(&r_sky_z);
	CVar::registerCVar(&r_errors);
	CVar::registerCVar(&r_fullbright);
	CVar::registerCVar(&r_modeltexture);
	CVar::registerCVar(&r_celshading);
	CVar::registerCVar(&r_outline);
	CVar::registerCVar(&r_vertexshading);
	CVar::registerCVar(&gl_npatches);
	CVar::registerCVar(&gl_anisotropic);
	CVar::registerCVar(&gl_24bitmaptex);

	R_InitParticles();
}

/**
 * Translates a skin texture by the per-player color lookup
 */
void R_TranslatePlayerSkin(int playernum) {
	int top, bottom;
	byte translate[256];
	unsigned translate32[256];
	unsigned int i;

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors & 15) << 4;

	for (i = 0; i < 256; i++)
		translate[i] = i;

	for (i = 0; i < 16; i++) {
		if (top < 128) // the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE + i] = top + i;
		else
			translate[TOP_RANGE + i] = top + 15 - i;

		if (bottom < 128)
			translate[BOTTOM_RANGE + i] = bottom + i;
		else
			translate[BOTTOM_RANGE + i] = bottom + 15 - i;
	}

	for (i = 0; i < 256; i++)
		translate32[i] = d_8to24table[translate[i]];
}

void R_NewMap(void) {
	for (int i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264; // normal light value

	memset(&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.model = cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (int i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;

	R_ClearParticles();
	R_ClearBeams();

	GL_BuildLightmaps();
}

/**
 * For program optimization
 */
void R_TimeRefresh_f(void) {
	int i;
	float start, stop, time;

	glDrawBuffer(GL_FRONT);
	glFinish();

	start = Sys_FloatTime();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i / 128.0 * 360.0;
		R_RenderView();
	}

	glFinish();
	stop = Sys_FloatTime();
	time = stop - start;
	Con_Printf("%f seconds (%f fps)\n", time, 128 / time);

	glDrawBuffer(GL_BACK);
	GL_EndRendering();
}
