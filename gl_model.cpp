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
// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"
#include "gl_md3.h"
#include "mathlib.h"
#include "Texture.h"

model_t *loadmodel;
char loadname[32]; // for hunk tags

void Mod_LoadSpriteModel(model_t *mod, void *buffer);
void Mod_LoadBrushModel(model_t *mod, void *buffer);
void Mod_LoadAliasModel(model_t *mod, void *buffer);
void Mod_LoadQ2AliasModel(model_t *mod, void *buffer);
model_t *Mod_LoadModel(model_t *mod, bool crash);

byte mod_novis[MAX_MAP_LEAFS / 8];

#define	MAX_MOD_KNOWN	512
model_t mod_known[MAX_MOD_KNOWN];
int mod_numknown;

CVar gl_subdivide_size("gl_subdivide_size", "128", true);
texture_t *r_notexture_mip;

void Mod_Init(void) {
	CVar::registerCVar(&gl_subdivide_size);
	Q_memset(mod_novis, 0xff, sizeof (mod_novis));
	
	// create a simple checkerboard texture for the default
	r_notexture_mip = (texture_t *) Hunk_AllocName(sizeof (texture_t), "notexture");
	r_notexture_mip->width = r_notexture_mip->height = 0;
	for (int i=0; i<MIPLEVELS; i++)
		r_notexture_mip->offsets[i] = 0;
	r_notexture_mip->gl_texturenum = TextureManager::defaultTexture;
}

/**
 * Caches the data if needed
 */
void *Mod_Extradata(model_t *mod) {
	void *r;

	r = mod->cache->getData();
	if (r)
		return r;

	Mod_LoadModel(mod, true);

	r = mod->cache->getData();
	if (!r)
		Sys_Error("Mod_Extradata: caching failed");

	return r;
}

