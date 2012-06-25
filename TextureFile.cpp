#include "Texture.h"

extern void LoadPNG(FILE *f, char *filename, Texture *tex);
extern void LoadJPG(FILE *f, char *filename, Texture *tex);
extern void LoadPCX(FILE *f, char *filename, Texture *texv);
extern void LoadTGA(FILE *f, char *filename, Texture *tex);

Texture *TextureManager::LoadFile(char *filename, bool complain) {
	FILE *f;
	char basename[128], name[128];
	int filesize = 0;

	COM_StripExtension(filename, basename);
	COM_MakeFilenameValid(basename);
	
	//png loading
	sprintf(name, "%s.png", basename);
	filesize = COM_FOpenFile(name, &f);
	if (f)
		return LoadFilePNG(f, basename);
		
	//tga loading
	sprintf(name, "%s.tga", basename);
	filesize = COM_FOpenFile(name, &f);
	if (f)
		return LoadFileTGA(f, basename);

	//jpg loading
	sprintf(name, "%s.jpg", basename);
	filesize = COM_FOpenFile(name, &f);
	if (f)
		return LoadFileJPG(f, basename);

	//pcx loading
	sprintf(name, "%s.pcx", basename);
	filesize = COM_FOpenFile(name, &f);
	if (f)
		return LoadFilePCX(f, basename);

	if (complain)
		Con_Printf("Couldn't load %s with extension .png, .tga, .jpg or .pcx\n", filename);

	return NULL;
}

Texture *TextureManager::LoadFilePNG(FILE *f, char *name) {
	Texture *t = new Texture(name);
	LoadPNG(f, name, t);
	return t;
}

Texture *TextureManager::LoadFileJPG(FILE *f, char *name) {
	Texture *t = new Texture(name);
	LoadJPG(f, name, t);
	return t;
}

Texture *TextureManager::LoadFilePCX(FILE *f, char *name){
	Texture *t = new Texture(name);
	LoadPCX(f, name, t);
	return t;
}

Texture *TextureManager::LoadFileTGA(FILE *f, char *name) {
	Texture *t = new Texture(name);
	LoadTGA(f, name, t);
	return t;
}
