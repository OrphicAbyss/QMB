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
// gl_mesh.c: triangle model functions

#include "quakedef.h"
#include "Texture.h"
#include "gl_md3.h"

/*
=================================================================
ALIAS MODEL DISPLAY LIST GENERATION
=================================================================
 */

model_t *aliasmodel;
aliashdr_t *paliashdr;

bool used[8192];

// the command list holds counts and s/t values that are valid for
// every frame
int commands[8192];
int numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int vertexorder[8192];
int numorder;

int allverts, alltris;

int stripverts[128];
int striptris[128];
int stripcount;

int StripLength(int starttri, int startv) {
	int m1, m2;
	int j;
	mtriangle_t *last, *check;
	int k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv) % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv + 2) % 3];
	m2 = last->vertindex[(startv + 1) % 3];

	// look for a matching triangle
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < pheader->numtris; j++, check++) {
		if (check->facesfront != last->facesfront)
			continue;
		for (k = 0; k < 3; k++) {
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k + 1) % 3 ] != m2)
				continue;

			// this is the next part of the fan
			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			if (stripcount & 1)
				m2 = check->vertindex[ (k + 2) % 3 ];
			else
				m1 = check->vertindex[ (k + 2) % 3 ];

			stripverts[stripcount + 2] = check->vertindex[ (k + 2) % 3 ];
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < pheader->numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

int FanLength(int starttri, int startv) {
	int m1, m2;
	int j;
	mtriangle_t *last, *check;
	int k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv) % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv + 0) % 3];
	m2 = last->vertindex[(startv + 2) % 3];

	// look for a matching triangle
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < pheader->numtris; j++, check++) {
		if (check->facesfront != last->facesfront)
			continue;
		for (k = 0; k < 3; k++) {
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k + 1) % 3 ] != m2)
				continue;

			// this is the next part of the fan
			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			m2 = check->vertindex[ (k + 2) % 3 ];

			stripverts[stripcount + 2] = m2;
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < pheader->numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/**
 * Generate a list of trifans or strips for the model, which holds for all frames
 */
