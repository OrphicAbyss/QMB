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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"
#include "Texture.h"

extern model_t *loadmodel;

char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
int skytexorder[6] = {3, 1, 0, 2, 4, 5};
int skytex[6];

char skyname[32];
model_t *skymodel;

int oldsky = true;
int R_Skybox = false;

msurface_t *warpface;

extern CVar gl_subdivide_size;

void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs) {
	float *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (int i = 0; i < numverts; i++)
		for (int j = 0; j < 3; j++, v++) {
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon(int numverts, float *verts) {
	vec3_t mins, maxs;
	vec3_t front[64], back[64];
	float dist[64];
	float frac;
	glpoly_t *poly;

	if (numverts > 60)
		Sys_Error("numverts = %i", numverts);

	BoundPoly(numverts, verts, mins, maxs);

	for (int i = 0; i < 3; i++) {
		float mid = (mins[i] + maxs[i]) * 0.5;
		mid = gl_subdivide_size.getFloat() * floor(mid / gl_subdivide_size.getFloat() + 0.5);
		if (maxs[i] - mid < 8)
			continue;
		if (mid - mins[i] < 8)
			continue;

		// cut it
		float *v = verts + i;
		for (int j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - mid;

		// wrap cases
		dist[numverts] = dist[0];
		v -= i;
		VectorCopy(verts, v);

		int f = 0, b = 0;
		v = verts;
		for (int j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy(v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy(v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0)) {
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (int k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
		return;
	}

	poly = (glpoly_t *) Hunk_Alloc(sizeof (glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof (float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (int i = 0; i < numverts; i++, verts += 3) {
		VectorCopy(verts, poly->verts[i]);
		poly->verts[i][3] = DotProduct(verts, warpface->texinfo->vecs[0]);
		poly->verts[i][4] = DotProduct(verts, warpface->texinfo->vecs[1]);
	}
}

/**
 * Breaks a polygon up along axial 64 unit boundaries so that turbulent and sky
 *  warps can be done reasonably.
 */
void GL_SubdivideSurface(msurface_t *fa) {
	vec3_t verts[64];
	float *vec;

	warpface = fa;

	// convert edges back to a normal polygon
	int numverts = 0;
	for (int i = 0; i < fa->numedges; i++) {
		int lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy(vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon(numverts, verts[0]);
}

//=========================================================

// speed up sin calculations - Ed
float turbsin[] ={
#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))
#define toradians 3.1415926535897932384626433832795
// MrG - texture shader stuffs
//Thanx MrG
#define DST_SIZE 16
#define DTS_CAUSTIC_SIZE 128
#define DTS_CAUSTIC_MID 64

unsigned int dst_texture = 0;
unsigned int dst_caustic = 0;

/**
 * Create the texture which warps texture shaders
 */
void CreateDSTTex() {
	signed char data[DST_SIZE][DST_SIZE][2];
	signed char data2[DTS_CAUSTIC_SIZE][DTS_CAUSTIC_SIZE][2];
	int x, y;
	//	int separation;

	for (x = 0; x < DST_SIZE; x++)
		for (y = 0; y < DST_SIZE; y++) {
			data[x][y][0] = rand() % 255 - 128;
			data[x][y][1] = rand() % 255 - 128;
		}

	glGenTextures(1, &dst_texture);
	glBindTexture(GL_TEXTURE_2D, dst_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DSDT8_NV, DST_SIZE, DST_SIZE, 0, GL_DSDT_NV, GL_BYTE, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);


	for (x = 0; x < DTS_CAUSTIC_SIZE; x++)
		for (y = 0; y < DTS_CAUSTIC_SIZE; y++) {
			data2[x][y][0] = sin(((float) x * 2.0f / (DTS_CAUSTIC_SIZE - 1)) * toradians)*128;
			data2[x][y][1] = sin(((float) y * 2.0f / (DTS_CAUSTIC_SIZE - 1)) * toradians)*128;
			//data2[x][y][2]=0;
		}

	glGenTextures(1, &dst_caustic);
	glBindTexture(GL_TEXTURE_2D, dst_caustic);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, DTS_CAUSTIC_SIZE, DTS_CAUSTIC_SIZE, 0, GL_RGB, GL_BYTE, data2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DSDT8_NV, DTS_CAUSTIC_SIZE, DTS_CAUSTIC_SIZE, 0, GL_DSDT_NV, GL_BYTE, data2);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/**
 * Does a water warp on the pre-fragmented glpoly_t chain
 */
void EmitWaterPolysMulti(msurface_t *fa) {
	glpoly_t *p;
	float *v;
	int i;
	float s, ss, t, tt, os, ot;
	vec3_t nv; //qmb :water wave
	//Texture shader
	float args[4] = {0.05f, 0.0f, 0.0f, 0.02f};

	glActiveTexture(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, fa->texinfo->texture->gl_texturenum);

	/*
	Texture Shader waterwarp, Damn this looks fantastic

	WHY texture shaders? because I can!
	- MrG
	 */
	if (gl_shader) {
		if (!dst_texture)
			CreateDSTTex();
		glBindTexture(GL_TEXTURE_2D, dst_texture);

		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);

		GL_EnableTMU(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_2D, fa->texinfo->texture->gl_texturenum);

		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_OFFSET_TEXTURE_2D_NV);
		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
		glTexEnvfv(GL_TEXTURE_SHADER_NV, GL_OFFSET_TEXTURE_MATRIX_NV, &args[0]);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glEnable(GL_TEXTURE_SHADER_NV);
	} else {
		GL_EnableTMU(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_2D, fa->texinfo->texture->gl_texturenum);

		if (gl_combine)
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
		else
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glColor4f(1, 1, 1, r_wateralpha.getFloat());

	for (p = fa->polys; p; p = p->next) {
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			os = v[3];
			ot = v[4];

			s = os + turbsin[(int) ((ot * 0.125 + cl.time) * TURBSCALE) & 255];
			s *= (1.0 / 64);

			ss = os + turbsin[(int) ((ot * 0.25 + (cl.time * 2)) * TURBSCALE) & 255];
			ss *= (0.5 / 64)*(-1);

			t = ot + turbsin[(int) ((os * 0.125 + cl.time) * TURBSCALE) & 255];
			t *= (1.0 / 64);

			tt = ot + turbsin[(int) ((os * 0.25 + (cl.time * 2)) * TURBSCALE) & 255];
			tt *= (0.5 / 64)*(-1);

			VectorCopy(v, nv);

			if (r_wave.getBool())
				nv[2] = v[2] + r_wave.getFloat() * sin(v[0]*0.02 + cl.time) * sin(v[1]*0.02 + cl.time) * sin(v[2]*0.02 + cl.time);

			glMultiTexCoord2f(GL_TEXTURE0_ARB, s, t);
			glMultiTexCoord2f(GL_TEXTURE1_ARB, ss + cl.time / 10, tt + cl.time / 10);

			glVertex3fv(nv);
		}
		glEnd();
	}

	glColor4f(1, 1, 1, 1);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);

	if (gl_shader) // MrG - texture shader waterwarp
		glDisable(GL_TEXTURE_SHADER_NV);

	GL_DisableTMU(GL_TEXTURE1_ARB);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glActiveTexture(GL_TEXTURE0_ARB);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void R_DrawWaterChain(msurface_t *s) {
	msurface_t *removelink;

	if ((r_wateralpha.getInt() == 0) || !s)
		return;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	while (s) {
		EmitWaterPolysMulti(s);

		removelink = s;
		s = s->texturechain;
		removelink->texturechain = NULL;
	}
}

void EmitSkyPolysMulti(msurface_t *fa) {
	int i;
	float *v;
	float s, ss, t, tt;
	vec3_t dir;
	float length;

	glActiveTexture(GL_TEXTURE0_ARB);
	glColor3f(1, 1, 1);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBindTexture(GL_TEXTURE_2D, fa->texinfo->texture->gl_texturenum);

	GL_EnableTMU(GL_TEXTURE1_ARB);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glBindTexture(GL_TEXTURE_2D, fa->texinfo->texture->gl_skynum);

	for (glpoly_t *p = fa->polys; p; p = p->next) {
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorSubtract(v, r_origin, dir);
			//VectorNormalize (dir);
			dir[2] *= 6; // flatten the sphere

			length = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
			length = sqrt(length);
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			s = (realtime * 8 + dir[0]) * (1.0 / 128);
			t = (realtime * 8 + dir[1]) * (1.0 / 128);

			ss = (realtime * 16 + dir[0]) * (1.0 / 128);
			tt = (realtime * 16 + dir[1]) * (1.0 / 128);

			glMultiTexCoord2f(GL_TEXTURE0_ARB, s, t);
			glMultiTexCoord2f(GL_TEXTURE1_ARB, ss, tt);
			glVertex3fv(v);
		}
		glEnd();
	}

	GL_DisableTMU(GL_TEXTURE1_ARB);
	glActiveTexture(GL_TEXTURE0_ARB);
}

int R_LoadSky(char *newname) {
	extern void GL_UploadLightmap(void);
	extern void BuildSurfaceDisplayList(model_t *m, msurface_t * fa);
	extern void GL_CreateSurfaceLightmap(msurface_t * surf);

	int i;
	char name[96];
	char dirname[64];

	oldsky = false;

	sprintf(skyname, "%s", newname);

	skymodel = Mod_ForName(skyname, false);
	if (skymodel) {
		//GL_BuildLightmaps ();
		for (i = 0; i < skymodel->numsurfaces; i++) {
			//			if ( skymodel->surfaces[i].flags & SURF_DRAWTURB )
			//				continue;
			GL_CreateSurfaceLightmap(skymodel->surfaces + i);
			BuildSurfaceDisplayList(skymodel, skymodel->surfaces + i);
			GL_UploadLightmap();
		}
		Con_Printf("Loaded sky model: %s\n", newname);
		return true;
	}

	sprintf(dirname, "gfx/env/");
	sprintf(name, "%s%s%s", dirname, skyname, suf[0]);

	//find where the sky is
	//some are in /env others /gfx/ernv
	//some have skyname?? others skyname_??
	skytex[0] = TextureManager::LoadExternTexture(name, false, false, gl_sincity.getBool());
	if (skytex[0] == 0) {
		sprintf(dirname, "env/");
		sprintf(name, "%s%s%s", dirname, skyname, suf[0]);
		skytex[0] = TextureManager::LoadExternTexture(name, false, false, gl_sincity.getBool());
		if (skytex[0] == 0) {
			sprintf(skyname, "%s_", skyname);
			sprintf(name, "%s%s%s", dirname, skyname, suf[0]);
			skytex[0] = TextureManager::LoadExternTexture(name, false, false, gl_sincity.getBool());
			if (skytex[0] == 0) {
				sprintf(dirname, "gfx/env/");
				sprintf(name, "%s%s%s", dirname, skyname, suf[0]);
				skytex[0] = TextureManager::LoadExternTexture(name, false, false, gl_sincity.getBool());
				if (skytex[0] == 0) {
					oldsky = true;
					return false;
				}
			}
		}
	}

	for (i = 1; i < 6; i++) {
		sprintf(name, "gfx/env/%s%s", skyname, suf[i]);

		skytex[i] = TextureManager::LoadExternTexture(name, false, false, false);
		if (skytex[i] == 0) {
			oldsky = true;
			return false;
		}
	}
	return true;
}

// LordHavoc: added LoadSky console command
void R_LoadSky_f(void) {
	switch (CmdArgs::getArgCount()) {
		case 1:
			if (skyname[0])
				Con_Printf("current sky: %s\n", skyname);
			else
				Con_Printf("no skybox has been set\n");
			break;
		case 2:
			if (R_LoadSky(CmdArgs::getArg(1))) {
				if (skyname[0])
					Con_Printf("skybox set to %s\n", skyname);
				else
					Con_Printf("skybox disabled\n");
			} else
				Con_Printf("failed to load skybox %s\n", CmdArgs::getArg(1));
			break;
		default:
			Con_Printf("usage: loadsky skyname\n");
			break;
	}
}

// LordHavoc: added LoadSky console command
void R_CurrentCoord_f(void) {
	Con_Printf("Current Position: %f,%f,%f\n", r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
}

void R_DrawSkyBox(void) {
	const float zero = 1.0 / 512;
	const float one = 511.0 / 512;

	glPushMatrix();
	glLoadIdentity();

	if (r_skydetail.getInt() >= 2) {
		glRotatef(-90, 1, 0, 0); // put Z going up
		glRotatef(90, 0, 0, 1); // put Z going up
		glRotatef(-r_refdef.viewangles[2], 1, 0, 0);
		glRotatef(-r_refdef.viewangles[0], 0, 1, 0);
		glRotatef(-r_refdef.viewangles[1], 0, 0, 1);
	} else {
		glRotatef(r_refdef.viewangles[2], 0, 0, 1);
		glRotatef(r_refdef.viewangles[0], 1, 0, 0);
		glRotatef(r_refdef.viewangles[1], 0, -1, 0);
	}

	if ((r_skydetail.getInt() == 1)) {
		glDisable(GL_DEPTH_TEST);
		glBindTexture(GL_TEXTURE_2D, skytex[skytexorder[0]]);
		glBegin(GL_QUADS);
		glTexCoord2f(one, zero);
		glVertex3f(16, -16, 16);
		glTexCoord2f(one, one);
		glVertex3f(16, -16, -16);
		glTexCoord2f(zero, one);
		glVertex3f(16, 16, -16);
		glTexCoord2f(zero, zero);
		glVertex3f(16, 16, 16);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, skytex[skytexorder[1]]);
		glBegin(GL_QUADS);
		glTexCoord2f(one, zero);
		glVertex3f(-16, 16, 16);
		glTexCoord2f(one, one);
		glVertex3f(-16, 16, -16);
		glTexCoord2f(zero, one);
		glVertex3f(-16, -16, -16);
		glTexCoord2f(zero, zero);
		glVertex3f(-16, -16, 16);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, skytex[skytexorder[2]]);
		glBegin(GL_QUADS);
		glTexCoord2f(one, zero);
		glVertex3f(16, 16, 16);
		glTexCoord2f(one, one);
		glVertex3f(16, 16, -16);
		glTexCoord2f(zero, one);
		glVertex3f(-16, 16, -16);
		glTexCoord2f(zero, zero);
		glVertex3f(-16, 16, 16);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, skytex[skytexorder[3]]);
		glBegin(GL_QUADS);
		glTexCoord2f(one, zero);
		glVertex3f(-16, -16, 16);
		glTexCoord2f(one, one);
		glVertex3f(-16, -16, -16);
		glTexCoord2f(zero, one);
		glVertex3f(16, -16, -16);
		glTexCoord2f(zero, zero);
		glVertex3f(16, -16, 16);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, skytex[skytexorder[4]]);
		glBegin(GL_QUADS);
		glTexCoord2f(one, zero);
		glVertex3f(16, -16, 16);
		glTexCoord2f(one, one);
		glVertex3f(16, 16, 16);
		glTexCoord2f(zero, one);
		glVertex3f(-16, 16, 16);
		glTexCoord2f(zero, zero);
		glVertex3f(-16, -16, 16);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, skytex[skytexorder[5]]);
		glBegin(GL_QUADS);
		glTexCoord2f(one, zero);
		glVertex3f(16, 16, -16);
		glTexCoord2f(one, one);
		glVertex3f(16, -16, -16);
		glTexCoord2f(zero, one);
		glVertex3f(-16, -16, -16);
		glTexCoord2f(zero, zero);
		glVertex3f(-16, 16, -16);
		glEnd();
		glEnable(GL_DEPTH_TEST);
	} else {
		float modelorg[3];

		R_Skybox = true;

		modelorg[0] = r_sky_x.getFloat();
		modelorg[1] = r_sky_y.getFloat();
		modelorg[2] = r_sky_z.getFloat();

		//VectorCopy (r_refdef.vieworg, modelorg);

		glTranslatef(-modelorg[0], -modelorg[1] - 10, -modelorg[2]);

		if (r_skydetail.getInt() == 2)
			R_DrawBrush(cl.worldmodel, &modelorg[0]); //doesnt work yet, needs some fiddleing because of draw order
		else
			R_DrawBrush(skymodel, &modelorg[0]);

		R_Skybox = false;
	}
	glPopMatrix();
}

void R_DrawSkyChain(msurface_t *s) {
	msurface_t *removelink;
	msurface_t *surf;
	const float zero = 1.0 / 512 + realtime * 0.1;
	const float one = 511.0 / 512 + realtime * 0.1;

	if (((r_skydetail.getInt() != 0 && oldsky != true) || r_skydetail.getInt() == 2) && !R_Skybox) {
		glpoly_t *p;
		float *v;
		int i;

		glActiveTexture(GL_TEXTURE0_ARB);
		glColor3f(1, 1, 1);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		//dont draw over the sky later
		glStencilFunc(GL_ALWAYS, 200, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glEnable(GL_STENCIL_TEST);
		R_DrawSkyBox();
		glDisable(GL_STENCIL_TEST);

		glClear(GL_DEPTH_BUFFER_BIT);

		if (r_skydetail.getInt() == 1)
			glColorMask(0, 0, 0, 0); //if its the sky cube all the sky will be covered
		else
			glColor3f(0, 0, 0); //else clear all the missed bits to black

		GL_DisableTMU(GL_TEXTURE0_ARB);
		glStencilFunc(GL_EQUAL, 200, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glEnable(GL_STENCIL_TEST);

		surf = s;
		while (surf) {
			for (p = surf->polys; p; p = p->next) {
				glBegin(GL_POLYGON);
				for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
					glVertex3fv(v);
				}
				glEnd();
			}
			removelink = surf;
			surf = surf->texturechain;
			//removelink->texturechain = NULL;
		}

		glDisable(GL_STENCIL_TEST);
		GL_EnableTMU(GL_TEXTURE0_ARB);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor3f(1, 1, 1);
		glColorMask(1, 1, 1, 1);
		return;
	}

	while (s) {
		EmitSkyPolysMulti(s);

		removelink = s;
		s = s->texturechain;
		removelink->texturechain = NULL;
	}
}

//===============================================================

/**
 * A sky texture is 256*128, with the right side being a masked overlay
 */
void R_InitSky(texture_t *mt) {
	int p;
	unsigned trans[128 * 128];
	unsigned transpix;
	unsigned *rgba;
	char name[64];

	byte *src = (byte *) mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level
	int r = 0, g = 0, b = 0;
	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++) {
			p = src[i * 256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i * 128) + j] = *rgba;
			r += ((byte *) rgba)[0];
			g += ((byte *) rgba)[1];
			b += ((byte *) rgba)[2];
		}

	((byte *) & transpix)[0] = r / (128 * 128);
	((byte *) & transpix)[1] = g / (128 * 128);
	((byte *) & transpix)[2] = b / (128 * 128);
	((byte *) & transpix)[3] = 0;
	
	sprintf(name, "%ssolid", mt->name);
	mt->gl_texturenum = TextureManager::LoadInternTexture(name, 128, 128, (byte *) & trans[0], false, false, 4, false);

	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++) {
			p = src[i * 256 + j];
			if (p == 0)
				trans[(i * 128) + j] = transpix;
			else
				trans[(i * 128) + j] = d_8to24table[p];
		}
	
	sprintf(name, "%salpha", mt->name);
	mt->gl_skynum = TextureManager::LoadInternTexture(name, 128, 128, (byte *) & trans[0], false, false, 4, false);
}
