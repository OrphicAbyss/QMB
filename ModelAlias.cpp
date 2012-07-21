#include "Model.h"
#include "gl_md3.h"

AliasModel::AliasModel() {
	this->type = ALIAS;
}

//AliasModel *AliasModel::LoadMD3(void *buffer) {
//	AliasModel *loading = new AliasModel();
//	md3header_t *header;
//	md3surface_t *surf;
//	int posn;
//	int surfstartposn;
//	char name[128];
//	
//	loading->data = new AliasHeader();
//	loading->data->numSkins = 0;
//	
//	//we want to work out the size of the model in memory
//	//size of surfaces = surface size + num_shaders * shader_mem size +
//	//		num_triangles * tri size + num_verts * textcoord size +
//	//		num_verts * vert_mem size
//	int surf_size = 0;
//	int mem_size = 0;
//
//	//pointer to header
//	header = (md3header_t *) buffer;
//	//pointer to the surface list
//	surf = (md3surface_t*) ((byte *) buffer + header->surface_offs);
//
//	Q_strcpy(loading->data->filename, "");
//	loading->data->flags = header->flags;	
//	loading->data->numSurfaces = header->num_surfaces;
//	loading->data->numFrames = header->num_frames;
//	loading->data->numTags = header->num_tags;
//	
//	loading->data->frames = new AliasFrame[header->num_frames];
//	for (int i=0; i < header->num_frames; i++) {
//		md3frame_t *frameDisk = (md3frame_t *)((byte *)header + header->frame_offs);
//		AliasFrame *frameMem = &loading->data->frames[i];
//		memcpy(frameMem, frameDisk, sizeof(md3frame_t));
//	}
//	
//	loading->data->tags = new AliasTag[header->num_tags];
//	for (int i=0; i < header->num_tags; i++) {
//		md3tag_t *tagDisk = (md3tag_t *)((byte *)header + header->tag_offs);
//		AliasTag *tagMem = &loading->data->tags[i];
//		memcpy(tagMem, tagDisk, sizeof(md3tag_t));
//	}
//
//	loading->data->surfaces = new AliasSurface[header->num_surfaces];
//	for (int i=0; i < header->num_surfaces; i++) {
//		md3surface_t *surfDisk = (md3surface_t *)((byte *)header + header->surface_offs);
//		AliasSurface *surfMem = &loading->data->surfaces[i];
//		surfMem->flags = surfDisk->flags;
//		Q_strcpy(surfMem->name, surfDisk->name);
//		surfMem->numFrames = surfDisk->num_surf_frames;
//		surfMem->numIndices = surfDisk->num_surf_tris * 3;
//		surfMem->numVerts = surfDisk->num_surf_verts;
//		surfMem->numShaders = surfDisk->num_surf_shaders;
//		
//		md3st_t *texCoord = (md3st_t *)((byte *)header + surfDisk->tc_offs);
//		surfMem->texCoords = new AliasTextureCoord[surfDisk->num_surf_verts];
//		for (int j=0; j < surfDisk->num_surf_verts; j++) {
//			surfMem->texCoords[j].s = texCoord[j][0];
//			surfMem->texCoords[j].t = texCoord[j][1];
//		}
//		
//		md3shader_t *shader = (md3shader_t *)((byte *)header + surfDisk->shader_offs);
//		
//	}
//	
//	surf_size = 0;
//	for (int i = 0; i < header->num_surfaces; i++) {
//		surf_size += sizeof (md3surface_mem_t);
//		surf_size += surf->num_surf_shaders * sizeof (md3shader_mem_t);
//		surf_size += surf->num_surf_tris * sizeof (md3tri_mem_t);
//		surf_size += surf->num_surf_verts * sizeof (md3st_mem_t);
//		surf_size += surf->num_surf_verts * surf->num_surf_frames * sizeof (md3vert_mem_t);
//
//		//goto next surf
//		surf = (md3surface_t*) ((byte *) surf + surf->end_offs);
//	}
//
//	//total size =	header size + num_frames * frame size + num_tags * tag size +
//	//		size of surfaces
//	mem_size = sizeof (md3header_mem_t);
//	mem_size += header->num_frames * sizeof (md3frame_mem_t);
//	mem_size += header->num_tags * sizeof (md3tag_mem_t);
//	mem_size += surf_size;
//
//	Con_DPrintf("Loading md3 model...%s (%s)\n", header->filename, mod->name);
//
//	mod->cache = MemoryObj::Alloc(MemoryObj::CACHE, (char *) &mod->name, mem_size);
//	mem_head = (md3header_mem_t *) mod->cache->getData();
//	if (!mod->cache->getData()) {
//		Con_Printf("Unable to allocate cache to load model: %s\n", mod->name);
//		return; //cache alloc failed
//	}
//
//	//setup more mem stuff
//	mod->type = mod_alias;
//	mod->aliastype = MD3IDHEADER;
//	mod->numframes = header->num_frames;
//
//	//copy stuff across from disk buffer to memory
//	posn = 0; //posn in new buffer
//
//	//copy header
//	memcpy(mem_head, header, sizeof (md3header_t));
//	posn += sizeof (md3header_mem_t);
//
//	//posn of frames
//	mem_head->offs_frames = posn;
//
//	//copy frames
//	memcpy((byte *) mem_head + mem_head->offs_frames, (byte *) header + header->frame_offs, sizeof (md3frame_t) * header->num_frames);
//	posn += sizeof (md3frame_mem_t) * header->num_frames;
//
//	//posn of tags
//	mem_head->offs_tags = posn;
//
//	//copy tags
//	memcpy((byte *) mem_head + mem_head->offs_tags, (byte *) header + header->tag_offs, sizeof (md3tag_t) * header->num_tags);
//	posn += sizeof (md3tag_mem_t) * header->num_tags;
//
//	//posn of surfaces
//	mem_head->offs_surfaces = posn;
//
//	//copy surfaces, one at a time
//	//get pointer to surface in file
//	surf = (md3surface_t *) ((byte *) header + header->surface_offs);
//	//get pointer to surface in memory
//	currentsurf = (md3surface_mem_t *) ((byte *) mem_head + posn);
//	surfstartposn = posn;
//
//	for (int i = 0; i < header->num_surfaces; i++) {
//		//copy size of surface
//		memcpy((byte *) mem_head + posn, (byte *) header + header->surface_offs, sizeof (md3surface_t));
//		posn += sizeof (md3surface_mem_t);
//
//		//posn of shaders for this surface
//		currentsurf->offs_shaders = posn - surfstartposn;
//
//		for (int j = 0; j < surf->num_surf_shaders; j++) {
//			//copy jth shader accross
//			memcpy((byte *) mem_head + posn, (byte *) surf + surf->shader_offs + j * sizeof (md3shader_t), sizeof (md3shader_t));
//			posn += sizeof (md3shader_mem_t); //copyed non-mem into mem one
//		}
//		//posn of tris for this surface
//		currentsurf->offs_tris = posn - surfstartposn;
//
//		//copy tri
//		memcpy((byte *) mem_head + posn, (byte *) surf + surf->tris_offs, sizeof (md3tri_t) * surf->num_surf_tris);
//		posn += sizeof (md3tri_mem_t) * surf->num_surf_tris;
//
//		//posn of tex coords in this surface
//		currentsurf->offs_tc = posn - surfstartposn;
//
//		//copy st
//		memcpy((byte *) mem_head + posn, (byte *) surf + surf->tc_offs, sizeof (md3st_t) * surf->num_surf_verts);
//		posn += sizeof (md3st_t) * surf->num_surf_verts;
//
//		//posn points to surface->verts
//		currentsurf->offs_verts = posn - surfstartposn;
//
//		//next to have to be redone
//		for (int j = 0; j < surf->num_surf_verts * surf->num_surf_frames; j++) {
//			float lat;
//			float lng;
//
//			//convert verts from shorts to floats
//			md3vert_mem_t *mem_vert = (md3vert_mem_t *) ((byte *) mem_head + posn);
//			md3vert_t *disk_vert = (md3vert_t *) ((byte *) surf + surf->vert_offs + j * sizeof (md3vert_t));
//			mem_vert->vec[0] = (float) disk_vert->vec[0] / 64.0f;
//			mem_vert->vec[1] = (float) disk_vert->vec[1] / 64.0f;
//			mem_vert->vec[2] = (float) disk_vert->vec[2] / 64.0f;
//
//			//work out normals
//			lat = (disk_vert->normal + 255) * (2 * 3.141592654f) / 256.0f;
//			lng = ((disk_vert->normal >> 8) & 255) * (2 * 3.141592654f) / 256.0f;
//			mem_vert->normal[0] = -(float) (sin(lat) * cos(lng));
//			mem_vert->normal[1] = (float) (sin(lat) * sin(lng));
//			mem_vert->normal[2] = (float) (cos(lat) * 1);
//
//			posn += sizeof (md3vert_mem_t); //copyed non-mem into mem one
//		}
//
//		//point to next surf (or eof)
//		surf = (md3surface_t*) ((byte *) surf + surf->end_offs);
//
//		//posn points to the end of this surface
//		currentsurf->offs_end = posn;
//		//next start of surf (if there is one)
//		surfstartposn = posn;
//	}
//	//posn should now equal mem_size
//	if (posn != mem_size) {
//		Con_Printf("Copied diffrent ammount to what was worked out, copied: %i worked out: %i\n", posn, mem_size);
//	}
//
//	VectorCopy(((md3frame_mem_t *) ((byte *) mem_head + mem_head->offs_frames))->mins, mod->mins);
//	VectorCopy(((md3frame_mem_t *) ((byte *) mem_head + mem_head->offs_frames))->maxs, mod->maxs);
//	mod->flags = mem_head->flags;
//
//	//get pointer to first surface
//	currentsurf = (md3surface_mem_t *) ((byte *) mem_head + mem_head->offs_surfaces);
//	for (int i = 0; i < mem_head->num_surfaces; i++) {
//		if (*(int *) currentsurf->id != MD3IDHEADER) {
//			Con_Printf("MD3 bad surface for: %s\n", mem_head->filename);
//
//		} else {
//			md3shader_mem_t *shader = (md3shader_mem_t *) ((byte *) currentsurf + currentsurf->offs_shaders);
//
//			for (int j = 0; j < currentsurf->num_surf_shaders; j++) {
//				//try loading texture here
//				sprintf(&name[0], "progs/%s", shader[j].name);
//
//				shader[j].texnum = GL_LoadTexImage(&name[0], false, true, gl_sincity.getBool());
//				if (shader[j].texnum == 0) {
//					Con_Printf("Model: %s  Texture missing: %s\n", mod->name, shader[j].name);
//				}
//			}
//		}
//		currentsurf = (md3surface_mem_t *) ((byte *) currentsurf + currentsurf->offs_end);
//	}
//}