void BuildTris(void) {
	int startv;
	int len, bestlen, besttype;
	int bestverts[1024];
	int besttris[1024];

	// build tristrips
	numorder = 0;
	numcommands = 0;
	Q_memset(used, 0, sizeof (used));
	for (int i = 0; i < pheader->numtris; i++) {
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		bestlen = 0;
		for (int type = 0; type < 2; type++) { //	type = 1;
			for (startv = 0; startv < 3; startv++) {
				if (type == 1)
					len = StripLength(i, startv);
				else
					len = FanLength(i, startv);
				if (len > bestlen) {
					besttype = type;
					bestlen = len;
					for (int j = 0; j < bestlen + 2; j++)
						bestverts[j] = stripverts[j];
					for (int j = 0; j < bestlen; j++)
						besttris[j] = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (int j = 0; j < bestlen; j++)
			used[besttris[j]] = 1;

		if (besttype == 1)
			commands[numcommands++] = (bestlen + 2);
		else
			commands[numcommands++] = -(bestlen + 2);

		for (int j = 0; j < bestlen + 2; j++) {
			// emit a vertex into the reorder buffer
			int k = bestverts[j];
			vertexorder[numorder++] = k;

			// emit s/t coords into the commands stream
			float s = stverts[k].s;
			float t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->skinwidth / 2; // on back side
			s = (s + 0.5) / pheader->skinwidth;
			t = (t + 0.5) / pheader->skinheight;

			*(float *)&commands[numcommands++] = s;
			*(float *)&commands[numcommands++] = t;
		}
	}

	commands[numcommands++] = 0; // end of list marker

	Con_DPrintf("%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder, numcommands);

	allverts += numorder;
	alltris += pheader->numtris;
}

void GL_MakeAliasModelDisplayLists(model_t *m, aliashdr_t *hdr) {
	aliasmodel = m;
	paliashdr = hdr; // (aliashdr_t *)Mod_Extradata (m);

	// build it from scratch
	BuildTris(); // trifans or lists

	// save the data out
	paliashdr->poseverts = numorder;

	int *cmds = (int *) Hunk_Alloc(numcommands * 4);
	paliashdr->commands = (byte *) cmds - (byte *) paliashdr;
	Q_memcpy(cmds, commands, numcommands * 4);

	trivertx_t *verts = (trivertx_t *) Hunk_Alloc(paliashdr->numposes * paliashdr->poseverts * sizeof (trivertx_t));
	paliashdr->posedata = (byte *) verts - (byte *) paliashdr;
	for (int i = 0; i < paliashdr->numposes; i++)
		for (int j = 0; j < numorder; j++)
			*verts++ = poseverts[i][vertexorder[j]];
}

/*
=============================================================
  ALIAS MODELS DRAWING
=============================================================
 */

#define NUMVERTEXNORMALS	162

float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t shadevector;

extern vec3_t lightcolor; // LordHavoc: .lit support to the definitions at the top

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
	;

float *shadedots = r_avertexnormal_dots[0];

int lastposenum0;
int lastposenum;

void GL_DrawAliasFrame(aliashdr_t *paliashdr, int posenum, int shell, int cell) {
	float l;
	vec3_t colour, normal;
	trivertx_t *verts;
	int *order;
	int count;

	//cell shading
	vec3_t lightVector;

	lightVector[0] = 0;
	lightVector[1] = 0;
	lightVector[2] = 1;

	lastposenum = posenum;

	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	if (r_celshading.getBool() || r_vertexshading.getBool()) {
		//setup for shading
		glActiveTexture(GL_TEXTURE1_ARB);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_1D);
		if (r_celshading.getBool())
			glBindTexture(GL_TEXTURE_1D, TextureManager::celtexture);
		else
			glBindTexture(GL_TEXTURE_1D, TextureManager::vertextexture);

	}

	while (1) {
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break; // done
		if (count < 0) {
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		} else
			glBegin(GL_TRIANGLE_STRIP);

		do {

			// normals and vertexes come from the frame list
			//qmb :alias coloured lighting
			l = shadedots[verts->lightnormalindex];
			VectorScale(lightcolor, l, colour);
			//if (colour[0] > 1)	colour[0] = 1;
			//if (colour[1] > 1)	colour[1] = 1;
			//if (colour[2] > 1)	colour[2] = 1;

			if ((!shell) && (!cell)) glColor3f(colour[0], colour[1], colour[2]);

			normal[0] = (r_avertexnormals[verts->lightnormalindex][0]);
			normal[1] = (r_avertexnormals[verts->lightnormalindex][1]);
			normal[2] = (r_avertexnormals[verts->lightnormalindex][2]);

			glNormal3fv(&r_avertexnormals[verts->lightnormalindex][0]);

			if (!shell) {
				// texture coordinates come from the draw list
				glMultiTexCoord1f(GL_TEXTURE1_ARB, bound(0, DotProduct(shadevector, normal), 1));
				glMultiTexCoord2f(GL_TEXTURE0_ARB, ((float *) order)[0], ((float *) order)[1]);
				glVertex3f(verts->v[0], verts->v[1], verts->v[2]);
			} else {
				glTexCoord2f(((float *) order)[0] + realtime * 2, ((float *) order)[1] + realtime * 2);
				glVertex3f(r_avertexnormals[verts->lightnormalindex][0] * 5 + verts->v[0],
						r_avertexnormals[verts->lightnormalindex][1] * 5 + verts->v[1],
						r_avertexnormals[verts->lightnormalindex][2] * 5 + verts->v[2]);
			}
			order += 2;
			verts++;
		} while (--count);

		glEnd();
	}


	if (r_celshading.getBool() || r_vertexshading.getBool()) {
		//setup for shading
		glDisable(GL_TEXTURE_1D);
		glActiveTexture(GL_TEXTURE0_ARB);
	}
}

void GL_DrawAliasFrameNew(aliashdr_t *paliashdr, int posenum, int shell, int cell) {
	//float 	l, colour[3];
	//trivertx_t	*verts;
	//int		*order;
	//int		count;

	float *texcoords;
	byte *vertices;
	int *indecies;


	lastposenum = posenum;

	texcoords = (float *) ((byte *) paliashdr + paliashdr->texcoords);
	indecies = (int *) ((byte *) paliashdr + paliashdr->indecies + 4);
	vertices = (byte *) ((byte *) (paliashdr + paliashdr->posedata + posenum * paliashdr->poseverts));

	glVertexPointer(3, GL_BYTE, 1, vertices);
	glEnableClientState(GL_VERTEX_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glDrawElements(GL_TRIANGLES, paliashdr->numtris * 3, GL_UNSIGNED_INT, indecies);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	/*
	glBegin (GL_TRIANGLES);
	do {
		// normals and vertexes come from the frame list
		//qmb :alias coloured lighting
		l = shadedots[verts->lightnormalindex];
		VectorScale(lightcolor, l, colour);
		//if (colour[0] > 1)	colour[0] = 1;
		//if (colour[1] > 1)	colour[1] = 1;
		//if (colour[2] > 1)	colour[2] = 1;

		if ((!shell)&&(!cell)) glColor3f (colour[0], colour[1], colour[2]);

		glNormal3fv(&r_avertexnormals[verts->lightnormalindex][0]);

		if (!shell){
			// texture coordinates come from the draw list
			glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			glVertex3f (verts->v[0], verts->v[1], verts->v[2]);
		}else{
			glTexCoord2f (((float *)order)[0] + realtime*2, ((float *)order)[1] + realtime*2);
			glVertex3f (r_avertexnormals[verts->lightnormalindex][0] * 5 + verts->v[0],
						r_avertexnormals[verts->lightnormalindex][1] * 5 + verts->v[1],
						r_avertexnormals[verts->lightnormalindex][2] * 5 + verts->v[2]);
		}
		order += 2;
		verts++;
	} while (--count);

	glEnd ();*/
}

void GL_DrawAliasBlendedFrame(aliashdr_t *paliashdr, int pose1, int pose2, float blend, int shell, int cell) {
	//float       l;
	trivertx_t* verts1;
	trivertx_t* verts2;
	int* order;
	int count;
	vec3_t colour, normal;

	//shell and new blending
	vec3_t iblendshell;
	vec3_t blendshell;
	float iblend;

	//cell shading
	vec3_t lightVector;

	lightVector[0] = 0;
	lightVector[1] = 0;
	lightVector[2] = 1;

	if (r_celshading.getBool() || r_vertexshading.getBool()) {
		//setup for shading
		glActiveTexture(GL_TEXTURE1_ARB);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_1D);
		if (r_celshading.getBool())
			glBindTexture(GL_TEXTURE_1D, TextureManager::celtexture);
		else
			glBindTexture(GL_TEXTURE_1D, TextureManager::vertextexture);

	}

	lastposenum0 = pose1;
	lastposenum = pose2;

	verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	//LH: shell blending
	iblend = 1.0 - blend;
	if (shell) {
		iblendshell[0] = iblend * 5;
		iblendshell[1] = iblend * 5;
		iblendshell[2] = iblend * 5;
		blendshell[0] = blend * 5;
		blendshell[1] = blend * 5;
		blendshell[2] = blend * 5;
	}
	for (;;) {
		// get the vertex count and primitive type
		count = *order++;

		if (!count) break;

		if (count < 0) {
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		} else {
			glBegin(GL_TRIANGLE_STRIP);
		}
		do {
			// normals and vertexes come from the frame list
			// blend the light intensity from the two frames together
			colour[0] = colour[1] = colour[2] = shadedots[verts1->lightnormalindex] * iblend + shadedots[verts2->lightnormalindex] * blend;
			colour[0] *= lightcolor[0];
			colour[1] *= lightcolor[1];
			colour[2] *= lightcolor[2];

			//if (colour[0] > 1)	colour[0] = 1;
			//if (colour[1] > 1)	colour[1] = 1;
			//if (colour[2] > 1)	colour[2] = 1;
			if ((!shell) && (!cell)) glColor3f(colour[0], colour[1], colour[2]);

			normal[0] = (r_avertexnormals[verts1->lightnormalindex][0] * iblend + r_avertexnormals[verts2->lightnormalindex][0] * blend);
			normal[1] = (r_avertexnormals[verts1->lightnormalindex][1] * iblend + r_avertexnormals[verts2->lightnormalindex][1] * blend);
			normal[2] = (r_avertexnormals[verts1->lightnormalindex][2] * iblend + r_avertexnormals[verts2->lightnormalindex][2] * blend);

			glNormal3fv(&normal[0]);

			// blend the vertex positions from each frame together
			if (!shell) {
				// texture coordinates come from the draw list
				glMultiTexCoord1f(GL_TEXTURE1_ARB, bound(0, DotProduct(shadevector, normal), 1));
				glMultiTexCoord2f(GL_TEXTURE0_ARB, ((float *) order)[0], ((float *) order)[1]);
				glVertex3f(verts1->v[0] * iblend + verts2->v[0] * blend,
						verts1->v[1] * iblend + verts2->v[1] * blend,
						verts1->v[2] * iblend + verts2->v[2] * blend);
			} else {
				glTexCoord2f(((float *) order)[0] + realtime * 2, ((float *) order)[1] + realtime * 2);
				glVertex3f(verts1->v[0] * iblend + verts2->v[0] * blend + r_avertexnormals[verts1->lightnormalindex][0] * iblendshell[0] + r_avertexnormals[verts2->lightnormalindex][0] * blendshell[0],
						verts1->v[1] * iblend + verts2->v[1] * blend + r_avertexnormals[verts1->lightnormalindex][1] * iblendshell[1] + r_avertexnormals[verts2->lightnormalindex][1] * blendshell[1],
						verts1->v[2] * iblend + verts2->v[2] * blend + r_avertexnormals[verts1->lightnormalindex][2] * iblendshell[2] + r_avertexnormals[verts2->lightnormalindex][2] * blendshell[2]);
			}

			order += 2;
			verts1++;
			verts2++;
		} while (--count);
		glEnd();
	}

	if (r_celshading.getBool() || r_vertexshading.getBool()) {
		//setup for shading
		glDisable(GL_TEXTURE_1D);
		glActiveTexture(GL_TEXTURE0_ARB);
	}
}

void R_SetupQ3AliasFrame(int frame, aliashdr_t *paliashdr, int shell) {
	int pose, numposes;
	float interval;
	//md3 stuff
	float *texcoos, *vertices;
	int *indecies;

	if ((frame >= paliashdr->numframes) || (frame < 0)) {
		Con_DPrintf("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) {
		interval = paliashdr->frames[frame].interval;
		pose += (int) (cl.time / interval) % numposes;
	}

	glDisable(GL_DEPTH);
	texcoos = (float *) ((byte *) paliashdr + paliashdr->texcoords);
	indecies = (int *) ((byte *) paliashdr + paliashdr->indecies);
	vertices = (float *) ((byte *) (paliashdr + paliashdr->posedata + pose * paliashdr->poseverts));
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glEnableClientState(GL_VERTEX_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoos);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glDrawElements(GL_TRIANGLES, paliashdr->numtris * 3, GL_UNSIGNED_INT, indecies);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnable(GL_DEPTH);
}

void R_SetupAliasBlendedFrame(int frame, aliashdr_t *paliashdr, entity_t* e, int shell, int cell) {
	float blend;

	if ((frame >= paliashdr->numframes) || (frame < 0)) {
		Con_DPrintf("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	int pose = paliashdr->frames[frame].firstpose;
	int numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) {
		e->frame_interval = paliashdr->frames[frame].interval;
		pose += (int) (cl.time / e->frame_interval) % numposes;
	} else {
		/* One tenth of a second is a good for most Quake animations.
		If the nextthink is longer then the animation is usually meant to pause
		(e.g. check out the shambler magic animation in shambler.qc).  If its
		shorter then things will still be smoothed partly, and the jumps will be
		less noticable because of the shorter time.  So, this is probably a good
		assumption. */
		e->frame_interval = 0.1f;
	}

	if (e->pose2 != pose) {
		e->frame_start_time = realtime;
		e->pose1 = e->pose2;
		e->pose2 = pose;
		blend = 0;
	} else {
		blend = (realtime - e->frame_start_time) / e->frame_interval;
	}

	if (cell) {
		glCullFace(GL_BACK);
		glEnable(GL_BLEND);
		glPolygonMode(GL_FRONT, GL_LINE);
		glLineWidth(2.0f);
		glEnable(GL_LINE_SMOOTH);
	}

	// wierd things start happening if blend passes 1
	if (cl.paused || blend > 1) {
		GL_DrawAliasFrame(paliashdr, e->pose2, shell, cell);
		blend = 1;
	} else
		GL_DrawAliasBlendedFrame(paliashdr, e->pose1, e->pose2, blend, shell, cell);

	if (cell) {
		glPolygonMode(GL_FRONT, GL_FILL);
		glDisable(GL_BLEND);
		glCullFace(GL_FRONT);
	}
}

void GL_DrawQ2AliasFrame(entity_t *e, md2_t *pheader, int lastpose, int pose, float lerp) {
	float ilerp;
	int *order, count;
	md2trivertx_t *verts1, *verts2;
	vec3_t scale1, translate1, scale2, translate2, d;
	md2frame_t *frame1, *frame2;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	//		glBlendFunc(GL_ONE, GL_ZERO);
	//		glDisable(GL_BLEND);
	glDepthMask(1);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	ilerp = 1.0 - lerp;

	//new version by muff - fixes bug, easier to read, faster (well slightly)
	frame1 = (md2frame_t *) ((char *) pheader + pheader->ofs_frames + (pheader->framesize * lastpose));
	frame2 = (md2frame_t *) ((char *) pheader + pheader->ofs_frames + (pheader->framesize * pose));

	VectorCopy(frame1->scale, scale1);
	VectorCopy(frame1->translate, translate1);
	VectorCopy(frame2->scale, scale2);
	VectorCopy(frame2->translate, translate2);
	verts1 = &frame1->verts[0];
	verts2 = &frame2->verts[0];
	order = (int *) ((char *) pheader + pheader->ofs_glcmds);

	glColor4f(1, 1, 1, 1); // FIXME - temporary shading for tutorial - NOT LordHavoc's original code

	while (1) {
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break; // done
		if (count < 0) {
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		} else
			glBegin(GL_TRIANGLE_STRIP);

		do {
			// normals and vertexes come from the frame list
			// blend the light intensity from the two frames together
			d[0] = d[1] = d[2] = shadedots[verts1->lightnormalindex] * ilerp + shadedots[verts2->lightnormalindex] * lerp;
			//d[0] = (shadedots[verts2->lightnormalindex] + shadedots[verts1->lightnormalindex])/2;
			d[0] *= lightcolor[0];
			d[1] *= lightcolor[1];
			d[2] *= lightcolor[2];
			glColor3f(d[0], d[1], d[2]);

			glTexCoord2f(((float *) order)[0], ((float *) order)[1]);
			glVertex3f((verts1[order[2]].v[0] * scale1[0] + translate1[0]) * ilerp + (verts2[order[2]].v[0] * scale2[0] + translate2[0]) * lerp,
					(verts1[order[2]].v[1] * scale1[1] + translate1[1]) * ilerp + (verts2[order[2]].v[1] * scale2[1] + translate2[1]) * lerp,
					(verts1[order[2]].v[2] * scale1[2] + translate1[2]) * ilerp + (verts2[order[2]].v[2] * scale2[2] + translate2[2]) * lerp);

			order += 3;
		} while (--count);

		glEnd();
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // normal alpha blend
	glDisable(GL_BLEND);
	glDepthMask(1);
	//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void R_SetupQ2AliasFrame(entity_t *e, md2_t *pheader) {
	int frame;
	float lerp; //, lerpscale;

	frame = e->frame;

	glPushMatrix();
	R_RotateForEntity(e);

	if ((frame >= pheader->num_frames) || (frame < 0)) {
		Con_DPrintf("R_SetupQ2AliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	if (e->draw_lastmodel == e->model) {
		if (frame != e->draw_pose) {
			e->draw_lastpose = e->draw_pose;
			e->draw_pose = frame;
			e->draw_lerpstart = cl.time;
			lerp = 0;
		} else
			lerp = (cl.time - e->draw_lerpstart) * 20.0;
	} else // uninitialized
	{
		e->draw_lastmodel = e->model;
		e->draw_lastpose = e->draw_pose = frame;
		e->draw_lerpstart = cl.time;
		lerp = 0;
	}
	if (lerp > 1) lerp = 1;

	GL_DrawQ2AliasFrame(e, pheader, e->draw_lastpose, frame, lerp);
	glPopMatrix();
}

int quadtexture;

void R_DrawAliasModel(entity_t *e) {
	extern void AddFire(vec3_t org, float size);

	int lnum;
	vec3_t dist;
	float add;
	model_t *clmodel;
	vec3_t mins, maxs;
	aliashdr_t *paliashdr;
	float an; //s, t,
	int anim;
	md2_t *pheader; // LH / muff
	int shell; //QMB :model shells

	//not implemented yet
	//	byte		c, *color;	//QMB :color map

	//does the model need a shell?
	if (cl.items & IT_QUAD && e == &cl.viewent)
		shell = true;
	else
		shell = false;

	//set get the model from the e
	clmodel = e->model;

	//work out its max and mins
	VectorAdd(e->origin, clmodel->mins, mins);
	VectorAdd(e->origin, clmodel->maxs, maxs);
	//make sure its in screen
	if (R_CullBox(mins, maxs) && e != &cl.viewent)
		return;

	//QMB: FIXME
	//should use a particle emitter linked to the model for its org
	//needs to be linked when the model is created and distroyed when
	//the entity is distroyed
	//check if its a fire and add the particle effect
	if (!Q_strcmp(clmodel->name, "progs/flame.mdl"))
		AddFire(e->origin, 4);

	if (!Q_strcmp(clmodel->name, "progs/flame2.mdl")) {
		AddFire(e->origin, 10);
		return; //do not draw the big fire model, its just a place holder for the particles
	}

	// get lighting information
	//QMB: FIXME
	//SHOULD CHANGE TO A PASSED VAR
	//get vertex normals (for lighting and shells)
	shadedots = r_avertexnormal_dots[((int) (e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	//make a default lighting direction
	an = e->angles[1] / 180 * M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1; //e->angles[0];
	VectorNormalize(shadevector);

	//get the light for the model
	R_LightPoint(e->origin); // LordHavoc: lightcolor is all that matters from this

	//work out lighting from the dynamic lights
	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		//if the light is alive
		if (cl_dlights[lnum].die >= cl.time) {
			//work out the distance to the light
			VectorSubtract(e->origin, cl_dlights[lnum].origin, dist);
			add = cl_dlights[lnum].radius - Length(dist);
			//if its close enough add light from it
			if (add > 0) {
				lightcolor[0] += add * cl_dlights[lnum].colour[0];
				lightcolor[1] += add * cl_dlights[lnum].colour[1];
				lightcolor[2] += add * cl_dlights[lnum].colour[2];
			}
		}
	}

	//scale lighting to floating point
	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);

	// locate the proper data
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_n_patches && gl_npatches.getBool()) {
		glEnable(GL_PN_TRIANGLES_ATI);
		glPNTrianglesiATI(GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, gl_npatches.getInt());

		if (true)
			glPNTrianglesiATI(GL_PN_TRIANGLES_POINT_MODE_ATI, GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI);
		else
			glPNTrianglesiATI(GL_PN_TRIANGLES_POINT_MODE_ATI, GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI);

		if (true)
			glPNTrianglesiATI(GL_PN_TRIANGLES_NORMAL_MODE_ATI, GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI);
		else
			glPNTrianglesiATI(GL_PN_TRIANGLES_NORMAL_MODE_ATI, GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI);
	}

	if (clmodel->aliastype == MD3IDHEADER) {
		//do nothing for testing
		if (!r_modeltexture.getBool()) {
			GL_DisableTMU(GL_TEXTURE0_ARB);
		}//disable texture if needed

		R_DrawQ3Model(e, false, false);

		if (!r_modeltexture.getBool()) {
			GL_EnableTMU(GL_TEXTURE0_ARB);
		}//enable texture if needed

		if (r_celshading.getBool() || r_outline.getBool()) {
			glCullFace(GL_BACK);
			glEnable(GL_BLEND);
			glPolygonMode(GL_FRONT, GL_LINE);

			if (e == &cl.viewent) {
				glLineWidth(1.0f);
			} else {
				glLineWidth(5.0f);
			}

			glEnable(GL_LINE_SMOOTH);

			GL_DisableTMU(GL_TEXTURE0_ARB);

			glColor3f(0.0, 0.0, 0.0);
			R_DrawQ3Model(e, false, true);
			glColor3f(1.0, 1.0, 1.0);

			GL_EnableTMU(GL_TEXTURE0_ARB);

			glPolygonMode(GL_FRONT, GL_FILL);
			glDisable(GL_BLEND);
			glCullFace(GL_FRONT);
		}

		if (shell) {
			glBindTexture(GL_TEXTURE_2D, quadtexture);
			glColor4f(1.0, 1.0, 1.0, 0.5);
			glEnable(GL_BLEND);
			R_DrawQ3Model(e, true, false);
			glDisable(GL_BLEND);
			glColor3f(1.0, 1.0, 1.0);
		}
	} else if (clmodel->aliastype != ALIASTYPE_MD2) {
		paliashdr = (aliashdr_t *) Mod_Extradata(e->model);
		c_alias_polys += paliashdr->numtris;

		glPushMatrix();

		//interpolate unless its the viewmodel
		if (e != &cl.viewent)
			R_BlendedRotateForEntity(e);
		else
			R_RotateForEntity(e);

		glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

		anim = (int) (cl.time * 10) & 3;
		glBindTexture(GL_TEXTURE_2D, paliashdr->gl_texturenum[e->skinnum][anim]);

		// draw all the triangles
		if (!r_modeltexture.getBool()) {
			GL_DisableTMU(GL_TEXTURE0_ARB);
		} else {
			//highlighting test code
			if (0 && gl_textureunits > 2) {
				GL_EnableTMU(GL_TEXTURE1_ARB);

				glBindTexture(GL_TEXTURE_2D, TextureManager::highlighttexture);

				glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
				glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
				glEnable(GL_TEXTURE_GEN_S);
				glEnable(GL_TEXTURE_GEN_T);
				//need correct mode
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
				glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_ADD);
				glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0);

				//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			}
		}

		glColor3fv(lightcolor);
		R_SetupAliasBlendedFrame(e->frame, paliashdr, e, false, false);
		glDisable(GL_TEXTURE_1D);

		if (r_celshading.getBool() || r_outline.getBool()) {
			glColor3f(0.0, 0.0, 0.0);
			R_SetupAliasBlendedFrame(e->frame, paliashdr, e, false, true);
			glColor3f(1.0, 1.0, 1.0);
		}

		if (!r_modeltexture.getBool()) {
			GL_EnableTMU(GL_TEXTURE0_ARB);
		} else {
			if (0 && gl_textureunits > 2) {
				//highlighting test code
				glDisable(GL_TEXTURE_GEN_S);
				glDisable(GL_TEXTURE_GEN_T);
				GL_DisableTMU(GL_TEXTURE1_ARB);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
		}
		glActiveTexture(GL_TEXTURE0_ARB);

		//colour map code... not working yet...
		/*		if (e->colormap != vid.colormap && !gl_nocolors.value)
				{
					if (paliashdr->gl_texturenumColorMap&&paliashdr->gl_texturenumColorMap){
						glBindTexture(GL_TEXTURE_2D,paliashdr->gl_texturenumColorMap);
						c = (byte)e->colormap & 0xF0;
						c += (c >= 128 && c < 224) ? 4 : 12; // 128-224 are backwards ranges
						color = (byte *) (&d_8to24table[c]);
						//glColor3fv(color);
						glColor3f(1.0,1.0,1.0);
					}
				}*/
		if (shell) {
			glBindTexture(GL_TEXTURE_2D, quadtexture);
			glColor4f(1.0, 1.0, 1.0, 0.5);
			glEnable(GL_BLEND);
			R_SetupAliasBlendedFrame(e->frame, paliashdr, e, true, false);
			glDisable(GL_BLEND);
			glColor3f(1.0, 1.0, 1.0);
		}
		glPopMatrix();

	} else {
		pheader = (md2_t *) Mod_Extradata(e->model);
		c_alias_polys += pheader->num_tris;

		glBindTexture(GL_TEXTURE_2D, pheader->gl_texturenum[e->skinnum]);
		R_SetupQ2AliasFrame(e, pheader);
	}

	if (gl_n_patches && gl_npatches.getBool()) {
		glDisable(GL_PN_TRIANGLES_ATI);
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}
//==================================================================================