mleaf_t *Mod_PointInLeaf(vec3_t p, model_t *model) {
	mnode_t *node;
	float d;
	mplane_t *plane;

	if (!model || !model->nodes)
		Sys_Error("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1) {
		if (node->contents < 0)
			return (mleaf_t *) node;
		plane = node->plane;
		d = DotProduct(p, plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
}

byte *Mod_DecompressVis(byte *in, model_t *model) {
	static byte decompressed[MAX_MAP_LEAFS / 8];
	int c;
	byte *out;
	int row;

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

	if (!in) { // no vis info, so make all visible
		while (row) {
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

byte *Mod_LeafPVS(mleaf_t *leaf, model_t *model) {
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis(leaf->compressed_vis, model);
}

void Mod_ClearAll(void) {
	int i;
	model_t *mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (mod->type != mod_alias)
			mod->needload = true;
}

model_t *Mod_FindName(const char *name) {
	int i;
	model_t *mod;

	if (!name[0])
		Sys_Error("Mod_ForName: NULL name");

	// search the currently loaded models
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (!Q_strcmp(mod->name, name))
			break;

	if (i == mod_numknown) {
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error("mod_numknown == MAX_MOD_KNOWN");
		Q_strcpy(mod->name, name);
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}

void Mod_TouchModel(char *name) {
	model_t *mod;

	mod = Mod_FindName(name);

	if (!mod->needload) {
		//		if (mod->type == mod_alias)
		//			Cache_Check (&mod->cache);
	}
}

/**
 * Loads a model into the cache
 */
model_t *Mod_LoadModel(model_t *mod, bool crash) {
	void *d;
	unsigned *buf;
	byte stackbuf[1024]; // avoid dirtying the cache heap
	char strip[128];
	char md2name[128];
	char md3name[128];

	if (!mod->needload) {
		if (mod->type == mod_alias) {
			d = mod->cache->getData();
			if (d)
				return mod;
		} else
			return mod; // not cached at all
	}

#ifdef MULTIMDL //If a directory is passed in stead of a path, open it as a Q3Player.
	c = COM_FileExtension(mod->name);
	if (!c[0]) {
		mod->needload = false;
		Mod_LoadQ3MultiModel(mod);
		return mod;
	}
#endif

	// load the file
	COM_StripExtension(mod->name, &strip[0]);
	sprintf(&md2name[0], "%s.md2", &strip[0]);
	sprintf(&md3name[0], "%s.md3", &strip[0]);

	buf = (unsigned *) COM_LoadStackFile(md3name, stackbuf, sizeof (stackbuf));
	if (!buf) {
		buf = (unsigned *) COM_LoadStackFile(md2name, stackbuf, sizeof (stackbuf));
		if (!buf) {
			buf = (unsigned *) COM_LoadStackFile(mod->name, stackbuf, sizeof (stackbuf));
			if (!buf) {
				if (crash)
					Sys_Error("Mod_NumForName: %s not found", mod->name);
				return NULL;
			}
		}
	}

	// allocate a new model
	COM_FileBase(mod->name, loadname);

	loadmodel = mod;

	// fill it in

	// call the apropriate loader
	mod->needload = false;

	switch (LittleLong(*(unsigned *) buf)) {
		case MD3IDHEADER:
			Mod_LoadQ3Model(mod, buf);
			break;

		case MD2IDALIASHEADER:
			Mod_LoadQ2AliasModel(mod, buf);
			break;

		case IDPOLYHEADER:
			Mod_LoadAliasModel(mod, buf);
			break;

		case IDSPRITEHEADER:
			Mod_LoadSpriteModel(mod, buf);
			break;

		default:
			Mod_LoadBrushModel(mod, buf);
			break;
	}

	return mod;
}

/**
 * Loads in a model for the given name
 */
model_t *Mod_ForName(const char *name, bool crash) {
	model_t *mod;

	mod = Mod_FindName(name);

	return Mod_LoadModel(mod, crash);
}

/*
===============================================================================
					BRUSHMODEL LOADING
===============================================================================
 */

byte *mod_base;

bool Img_HasFullbrights(byte *pixels, int size) {
	for (int i = 0; i < size; i++)
		if (pixels[i] >= 224)
			return true;

	return false;
}

void Mod_LoadTextures(lump_t *l) {
	extern CVar gl_24bitmaptex;
	int pixels, num, max, altmax;
	miptex_t *mt;
	texture_t *tx, *tx2;
	texture_t * anims[10];
	texture_t * altanims[10];
	dmiptexlump_t *m;
	char texname[64], texnamefb[64], texnamefbluma[64];

	if (!l->filelen) {
		loadmodel->textures = NULL;
		return;
	}
	m = (dmiptexlump_t *) (mod_base + l->fileofs);

	m->nummiptex = LittleLong(m->nummiptex);

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = (texture_t **) Hunk_AllocName(m->nummiptex * sizeof (*loadmodel->textures), loadname);

	for (int i = 0; i < m->nummiptex; i++) {
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;
		mt = (miptex_t *) ((byte *) m + m->dataofs[i]);
		mt->width = LittleLong(mt->width);
		mt->height = LittleLong(mt->height);
		for (int j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong(mt->offsets[j]);

		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error("Texture %s is not 16 aligned", mt->name);
		pixels = mt->width * mt->height / 64 * 85;
		tx = (texture_t *) Hunk_AllocName(sizeof (texture_t) + pixels, loadname);
		loadmodel->textures[i] = tx;

		Q_memcpy(tx->name, mt->name, sizeof (tx->name));
		tx->width = mt->width;
		tx->height = mt->height;
		for (int j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof (texture_t) - sizeof (miptex_t);
		// the pixels immediately follow the structures
		Q_memcpy(tx + 1, mt + 1, pixels);

		if (!Q_strncmp(mt->name, "sky", 3)) {
			R_InitSky(tx);
		} else {
			sprintf(texname, "textures/%s", mt->name);
			if (gl_24bitmaptex.getBool()) {
				tx->gl_texturenum = TextureManager::LoadExternTexture(texname, false, false, gl_sincity.getBool());

				//if there is a 24bit texture, check for a fullbright
				if (tx->gl_texturenum != 0) {
					sprintf(texnamefbluma, "textures/%s_luma", mt->name);
					tx->gl_fullbright = TextureManager::LoadExternTexture(texnamefbluma, false, true, false);

					/*if (tx->gl_fullbright == 0){ //no texture _glow
						sprintf (texnamefb, "textures/%s_glow", mt->name);
						tx->gl_fullbright = GL_LoadTexImage (texnamefb, false, true);
					}*/
				} else {
					tx->gl_fullbright = 0;
				}
			}

			if (tx->gl_texturenum == 0) { // No Matching Texture
				tx->gl_texturenum = TextureManager::LoadInternTexture(mt->name, tx->width, tx->height, (byte *) (tx + 1), true, false, 1, gl_sincity.getBool());

				if (Img_HasFullbrights((byte *) (tx + 1), tx->width * tx->height)) {
					tx->gl_fullbright = TextureManager::LoadInternTexture(texnamefb, tx->width, tx->height, (byte *) (tx + 1), true, true, 1, false);
				} else {
					tx->gl_fullbright = 0;
				}
			}
		}
	}

	// sequence the animations
	for (int i = 0; i < m->nummiptex; i++) {
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue; // allready sequenced

		// find the number of frames in the animation
		Q_memset(anims, 0, sizeof (anims));
		Q_memset(altanims, 0, sizeof (altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9') {
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		} else if (max >= 'A' && max <= 'J') {
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		} else
			Sys_Error("Bad animating texture %s", tx->name);

		for (int j = i + 1; j < m->nummiptex; j++) {
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (Q_strcmp(tx2->name + 2, tx->name + 2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9') {
				num -= '0';
				anims[num] = tx2;
				if (num + 1 > max)
					max = num + 1;
			} else if (num >= 'A' && num <= 'J') {
				num = num - 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax)
					altmax = num + 1;
			} else
				Sys_Error("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (int j = 0; j < max; j++) {
			tx2 = anims[j];
			if (!tx2)
				Sys_Error("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j + 1) % max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (int j = 0; j < altmax; j++) {
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j + 1) % altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

void Mod_LoadLighting(lump_t *l) {
	int i;
	byte *in, *out, *data;
	byte d;
	char litfilename[1024];

	loadmodel->lightdata = NULL;
	// LordHavoc: check for a .lit file
	Q_strcpy(litfilename, loadmodel->name);
	COM_StripExtension(litfilename, litfilename);
	Q_strcat(litfilename, ".lit");

	data = (byte*) COM_LoadHunkFile(litfilename); //, false);
	if (data) {
		if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T') {
			i = LittleLong(((int *) data)[1]);
			if (i == 1) {
				Con_DPrintf("%s loaded", litfilename);
				loadmodel->lightdata = data + 8;
				return;
			} else
				Con_Printf("Unknown .lit file version (%d)\n", i);
		} else
			Con_Printf("Corrupt .lit file (old version?), ignoring\n");
	}
	// LordHavoc: no .lit found, expand the white lighting data to color

	//check if there is lump data
	if (!l->filelen)
		return;

	loadmodel->lightdata = (byte *) Hunk_AllocName(l->filelen * 3, litfilename);
	in = loadmodel->lightdata + l->filelen * 2; // place the file at the end, so it will not be overwritten until the very last write
	out = loadmodel->lightdata;
	Q_memcpy(in, mod_base + l->fileofs, l->filelen);
	for (i = 0; i < l->filelen; i++) {
		d = *in++;
		*out++ = d;
		*out++ = d;
		*out++ = d;
	}
}

void Mod_LoadVisibility(lump_t *l) {
	if (!l->filelen) {
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = (byte *) Hunk_AllocName(l->filelen, loadname);
	Q_memcpy(loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}

void Mod_LoadEntities(lump_t *l) {
	byte *data;
	char entfilename[1024];

	loadmodel->entities = NULL;

	Q_strcpy(entfilename, loadmodel->name);
	COM_StripExtension(entfilename, entfilename);
	Q_strcat(entfilename, ".ent");

	data = (byte*) COM_LoadHunkFile(entfilename); //, false);
	if (data) {
		loadmodel->entities = (char *) data;
		return;
	}
	//no .ent found
	//check if there is lump data
	if (!l->filelen) {
		loadmodel->entities = NULL;
		return;
	}

	loadmodel->entities = (char *) Hunk_AllocName(l->filelen, loadname);
	Q_memcpy(loadmodel->entities, mod_base + l->fileofs, l->filelen);
}

void Mod_LoadVertexes(lump_t *l) {
	dvertex_t *in;
	mvertex_t *out;
	int i, count;

	in = (dvertex_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (mvertex_t *) Hunk_AllocName(count * sizeof (*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

void Mod_LoadSubmodels(lump_t *l) {
	dmodel_t *in;
	dmodel_t *out;
	int i, j, count;

	in = (dmodel_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (dmodel_t *) Hunk_AllocName(count * sizeof (*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) { // spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong(in->headnode[j]);
		out->visleafs = LittleLong(in->visleafs);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

void Mod_LoadEdges(lump_t *l) {
	dedge_t *in;
	medge_t *out;
	int i, count;

	in = (dedge_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (medge_t *) Hunk_AllocName((count + 1) * sizeof (*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

void Mod_LoadTexinfo(lump_t *l) {
	texinfo_t *in;
	mtexinfo_t *out;
	int i, j, count;
	int miptex;
	float len1, len2;

	in = (texinfo_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (mtexinfo_t *) Hunk_AllocName(count * sizeof (*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 8; j++)
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);
		len1 = Length(out->vecs[0]);
		len2 = Length(out->vecs[1]);
		len1 = (len1 + len2) / 2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = LittleLong(in->miptex);
		out->flags = LittleLong(in->flags);

		if (!loadmodel->textures) {
			out->texture = r_notexture_mip; // checkerboard texture
			out->flags = 0;
		} else {
			if (miptex >= loadmodel->numtextures)
				Sys_Error("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture) {
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

/**
 * Fills in s->texturemins[] and s->extents[]
 */
void CalcSurfaceExtents(msurface_t *s) {
	float mins[2], maxs[2], val;
	int i, j, e;
	mvertex_t *v;
	mtexinfo_t *tex;
	int bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = loadmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j = 0; j < 2; j++) {
			val = v->position[0] * tex->vecs[j][0] +
					v->position[1] * tex->vecs[j][1] +
					v->position[2] * tex->vecs[j][2] +
					tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++) {
		bmins[i] = floor(mins[i] / 16);
		bmaxs[i] = ceil(maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */)
			Sys_Error("Bad surface extents");
	}
}

void Mod_LoadFaces(lump_t *l) {
	dface_t *in;
	msurface_t *out;
	int i, count, surfnum;
	int planenum, side;

	in = (dface_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (msurface_t *) Hunk_AllocName(count * sizeof (*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + LittleShort(in->texinfo);

		CalcSurfaceExtents(out);

		// lighting info
		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + (i * 3); // LordHavoc

		// set the drawing flags flag
		if (!Q_strncmp(out->texinfo->texture->name, "sky", 3)) { // sky
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			GL_SubdivideSurface(out); // cut up polygon for warps
			continue;
		}

		if (!Q_strncmp(out->texinfo->texture->name, "*", 1)) { // turbulent
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i = 0; i < 2; i++) {
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface(out); // cut up polygon for warps
			continue;
		}

		//qmb :refections
		//JHL:ADD; Make thee shine like a glass!
		if ((!Q_strncmp(out->texinfo->texture->name, "window", 6)
				&& Q_strncmp(out->texinfo->texture->name, "window03", 8))
				|| !Q_strncmp(out->texinfo->texture->name, "afloor3_1", 9)
				|| !Q_strncmp(out->texinfo->texture->name, "wizwin", 6))
			out->flags |= SURF_SHINY_GLASS;

		//JHL:ADD; Make thee shine like the mighty steel!
		if ((!Q_strncmp(out->texinfo->texture->name, "metal", 5) // iron / steel
				&& Q_strncmp(out->texinfo->texture->name, "metal4", 6)
				&& Q_strncmp(out->texinfo->texture->name, "metal5", 6)
				&& Q_strncmp(out->texinfo->texture->name, "metal1_6", 8)
				&& Q_strncmp(out->texinfo->texture->name, "metal2_1", 8)
				&& Q_strncmp(out->texinfo->texture->name, "metal2_2", 8)
				&& Q_strncmp(out->texinfo->texture->name, "metal2_3", 8)
				&& Q_strncmp(out->texinfo->texture->name, "metal2_4", 8)
				&& Q_strncmp(out->texinfo->texture->name, "metal2_5", 8)
				&& Q_strncmp(out->texinfo->texture->name, "metal2_7", 8)
				&& Q_strncmp(out->texinfo->texture->name, "metal2_8", 8))
				|| (!Q_strncmp(out->texinfo->texture->name, "cop", 3) // copper
				&& Q_strncmp(out->texinfo->texture->name, "cop3_1", 8))
				|| !Q_strncmp(out->texinfo->texture->name, "rune", 4) // runes
				|| !Q_strncmp(out->texinfo->texture->name, "met_", 4)
				|| !Q_strncmp(out->texinfo->texture->name, "met5", 4)) // misc. metal
			out->flags |= SURF_SHINY_METAL;
	}
}

void Mod_SetParent(mnode_t *node, mnode_t *parent) {
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

void Mod_LoadNodes(lump_t *l) {
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;

	in = (dnode_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (mnode_t *) Hunk_AllocName(count * sizeof (*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (j = 0; j < 2; j++) {
			p = LittleShort(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *) (loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent(loadmodel->nodes, NULL); // sets nodes and leafs
}

void Mod_LoadLeafs(lump_t *l) {
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p;

	in = (dleaf_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (mleaf_t *) Hunk_AllocName(count * sizeof (*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
				LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY) {
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

void Mod_LoadClipnodes(lump_t *l) {
	dclipnode_t *in, *out;
	int i, count;
	hull_t *hull;

	in = (dclipnode_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (dclipnode_t *) Hunk_AllocName(count * sizeof (*out), loadname);

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

/**
 * Deplicate the drawing hull structure as a clipping hull
 */
void Mod_MakeHull0(void) {
	mnode_t *in, *child;
	dclipnode_t *out;
	int i, j, count;
	hull_t *hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = (dclipnode_t *) Hunk_AllocName(count * sizeof (*out), loadname);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = in->plane - loadmodel->planes;
		for (j = 0; j < 2; j++) {
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

void Mod_LoadMarksurfaces(lump_t *l) {
	int i, j, count;
	short *in;
	msurface_t **out;

	in = (short *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (msurface_t **) Hunk_AllocName(count * sizeof (*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

void Mod_LoadSurfedges(lump_t *l) {
	int i, count;
	int *in, *out;

	in = (int *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (int *) Hunk_AllocName(count * sizeof (*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

void Mod_LoadPlanes(lump_t *l) {
	int i, j;
	mplane_t *out;
	dplane_t *in;
	int count;
	int bits;

	in = (dplane_t *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = (mplane_t *) Hunk_AllocName(count * 2 * sizeof (*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++) {
		bits = 0;
		for (j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}

float RadiusFromBounds(vec3_t mins, vec3_t maxs) {
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++) {
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length(corner);
}

void Mod_LoadBrushModel(model_t *mod, void *buffer) {
	dheader_t *header;
	dmodel_t *bm;

	loadmodel->type = mod_brush;

	header = (dheader_t *) buffer;
	int version = LittleLong(header->version);
	if (version != BSPVERSION)
		Sys_Error("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, version, BSPVERSION);

	// swap all the lumps
	mod_base = (byte *) header;

	for (unsigned i = 0; i<sizeof (dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

	// load into heap
	Mod_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures(&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes(&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities(&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels(&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0();

	mod->numframes = 2; // regular and alternate animation

	// set up the submodels (FIXME: this is confusing)
	for (int i = 0; i < mod->numsubmodels; i++) {
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (int j = 1; j < MAX_MAP_HULLS; j++) {
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy(bm->maxs, mod->maxs);
		VectorCopy(bm->mins, mod->mins);

		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1) { // duplicate the basic information
			char name[10];

			sprintf(name, "*%i", i + 1);
			loadmodel = Mod_FindName(name);
			*loadmodel = *mod;
			Q_strcpy(loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================
ALIAS MODELS
==============================================================================
 */
aliashdr_t *pheader;

stvert_t stverts[MAXALIASVERTS];
mtriangle_t triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t *poseverts[MAXALIASFRAMES];
int posenum;
int aliasbboxmins[3], aliasbboxmaxs[3]; //qmb :bounding box fix

void * Mod_LoadAliasFrame(void * pin, maliasframedesc_t *frame) {
	trivertx_t *pinframe;
	int i;
	daliasframe_t *pdaliasframe;

	pdaliasframe = (daliasframe_t *) pin;

	Q_strcpy(frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about
		// endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];

		aliasbboxmins[i] = min(frame->bboxmin.v[i], aliasbboxmins[i]); //qmb :bounding box
		aliasbboxmaxs[i] = max(frame->bboxmax.v[i], aliasbboxmaxs[i]); //qmb :bounding box
	}


	pinframe = (trivertx_t *) (pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *) pinframe;
}

void *Mod_LoadAliasGroup(void * pin, maliasframedesc_t *frame) {
	daliasgroup_t *pingroup;
	int i, numframes;
	daliasinterval_t *pin_intervals;
	void *ptemp;

	pingroup = (daliasgroup_t *) pin;

	numframes = LittleLong(pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];

		aliasbboxmins[i] = min(frame->bboxmin.v[i], aliasbboxmins[i]); //qmb :bounding box
		aliasbboxmaxs[i] = max(frame->bboxmax.v[i], aliasbboxmaxs[i]); //qmb :bounding box
	}

	pin_intervals = (daliasinterval_t *) (pingroup + 1);

	frame->interval = LittleFloat(pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++) {
		poseverts[posenum] = (trivertx_t *) ((daliasframe_t *) ptemp + 1);
		posenum++;

		ptemp = (trivertx_t *) ((daliasframe_t *) ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================

int GL_SkinSplitShirt(byte *in, int width, int height, int bits, char *name) {
	byte data[512 * 512];
	byte *out = data;
	int i, pixels, passed, texnum;
	byte pixeltest[16];

	for (i = 0; i < 16; i++)
		pixeltest[i] = (bits & (1 << i)) != 0;
	pixels = width*height;
	passed = 0;
	while (pixels--) {
		if (pixeltest[*in >> 4] && *in != 0 && *in != 255) {
			passed++;
			// turn to white while copying
			if (*in >= 128 && *in < 224) // backwards ranges
				*out = (*in & 15) ^ 15;
			else
				*out = *in & 15;
		} else
			*out = 0;
		in++;
		out++;
	}

	if (passed) {
		texnum = TextureManager::LoadInternTexture(name, width, height, out, true, false, 1, gl_sincity.getBool());
		return texnum;
	} else {
		return 0;
	}
}

int GL_SkinSplit(byte *in, int width, int height, int bits, char *name) {
	byte data[512 * 512];
	byte *out = data;
	int i, pixels, passed, texnum;
	byte pixeltest[16];

	for (i = 0; i < 16; i++)
		pixeltest[i] = (bits & (1 << i)) != 0;
	pixels = width*height;
	passed = 0;
	while (pixels--) {
		if (pixeltest[*in >> 4] && *in != 0 && *in != 255) {
			passed++;
			*out = *in;
		} else
			*out = 0;
		*in++;
		*out++;
	}

	if (passed) {
		texnum = TextureManager::LoadInternTexture(name, width, height, out, true, false, 1, gl_sincity.getBool());
		return texnum;
	} else {
		return 0;
	}

}

/**
 * Fill background pixels so mipmapping doesn't have haloes - Ed
 */
typedef struct {
	short x, y;
} floodfill_t;

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void Mod_FloodFillSkin(byte *skin, int skinwidth, int skinheight) {
	byte fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0;
	int filledcolor = -1;
	int i;

	if (filledcolor == -1) {
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) { // alpha 1.0
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255)) {
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt) {
		int x = fifo[outpt].x, y = fifo[outpt].y;
		int fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0) FLOODFILL_STEP(-1, -1, 0);
		if (x < skinwidth - 1) FLOODFILL_STEP(1, 1, 0);
		if (y > 0) FLOODFILL_STEP(-skinwidth, 0, -1);
		if (y < skinheight - 1) FLOODFILL_STEP(skinwidth, 0, 1);
		skin[x + skinwidth * y] = fdc;
	}
}

void *Mod_LoadAllSkins(int numskins, daliasskintype_t *pskintype) {
	byte *texels;
	daliasskingroup_t *pinskingroup;
	int groupskins;
	daliasskininterval_t *pinskinintervals;
	char name[64], model[64], model2[64];

	byte *skin = (byte *) (pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	int s = pheader->skinwidth * pheader->skinheight;

	for (int i = 0; i < numskins; i++) {
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);

			// save 8 bit texels for the player model to remap
			if (!Q_strcmp(loadmodel->name, "progs/player.mdl")) {
				texels = (byte *) Hunk_AllocName(s, loadname);
				pheader->texels[i] = texels - (byte *) pheader;
				Q_memcpy(texels, (byte *) (pskintype + 1), s);
				GL_SkinSplitShirt(texels, pheader->skinwidth, pheader->skinheight, 0x0040, model);
			}

			//Tomaz skin naming: blah_0.tga
			COM_StripExtension(loadmodel->name, model);
			sprintf(model2, "%s_%i", model, i);
			pheader->gl_texturenum[i][0] =
					pheader->gl_texturenum[i][1] =
					pheader->gl_texturenum[i][2] =
					pheader->gl_texturenum[i][3] =
					TextureManager::LoadExternTexture(model2, false, true, gl_sincity.getBool());
			if (pheader->gl_texturenum[i][0] == 0) {
				//Darkplaces skin naming: blah.mdl_0.tga
				sprintf(model2, "%s_%i", loadmodel->name, i); //qmb :loardhavoc's skin naming
				pheader->gl_texturenum[i][0] =
						pheader->gl_texturenum[i][1] =
						pheader->gl_texturenum[i][2] =
						pheader->gl_texturenum[i][3] =
						TextureManager::LoadExternTexture(model2, false, true, gl_sincity.getBool());

				if (pheader->gl_texturenum[i][0] == 0)// did not find a matching TGA...
				{
					sprintf(name, "%s_%i", loadmodel->name, i);
					pheader->gl_texturenum[i][0] =
							pheader->gl_texturenum[i][1] =
							pheader->gl_texturenum[i][2] =
							pheader->gl_texturenum[i][3] =
							TextureManager::LoadInternTexture(name, pheader->skinwidth, pheader->skinheight, (byte *) (pskintype + 1), true, false, 1, gl_sincity.getBool());
				}
			}

			pskintype = (daliasskintype_t *) ((byte *) (pskintype + 1) + s);
		} else {
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *) pskintype;
			groupskins = LittleLong(pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *) (pinskingroup + 1);

			pskintype = (daliasskintype_t *) (pinskinintervals + groupskins);

			int j;
			for (j = 0; j < groupskins; j++) {
				Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);
				if (j == 0) {
					texels = (byte *) Hunk_AllocName(s, loadname);
					pheader->texels[i] = texels - (byte *) pheader;
					Q_memcpy(texels, (byte *) (pskintype), s);
				}
				sprintf(name, "%s_%i_%i", loadmodel->name, i, j);
				pheader->gl_texturenum[i][j & 3] = TextureManager::LoadInternTexture(name, pheader->skinwidth, pheader->skinheight, (byte *) (pskintype), true, false, 1, gl_sincity.getBool());
				pskintype = (daliasskintype_t *) ((byte *) (pskintype) + s);
			}
			int k = j;
			for ( ; j < 4; j++)
				pheader->gl_texturenum[i][j & 3] = pheader->gl_texturenum[i][j - k];
		}
	}

	return (void *) pskintype;
}

//=========================================================================

void Mod_LoadAliasModel(model_t *mod, void *buffer) {
	stvert_t *pinstverts;
	dtriangle_t *pintriangles;
	int numframes;
	daliasframetype_t *pframetype;
	daliasskintype_t *pskintype;

	int start = Hunk_LowMark();

	mdl_t *pinmodel = (mdl_t *) buffer;

	int version = LittleLong(pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod->name, version, ALIAS_VERSION);

	// allocate space for a working header, plus all the data except the frames, skin and group info
	int size = sizeof (aliashdr_t) + (LittleLong(pinmodel->numframes) - 1) * sizeof (pheader->frames[0]);
	pheader = (aliashdr_t *) Hunk_AllocName(size, loadname);

	mod->flags = LittleLong(pinmodel->flags);

	// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = LittleFloat(pinmodel->boundingradius);
	pheader->numskins = LittleLong(pinmodel->numskins);
	pheader->skinwidth = LittleLong(pinmodel->skinwidth);
	pheader->skinheight = LittleLong(pinmodel->skinheight);

	pheader->numverts = LittleLong(pinmodel->numverts);

	if (pheader->numverts <= 0)
		Sys_Error("model %s has no vertices", mod->name);

	if (pheader->numverts > MAXALIASVERTS)
		Sys_Error("model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong(pinmodel->numtris);

	if (pheader->numtris <= 0)
		Sys_Error("model %s has no triangles", mod->name);

	pheader->numframes = LittleLong(pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Sys_Error("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pheader->size = LittleFloat(pinmodel->size);
	mod->synctype = (synctype_t) LittleLong(pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (int i = 0; i < 3; i++) {
		pheader->scale[i] = LittleFloat(pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat(pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat(pinmodel->eyeposition[i]);
	}

	// load the skins
	pskintype = (daliasskintype_t *) & pinmodel[1];
	pskintype = (daliasskintype_t *) Mod_LoadAllSkins(pheader->numskins, pskintype);

	// load base s and t vertices
	pinstverts = (stvert_t *) pskintype;

	for (int i = 0; i < pheader->numverts; i++) {
		stverts[i].onseam = LittleLong(pinstverts[i].onseam);
		stverts[i].s = LittleLong(pinstverts[i].s);
		stverts[i].t = LittleLong(pinstverts[i].t);
	}

	// load triangle lists
	pintriangles = (dtriangle_t *) & pinstverts[pheader->numverts];
	pheader->indecies = (byte *) & pinstverts[pheader->numverts] - (byte *) pheader;

	for (int i = 0; i < pheader->numtris; i++) {
		triangles[i].facesfront = LittleLong(pintriangles[i].facesfront);

		for (int j = 0; j < 3; j++) {
			triangles[i].vertindex[j] = LittleLong(pintriangles[i].vertindex[j]);
		}
	}

	// load the frames
	posenum = 0;
	pframetype = (daliasframetype_t *) & pintriangles[pheader->numtris];

	aliasbboxmins[0] = aliasbboxmins[1] = aliasbboxmins[2] = 99999;
	aliasbboxmaxs[0] = aliasbboxmaxs[1] = aliasbboxmaxs[2] = -99999;

	for (int i = 0; i < numframes; i++) {
		aliasframetype_t frametype;

		frametype = (aliasframetype_t) LittleLong(pframetype->type);

		if (frametype == ALIAS_SINGLE) {
			pframetype = (daliasframetype_t *)Mod_LoadAliasFrame(pframetype + 1, &pheader->frames[i]);
		} else {
			pframetype = (daliasframetype_t *)Mod_LoadAliasGroup(pframetype + 1, &pheader->frames[i]);
		}
	}

	pheader->numposes = posenum;
	mod->type = mod_alias;

	// FIXME: do this right :done right
	//qmb :bounding box
	for (int i = 0; i < 3; i++) {
		mod->mins[i] = min(-16, aliasbboxmins[i] * pheader->scale[i] + pheader->scale_origin[i]);
		mod->maxs[i] = max(16, aliasbboxmaxs[i] * pheader->scale[i] + pheader->scale_origin[i]);
	}

	// build the draw lists
	GL_MakeAliasModelDisplayLists(mod, pheader);

	// move the complete, relocatable alias model to the cache
	int end = Hunk_LowMark();
	int total = end - start;

	mod->cache = MemoryObj::Alloc(MemoryObj::CACHE, loadname, total);
	if (!mod->cache->getData())
		return;
	Q_memcpy(mod->cache->getData(), pheader, total);

	Hunk_FreeToLowMark(start);
}

void Mod_LoadQ2AliasModel(model_t *mod, void *buffer) {
	int i, j, version, numframes, size, *pinglcmd, *poutglcmd, start, end, total;
	md2_t *pinmodel, *pheader;
	md2triangle_t *pintriangles, *pouttriangles;
	md2frame_t *pinframe, *poutframe;
	char *pinskins;

	char model[64], modelFilename[64];

	start = Hunk_LowMark();

	pinmodel = (md2_t *) buffer;

	version = LittleLong(pinmodel->version);
	if (version != MD2ALIAS_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)",
			mod->name, version, MD2ALIAS_VERSION);

	mod->type = mod_alias;
	mod->aliastype = ALIASTYPE_MD2;

	// LordHavoc: see pheader ofs adjustment code below for why this is bigger
	size = LittleLong(pinmodel->ofs_end) + sizeof (md2_t);

	if (size <= 0 || size >= MD2MAX_SIZE)
		Sys_Error("%s is not a valid model", mod->name);
	pheader = (md2_t *) Hunk_AllocName(size, loadname);

	mod->flags = 0; // there are no MD2 flags

	// endian-adjust and copy the data, starting with the alias model header
	for (i = 0; i < 17; i++) // LordHavoc: err... FIXME or something...
		((int*) pheader)[i] = LittleLong(((int *) pinmodel)[i]);
	mod->numframes = numframes = pheader->num_frames;
	mod->synctype = ST_RAND;

	if (pheader->ofs_skins <= 0 || pheader->ofs_skins >= pheader->ofs_end)
		Sys_Error("%s is not a valid model", mod->name);
	if (pheader->ofs_st <= 0 || pheader->ofs_st >= pheader->ofs_end)
		Sys_Error("%s is not a valid model", mod->name);
	if (pheader->ofs_tris <= 0 || pheader->ofs_tris >= pheader->ofs_end)
		Sys_Error("%s is not a valid model", mod->name);
	if (pheader->ofs_frames <= 0 || pheader->ofs_frames >= pheader->ofs_end)
		Sys_Error("%s is not a valid model", mod->name);
	if (pheader->ofs_glcmds <= 0 || pheader->ofs_glcmds >= pheader->ofs_end)
		Sys_Error("%s is not a valid model", mod->name);

	if (pheader->num_tris < 1 || pheader->num_tris > MD2MAX_TRIANGLES)
		Sys_Error("%s has invalid number of triangles: %i", mod->name, pheader->num_tris);
	if (pheader->num_xyz < 1 || pheader->num_xyz > MD2MAX_VERTS)
		Sys_Error("%s has invalid number of vertices: %i", mod->name, pheader->num_xyz);
	if (pheader->num_frames < 1 || pheader->num_frames > MD2MAX_FRAMES)
		Sys_Error("%s has invalid number of frames: %i", mod->name, pheader->num_frames);
	if (pheader->num_skins < 0 || pheader->num_skins > MD2MAX_SKINS)
		Sys_Error("%s has invalid number of skins: %i", mod->name, pheader->num_skins);

	// LordHavoc: adjust offsets in new model to give us some room for the bigger header
	// cheap offsetting trick, just offset it all by the pheader size...mildly wasteful
	for (i = 0; i < 7; i++)
		((int*) &pheader->ofs_skins)[i] += sizeof (pheader);

	if (pheader->num_skins == 0)
		pheader->num_skins++;
	// load the skins
	if (pheader->num_skins) {
		pinskins = (char *) ((byte *) pinmodel + LittleLong(pinmodel->ofs_skins));
		for (i = 0; i < pheader->num_skins; i++) {
			COM_StripExtension(mod->name, model);
			sprintf(modelFilename, "%s_%i", model, i);
			// tomazquake external skin naming: blah_0.tga
			pheader->gl_texturenum[i] = TextureManager::LoadExternTexture(modelFilename, false, true, gl_sincity.getBool());
			if (pheader->gl_texturenum[i] == 0) {
				// darkplaces external skin naming: blah.mdl_0.tga
				sprintf(modelFilename, "%s_%i", mod->name, i);
				pheader->gl_texturenum[i] = TextureManager::LoadExternTexture(modelFilename, false, true, gl_sincity.getBool());

				if (pheader->gl_texturenum[i] == 0)
					pheader->gl_texturenum[i] = TextureManager::LoadExternTexture(pinskins, false, true, gl_sincity.getBool());
			}

			pinskins += MD2MAX_SKINNAME;
		}
	}

	// load triangles
	pintriangles = (md2triangle_t *) ((byte *) pinmodel + LittleLong(pinmodel->ofs_tris));
	pouttriangles = (md2triangle_t *) ((byte *) pheader + pheader->ofs_tris);
	// swap the triangle list
	for (i = 0; i < pheader->num_tris; i++) {
		for (j = 0; j < 3; j++) {
			pouttriangles->index_xyz[j] = LittleShort(pintriangles->index_xyz[j]);
			pouttriangles->index_st[j] = LittleShort(pintriangles->index_st[j]);
			if (pouttriangles->index_xyz[j] >= pheader->num_xyz)
				Sys_Error("%s has invalid vertex indices", mod->name);
			if (pouttriangles->index_st[j] >= pheader->num_st)
				Sys_Error("%s has invalid vertex indices", mod->name);
		}
		pintriangles++;
		pouttriangles++;
	}

	//
	// load the frames
	//
	pinframe = (md2frame_t *) ((byte *) pinmodel + LittleLong(pinmodel->ofs_frames));
	poutframe = (md2frame_t *) ((byte *) pheader + pheader->ofs_frames);
	for (i = 0; i < numframes; i++) {
		for (j = 0; j < 3; j++) {
			poutframe->scale[j] = LittleFloat(pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}

		for (j = 0; j < 17; j++)
			poutframe->name[j] = pinframe->name[j];

		for (j = 0; j < pheader->num_xyz; j++) {
			poutframe->verts[j].v[0] = pinframe->verts[j].v[0];
			poutframe->verts[j].v[1] = pinframe->verts[j].v[1];
			poutframe->verts[j].v[2] = pinframe->verts[j].v[2];
			poutframe->verts[j].lightnormalindex = pinframe->verts[j].lightnormalindex;
		}

		pinframe = (md2frame_t *) & pinframe->verts[j].v[0];
		poutframe = (md2frame_t *) & poutframe->verts[j].v[0];
	}

	// LordHavoc: I may fix this at some point
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -64;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 64;

	// load the draw list
	pinglcmd = (int *) ((byte *) pinmodel + LittleLong(pinmodel->ofs_glcmds));
	poutglcmd = (int *) ((byte *) pheader + pheader->ofs_glcmds);
	for (i = 0; i < pheader->num_glcmds; i++)
		*poutglcmd++ = LittleLong(*pinglcmd++);

	// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark();
	total = end - start;

	mod->cache = MemoryObj::Alloc(MemoryObj::CACHE, loadname, total);
	if (!mod->cache->getData())
		return;
	Q_memcpy(mod->cache->getData(), pheader, total);

	Hunk_FreeToLowMark(start);
}

//=============================================================================

void Mod_Sprite_StripExtension(char *in, char *out) {
	char *end;
	end = in + Q_strlen(in);
	if ((end - in) >= 6)
		if (Q_strcmp(end - 6, ".spr32") == 0)
			end -= 6;
	if ((end - in) >= 4)
		if (Q_strcmp(end - 4, ".spr") == 0)
			end -= 4;
	while (in < end)
		*out++ = *in++;
	*out++ = 0;
}

int Mod_Sprite_Bits(char *in) {
	char *end;
	end = in + Q_strlen(in);
	if ((end - in) >= 6)
		if (Q_strcmp(end - 6, ".spr32") == 0)
			return 4;
	return 1;
}

void * Mod_LoadSpriteFrame(void * pin, mspriteframe_t **ppframe, int framenum) {
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;
	int width, height, size, origin[2];
	char name[64], sprite[64], sprite2[64];

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong(pinframe->width);
	height = LittleLong(pinframe->height);
	size = width * height;

	pspriteframe = (mspriteframe_t *) Hunk_AllocName(sizeof (mspriteframe_t), loadname);

	Q_memset(pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong(pinframe->origin[0]);
	origin[1] = LittleLong(pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	//TGA: begin
	Mod_Sprite_StripExtension(loadmodel->name, sprite);
	sprintf(sprite2, "%s_%i", sprite, framenum);

	pspriteframe->gl_texturenum = TextureManager::LoadExternTexture(sprite2, false, true, gl_sincity.getBool());
	if (pspriteframe->gl_texturenum == 0) {
		sprintf(name, "%s_%i", loadmodel->name, framenum);
		pspriteframe->gl_texturenum = TextureManager::LoadInternTexture(name, width, height, (byte *) (pinframe + 1), true, false, 1, gl_sincity.getBool());
	}

	return (void *) ((byte *) pinframe + sizeof (dspriteframe_t) + size);
}

void * Mod_LoadSpriteGroup(void * pin, mspriteframe_t **ppframe, int framenum) {
	dspritegroup_t *pingroup;
	mspritegroup_t *pspritegroup;
	int i, numframes;
	dspriteinterval_t *pin_intervals;
	float *poutintervals;
	void *ptemp;

	pingroup = (dspritegroup_t *) pin;

	numframes = LittleLong(pingroup->numframes);

	pspritegroup = (mspritegroup_t *) Hunk_AllocName(sizeof (mspritegroup_t) +
			(numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = (float *) Hunk_AllocName(numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++) {
		*poutintervals = LittleFloat(pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++) {
		ptemp = Mod_LoadSpriteFrame(ptemp, &pspritegroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}

void Mod_LoadSpriteModel(model_t *mod, void *buffer) {
	int i;
	int version;
	dsprite_t *pin;
	msprite_t *psprite;
	int numframes;
	int size;
	dspriteframetype_t *pframetype;

	pin = (dsprite_t *) buffer;

	version = LittleLong(pin->version);

	// LordHavoc: 32bit textures
	if (version != SPRITE_VERSION && version != SPRITE32_VERSION)
		Host_Error("Mod_Sprite_SharedSetup: unsupported version %i, only %i (quake) and %i (sprite32) supported", version, SPRITE_VERSION, SPRITE32_VERSION);

	numframes = LittleLong(pin->numframes);

	size = sizeof (msprite_t) + (numframes - 1) * sizeof (psprite->frames);

	//psprite = (msprite_t *)Hunk_AllocName (size, loadname);

	mod->cache = MemoryObj::Alloc(MemoryObj::CACHE, loadname, size);
	psprite = (msprite_t *) mod->cache->getData();

	psprite->type = LittleLong(pin->type);
	psprite->maxwidth = LittleLong(pin->width);
	psprite->maxheight = LittleLong(pin->height);
	psprite->beamlength = LittleFloat(pin->beamlength);
	mod->synctype = (synctype_t) LittleLong(pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = -psprite->maxheight / 2;
	mod->maxs[2] = psprite->maxheight / 2;

	// load the frames
	if (numframes < 1)
		Sys_Error("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *) (pin + 1);

	for (i = 0; i < numframes; i++) {
		spriteframetype_t frametype;

		frametype = (spriteframetype_t) LittleLong(pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE) {
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteFrame(pframetype + 1, &psprite->frames[i].frameptr, i);
		} else {
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteGroup(pframetype + 1, &psprite->frames[i].frameptr, i);
		}
	}

	mod->type = mod_sprite;
}

//=============================================================================
void Mod_Print(void) {
	int i;
	model_t *mod;

	Con_Printf("Cached models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		Con_Printf("%8p : %s\n", mod->cache->getData(), mod->name);
	}
}
