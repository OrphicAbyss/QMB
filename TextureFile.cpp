#include "Texture.h"
#include "Image.h"
#include "FileManager.h"

Texture *TextureManager::LoadFile(const char *filename, bool complain) {
	FILE *f = NULL;
	char basename[128], name[128];

	FileManager::StripExtension(filename, basename);
	FileManager::MakeFilenameValid(basename);

	//png loading
	sprintf(name, "%s.png", basename);
	int filesize = FileManager::FOpenFile(name, &f);
	if (f)
		return LoadFilePNG(f, basename);

	//tga loading
	sprintf(name, "%s.tga", basename);
	filesize = FileManager::FOpenFile(name, &f);
	if (f)
		return LoadFileTGA(f, basename);

	//jpg loading
	sprintf(name, "%s.jpg", basename);
	filesize = FileManager::FOpenFile(name, &f);
	if (f)
		return LoadFileJPG(f, basename);

	//pcx loading
	sprintf(name, "%s.pcx", basename);
	filesize = FileManager::FOpenFile(name, &f);
	if (f)
		return LoadFilePCX(f, basename);

	if (complain)
		Con_Printf("Couldn't load %s with extension .png, .tga, .jpg or .pcx\n", filename);

	return NULL;
}

Texture *TextureManager::LoadFilePNG(FILE *f, char *name) {
	Texture *t = new Texture(name);
	Image::LoadPNG(f, name, t);
	return t;
}

Texture *TextureManager::LoadFileJPG(FILE *f, char *name) {
	Texture *t = new Texture(name);
	Image::LoadJPG(f, name, t);
	return t;
}

Texture *TextureManager::LoadFilePCX(FILE *f, char *name){
	Texture *t = new Texture(name);
	Image::LoadPCX(f, name, t);
	return t;
}

Texture *TextureManager::LoadFileTGA(FILE *f, char *name) {
	Texture *t = new Texture(name);
	Image::LoadTGA(f, name, t);
	return t;
}
