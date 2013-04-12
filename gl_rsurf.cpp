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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "Texture.h"

#define	MAX_LIGHTMAPS	1024
#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

typedef struct glRect_s {
	unsigned char l, t, w, h;
} glRect_t;

int lightmap_bytes; // 1, 2, or 4
int lightmap_textures[MAX_LIGHTMAPS];
bool lightmap_modified[MAX_LIGHTMAPS];
glpoly_t *lightmap_polys[MAX_LIGHTMAPS];
glRect_t lightmap_rectchange[MAX_LIGHTMAPS];

int active_lightmaps;

unsigned blocklights[BLOCK_WIDTH*BLOCK_HEIGHT * 3];

int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];
// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte lightmaps[4 * MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];
//world texture chains
msurface_t *skychain = NULL;
msurface_t *waterchain = NULL;
msurface_t *extrachain = NULL;
msurface_t *outlinechain = NULL;

int detailtexture;
int detailtexture2;

void R_RenderDynamicLightmaps(msurface_t *fa);
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride);

/**
 * Returns the proper texture for a given time and base texture
 */
texture_t *R_TextureAnimation(texture_t *base) {
	//	if (currententity && currententity->frame)
	//	{
	if (base->alternate_anims)
		base = base->alternate_anims;
	//	}

	if (!base->anim_total)
		return base;

	int reletive = (int) (cl.time * 10) % base->anim_total;

	int count = 0;
	while (base->anim_min > reletive || base->anim_max <= reletive) {
		base = base->anim_next;
		if (!base)
			Sys_Error("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/*
=============================================================
	BRUSH MODELS
=============================================================
 */

void GL_EnableTMU(int tmu) {
	glActiveTexture(tmu);
	glEnable(GL_TEXTURE_2D);
}

void GL_DisableTMU(int tmu) {
	glActiveTexture(tmu);
	glDisable(GL_TEXTURE_2D);
}

void GL_VertexLight(vec3_t vertex) {
	extern vec3_t lightcolor;

	R_LightPoint(vertex);

	VectorScale(lightcolor, 1.0f / 100.0f, lightcolor);
	glColor3fv(lightcolor);
}

/*
=============================================================
	WORLD MODEL
=============================================================
 */
void Surf_DrawTextureChainsFour(model_t *model);

void R_RecursiveWorldNode(mnode_t *node, float *modelorg) {
	int c, side;
	mplane_t *plane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;
	double dot;

	//make sure we are still inside the world
	if (node->contents == CONTENTS_SOLID)
		return; // solid

	//is this node visable
	if (node->visframe != r_visframecount)
		return;

	//i think this checks if its on the screen and not behind the viewer
	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	// if a leaf node, draw stuff
	if (node->contents < 0) {
		pleaf = (mleaf_t *) node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c) {
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

		// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags(&pleaf->efrags);

		return;
	}

	// node is just a decision point, so go down the apropriate sides
	// find which side of the node we are on
	plane = node->plane;

	switch (plane->type) {
		case PLANE_X:
			dot = modelorg[0] - plane->dist;
			break;
		case PLANE_Y:
			dot = modelorg[1] - plane->dist;
			break;
		case PLANE_Z:
			dot = modelorg[2] - plane->dist;
			break;
		default:
			dot = DotProduct(modelorg, plane->normal) - plane->dist;
			break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode(node->children[side], modelorg);

	// recurse down the back side
	if (r_outline.getBool())
		R_RecursiveWorldNode(node->children[!side], modelorg);

	// draw stuff
	c = node->numsurfaces;

	if (c) {
		surf = cl.worldmodel->surfaces + node->firstsurface;

		{
			for (; c; c--, surf++) {
				if (surf->visframe != r_framecount)
					continue;

				if (surf->flags & SURF_DRAWSKY) {
					surf->texturechain = skychain;
					skychain = surf;
				} else if (surf->flags & SURF_DRAWTURB) {
					surf->texturechain = waterchain;
					waterchain = surf;
				} else {
					//add chain for drawing.
					surf->texturechain = surf->texinfo->texture->texturechain;
					surf->texinfo->texture->texturechain = surf;

					//setup eyecandy chain
					if ((surf->flags & SURF_UNDERWATER && gl_caustics.getBool()) ||
							(surf->flags & SURF_SHINY_METAL && gl_shiny.getBool()) ||
							(surf->flags & SURF_SHINY_GLASS && gl_shiny.getBool())) {
						surf->extra = extrachain;
						extrachain = surf;
					}

					if (r_outline.getBool()) {
						surf->outline = outlinechain;
						outlinechain = surf;
					}
				}
			}
		}
	}

	// recurse down the back side
	if (!r_outline.getBool())
		R_RecursiveWorldNode(node->children[!side], modelorg);
}

void R_DrawBrushModel(entity_t *e) {
	int k;
	vec3_t mins, maxs;
	model_t *clmodel;
	bool rotated;
	int i;
	extern vec3_t lightcolor;
	int lnum;
	vec3_t dist;
	float add;
	float modelorg[3];

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	} else {
		rotated = false;
		VectorAdd(e->origin, clmodel->mins, mins);
		VectorAdd(e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	//Lighting for model
	//dynamic lights for non lightmaped bmodels
	if (clmodel->firstmodelsurface == 0) {
		R_LightPoint(e->origin);
		for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
			if (cl_dlights[lnum].die >= cl.time) {
				VectorSubtract(e->origin, cl_dlights[lnum].origin, dist);
				add = cl_dlights[lnum].radius - VectorLength(dist);
				if (add > 0) {
					lightcolor[0] += add * cl_dlights[lnum].colour[0];
					lightcolor[1] += add * cl_dlights[lnum].colour[1];
					lightcolor[2] += add * cl_dlights[lnum].colour[2];
				}
			}
		}
		VectorScale(lightcolor, 1.0f / 100.0f, lightcolor);

		if (gl_ammoflash.getBool()) {
			lightcolor[0] += sin(2 * cl.time * M_PI) / 4;
			lightcolor[1] += sin(2 * cl.time * M_PI) / 4;
			lightcolor[2] += sin(2 * cl.time * M_PI) / 4;
		}

		glColor3fv(lightcolor);
	} else {
		glColor3f(1.0, 1.0, 1.0);
		memset(lightmap_polys, 0, sizeof (lightmap_polys));
	}

	VectorSubtract(r_refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	glPushMatrix();
	e->angles[0] = -e->angles[0]; // stupid quake bug
	R_RotateForEntity(e);
	e->angles[0] = -e->angles[0]; // stupid quake bug

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend.getBool()) {
		for (k = 0; k < MAX_DLIGHTS; k++) {
			if ((cl_dlights[k].die < cl.time) ||
					(!cl_dlights[k].radius))
				continue;

			R_MarkLights(&cl_dlights[k], 1 << k,
					clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	R_DrawBrush(clmodel, &modelorg[0]);

	glPopMatrix();
}

void R_DrawBrush(model_t *clmodel, float *modelorg) {
	msurface_t *surf = &clmodel->surfaces[clmodel->firstmodelsurface];
	//if the chains haven't been made then make them
	for (int i = 0; i < clmodel->nummodelsurfaces; i++, surf++) {
		// find which side of the node we are on
		mplane_t *pplane = surf->plane;
		float dot;

		switch (pplane->type) {
			case PLANE_X:
				dot = modelorg[0] - pplane->dist;
				break;
			case PLANE_Y:
				dot = modelorg[1] - pplane->dist;
				break;
			case PLANE_Z:
				dot = modelorg[2] - pplane->dist;
				break;
			default:
				dot = DotProduct(modelorg, pplane->normal) - pplane->dist;
				break;
		}

		// draw the polygon
		if (r_outline.getBool() ||
				((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
				(!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			if (surf->flags & SURF_DRAWSKY) {
				surf->texturechain = skychain;
				skychain = surf;
			} else if (surf->flags & SURF_DRAWTURB) {
				surf->texturechain = waterchain;
				waterchain = surf;
			} else {
				//add chain for drawing.
				surf->texturechain = surf->texinfo->texture->texturechain;
				surf->texinfo->texture->texturechain = surf;

				//setup eyecandy chain
				if ((surf->flags & SURF_UNDERWATER && gl_caustics.getBool()) ||
						(surf->flags & SURF_SHINY_METAL && gl_shiny.getBool()) ||
						(surf->flags & SURF_SHINY_GLASS && gl_shiny.getBool())) {
					if (!R_Skybox) {
						surf->extra = extrachain;
						extrachain = surf;
					}
				}
				if (r_outline.getBool()) {
					surf->outline = outlinechain;
					outlinechain = surf;
				}
			}
		}
	}

	Surf_DrawTextureChainsFour(clmodel);
}

void R_DrawWorld(void) {
	entity_t ent;
	float modelorg[3];

	memset(&ent, 0, sizeof (ent));
	ent.model = cl.worldmodel;

	VectorCopy(r_refdef.vieworg, modelorg);

	glColor3f(1, 1, 1);
	memset(lightmap_polys, 0, sizeof (lightmap_polys));

	//draw the display list
	R_RecursiveWorldNode(cl.worldmodel->nodes, &modelorg[0]);

	//draw the world normally
	Surf_DrawTextureChainsFour(cl.worldmodel);
}

void R_MarkLeaves(void) {
	byte *vis;
	mnode_t *node;
	int i;
	byte solid[4096];

	if (r_oldviewleaf == r_viewleaf && !r_novis.getBool())
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.getBool()) {
		vis = solid;
		memset(solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
	} else
		vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel);

	for (i = 0; i < cl.worldmodel->numleafs; i++) {
		if (vis[i >> 3] & (1 << (i & 7))) {
			node = (mnode_t *) & cl.worldmodel->leafs[i + 1];
			do {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

void BuildSurfaceDisplayList(model_t *m, msurface_t *fa) {
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	int vertpage;
	float *vec;
	float s, t;
	glpoly_t *poly;

	// reconstruct the polygon
	pedges = m->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	// draw texture
	poly = (glpoly_t *) Hunk_Alloc(sizeof (glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof (float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++) {
		lindex = m->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = m->vertexes[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = m->vertexes[r_pedge->v[1]].position;
		}
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16; //fa->texinfo->texture->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;

		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= 128;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= 128;

		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][7] = s;
		poly->verts[i][8] = t;
	}

	// remove co-linear points - Ed
	if (!gl_keeptjunctions.getBool() && !(fa->flags & SURF_UNDERWATER)) {
		for (i = 0; i < lnumverts; ++i) {
			vec3_t v1, v2;
			float *prev, *current, *next;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			current = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract(current, prev, v1);
			VectorNormalize(v1);
			VectorSubtract(next, prev, v2);
			VectorNormalize(v2);

			// skip co-linear points
#define COLINEAR_EPSILON 0.001
			if ((fabs(v1[0] - v2[0]) <= COLINEAR_EPSILON) &&
					(fabs(v1[1] - v2[1]) <= COLINEAR_EPSILON) &&
					(fabs(v1[2] - v2[2]) <= COLINEAR_EPSILON)) {
				int j;
				for (j = i + 1; j < lnumverts; ++j) {
					int k;
					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;
}

/*
=============================================================================
  LIGHTMAP ALLOCATION
=============================================================================
 */

/**
 * returns a texture number and the position inside it
 */
int AllocBlock(int w, int h, int *x, int *y) {
	int i, j;
	int best, best2;
	int texnum;

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++) {
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++) {
			best2 = 0;

			for (j = 0; j < w; j++) {
				if (allocated[texnum][i + j] >= best)
					break;
				if (allocated[texnum][i + j] > best2)
					best2 = allocated[texnum][i + j];
			}
			if (j == w) { // this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error("AllocBlock: full");

	return 0;
}

void GL_CreateSurfaceLightmap(msurface_t *surf) {
	int smax, tmax;
	byte *base;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH*BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap(surf, base, BLOCK_WIDTH * lightmap_bytes);
}

void UpdateLightmap(int texNum) {
	if (lightmap_modified[texNum]) {
		lightmap_modified[texNum] = false;
		glRect_t *theRect = &lightmap_rectchange[texNum];
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, lightmaps + (texNum * BLOCK_HEIGHT + theRect->t) * BLOCK_WIDTH * lightmap_bytes);
		theRect->l = BLOCK_WIDTH;
		theRect->t = BLOCK_HEIGHT;
		theRect->h = 0;
		theRect->w = 0;
	}
}

void GL_UploadLightmap(void) {
	glActiveTexture(GL_TEXTURE0_ARB);

	// upload all lightmaps that were filled
	for (int i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!allocated[i][0])
			break; // no more used
//		UpdateLightmap(i);
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		glBindTexture(GL_TEXTURE_2D, lightmap_textures[i]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, lightmap_bytes, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes);
	}
	glActiveTexture(GL_TEXTURE0_ARB);
}

/**
 * Builds the lightmap texture with all the surfaces from all brush models
 */
void GL_BuildLightmaps(void) {
	memset(allocated, 0, sizeof (allocated));

	r_framecount = 1; // no dlightcache

	if (!lightmap_textures[0]) {
		for (int i=0; i<MAX_LIGHTMAPS; i++)
			lightmap_textures[i] = TextureManager::getTextureId();
	}

	lightmap_bytes = 4;

	for (int j = 1; j < MAX_MODELS; j++) {
		model_t *m = cl.model_precache[j];
		if (!m) break;
		if (m->name[0] == '*') continue;
		if (m->type != mod_brush) continue;

		for (int i = 0; i < m->numsurfaces; i++) {
			if (m->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY))
				continue;

			GL_CreateSurfaceLightmap(m->surfaces + i);
			BuildSurfaceDisplayList(m, m->surfaces + i);
		}
	}
	GL_UploadLightmap();
}

void R_AddDynamicLights(msurface_t *surf) {
	int sd, td;
	float dist, rad, minlight;
	vec3_t impact, local;
	int smax, tmax;
	mtexinfo_t *tex;
	float cred, cgreen, cblue, brightness;
	unsigned *bl;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (int lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		if (!(surf->dlightbits & (1 << lnum)))
			continue; // not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct(cl_dlights[lnum].origin, surf->plane->normal) - surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (int i = 0; i < 3; i++) {
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;
		}

		local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		bl = blocklights;
		cred = cl_dlights[lnum].colour[0] * 256.0f;
		cgreen = cl_dlights[lnum].colour[1] * 256.0f;
		cblue = cl_dlights[lnum].colour[2] * 256.0f;

		for (int t = 0; t < tmax; t++) {
			td = local[1] - t * 16;
			if (td < 0)
				td = -td;
			for (int s = 0; s < smax; s++) {
				sd = local[0] - s * 16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td >> 1);
				else
					dist = td + (sd >> 1);
				if (dist < minlight) {
					brightness = rad - dist;
					bl[2] += (int) (brightness * cred);
					bl[1] += (int) (brightness * cgreen);
					bl[0] += (int) (brightness * cblue);
				}
				bl += 3;
			}
		}
	}
}

//directly changes values
__inline void RGBtoGrayscale(unsigned *rgb) {
	unsigned value;
	//unsigned output;

	if (gl_sincity.getBool()) {
		//value = rgb[0] * 0.2125 + rgb[1] * 0.7154 + rgb[2] * 0.0721;
		//value = max(max(rgb[0],rgb[1]),rgb[2]);
		value = rgb[0] * 0.299 + rgb[1] * 0.587 + rgb[2] * 0.114;

		rgb[0] = value;
		rgb[1] = value;
		rgb[2] = value;
	}
}

/**
 * Combine and scale multiple lightmaps into the 8.8 format in blocklights
 */
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride) {
	int smax, tmax, t, size;
	byte *lightmap;
	unsigned scale;
	int maps;
	unsigned *bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax*tmax;
	lightmap = surf->samples;

	// set to full bright if no light data
	if (!cl.worldmodel->lightdata) {
		bl = blocklights;
		for (int i = 0; i < size; i++) {
			*bl++ = 255 * 256; //r
			*bl++ = 255 * 256; //g
			*bl++ = 255 * 256; //b
		}
		goto store;
	}

	// clear to no light
	bl = blocklights;
	for (int i = 0; i < size; i++) {
		*bl++ = 0; //r
		*bl++ = 0; //g
		*bl++ = 0; //b
	}

	// add all the lightmaps
	if (lightmap)
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
			unsigned *rgb;

			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale; // 8.8 fraction

			bl = blocklights;
			for (int i = 0; i < size; i++) {
				rgb = bl;
				*bl++ += *lightmap++ * scale; //r
				*bl++ += *lightmap++ * scale; //g
				*bl++ += *lightmap++ * scale; //b

				//RGBtoGrayscale(rgb);
			}
		}

	//qmb :overbright dynamic ligths only
	//so scale back the normal lightmap so it renders normally
	bl = blocklights;
	for (int i = 0; i < size * 3 && gl_combine; i++)
		*bl++ >>= 2;

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights(surf);

	// bound, invert, and shift
store:
	stride -= (smax << 2);
	bl = blocklights;
	for (int i = 0; i < tmax; i++, dest += stride) {
		for (int j = 0; j < smax; j++) {
			//FIXME: should scale back so it remains the right colour
			// LordHavoc: .lit support begin
			t = bl[0] >> 7;
			if (t > 255) t = 255;
			*dest++ = t; //r
			t = bl[1] >> 7;
			if (t > 255) t = 255;
			*dest++ = t; //g
			t = bl[2] >> 7;
			if (t > 255) t = 255;
			*dest++ = t; //b
			bl = bl + 3;
			*dest++ = 255;
			// LordHavoc: .lit support end
		}
	}
}

void R_RenderDynamicLightmaps(msurface_t *fa) {
	byte *base;
	int maps;
	glRect_t *theRect;
	int smax, tmax;

	c_brush_polys++;

	if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount // dynamic this frame
			|| fa->cached_dlight) // dynamic previously
	{
dynamic:
		if (r_dynamic.getBool()) {
			lightmap_modified[fa->lightmaptexturenum] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];
			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			smax = (fa->extents[0] >> 4) + 1;
			tmax = (fa->extents[1] >> 4) + 1;
			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s - theRect->l) + smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t - theRect->t) + tmax;
			base = lightmaps + fa->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap(fa, base, BLOCK_WIDTH * lightmap_bytes);
		}
	}
}