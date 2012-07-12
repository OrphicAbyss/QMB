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

void R_InitTextures(void) {
	// create a simple checkerboard texture for the default
	r_notexture_mip = (texture_t *) Hunk_AllocName(sizeof (texture_t) + 16 * 16/*+8*8+4*4+2*2*/, "notexture");
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof (texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (int m = 0; m < 4; m++) {
		byte *dest = (byte *) r_notexture_mip + r_notexture_mip->offsets[m];
		for (int y = 0; y < (16 >> m); y++)
			for (int x = 0; x < (16 >> m); x++) {
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}

void R_InitOtherTextures(void) {
	unsigned char cellData[32] = {55, 55, 55, 55, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
	unsigned char *cellFull = (unsigned char *)malloc(sizeof(unsigned char) * 32 * 3);
	unsigned char *vertexFull = (unsigned char *)malloc(sizeof(unsigned char) * 32 * 3);
	char crosshairFname[32];
	
	TextureManager::shinetex_glass = GL_LoadTexImage("textures/shine_glass", false, true, false);
	TextureManager::shinetex_chrome = GL_LoadTexImage("textures/shine_chrome", false, true, false);
	TextureManager::underwatertexture = GL_LoadTexImage("textures/water_caustic", false, true, false);
	TextureManager::highlighttexture = GL_LoadTexImage("gfx/highlight", false, true, false);

	for (int i = 0; i < 32; i++) {
		snprintf(&crosshairFname[0],32,"textures/crosshairs/crosshair%02i.tga",i);
		TextureManager::crosshair_tex[i] = GL_LoadTexImage((char *)&crosshairFname[0], false, false, false);
	}

	for (int i = 0; i < 32; i++) {
		cellFull[i * 3 + 0] = cellFull[i * 3 + 1] = cellFull[i * 3 + 2] = cellData[i];
		vertexFull[i * 3 + 0] = vertexFull[i * 3 + 1] = vertexFull[i * 3 + 2] = (unsigned char)((i / 32.0f) * 255);
	}	

	//cell shading stuff...
	Texture *cell = new Texture("celTexture");
	cell->data = cellFull;
	cell->height = 1;
	cell->width = 32;
	cell->bytesPerPixel = 3;
	cell->mipmap = false;
	cell->textureType = GL_TEXTURE_1D;
	TextureManager::LoadTexture(cell);
	TextureManager::celtexture = cell->textureId;

	//vertex shading stuff...
	Texture *vertex = new Texture("vertexTexture");
	vertex->data = vertexFull;
	vertex->height = 1;
	vertex->width = 32;
	vertex->bytesPerPixel = 3;
	vertex->mipmap = false;
	vertex->textureType = GL_TEXTURE_1D;
	TextureManager::LoadTexture(vertex);
	TextureManager::vertextexture = vertex->textureId;
}

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
	//R_InitParticleTexture();
	R_InitTextures();
	R_InitOtherTextures();
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
	int i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264; // normal light value

	Q_memset(&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.model = cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;

	R_ClearParticles();
	R_ClearBeams();

	GL_BuildLightmaps();

	//R_LoadSky ("sky");
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
