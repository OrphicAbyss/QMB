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

#include "gl_md2.h"
#include "FileManager.h"
#include "Texture.h"

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

	pheader = (md2_t *) Hunk_AllocName(size, mod->name);

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
			FileManager::StripExtension(mod->name, model);
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

		for (j = 0; j < 17; j++) {
			poutframe->name[j] = pinframe->name[j];
		}

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

	mod->cache = MemoryObj::Alloc(MemoryObj::CACHE, mod->name, total);
	if (!mod->cache->getData())
		return;
	memcpy(mod->cache->getData(), pheader, total);

	Hunk_FreeToLowMark(start);
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

void GL_DrawQ2AliasFrame(entity_t *e, md2_t *pheader, int lastpose, int pose, float lerp) {
    int count;
    vec3_t scale1, translate1, scale2, translate2, d;

	extern vec3_t shadedots;
	extern vec3_t lightcolor;

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    //		glBlendFunc(GL_ONE, GL_ZERO);
    //		glDisable(GL_BLEND);
    glDepthMask(1);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    float ilerp = 1.0 - lerp;

    //new version by muff - fixes bug, easier to read, faster (well slightly)
    md2frame_t *frame1 = (md2frame_t *) ((char *) pheader + pheader->ofs_frames + (pheader->framesize * lastpose));
    md2frame_t *frame2 = (md2frame_t *) ((char *) pheader + pheader->ofs_frames + (pheader->framesize * pose));

    VectorCopy(frame1->scale, scale1);
    VectorCopy(frame1->translate, translate1);
    VectorCopy(frame2->scale, scale2);
    VectorCopy(frame2->translate, translate2);
    md2trivertx_t *verts1 = &frame1->verts[0];
    md2trivertx_t *verts2 = &frame2->verts[0];
    int *order = (int *) ((char *) pheader + pheader->ofs_glcmds);

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