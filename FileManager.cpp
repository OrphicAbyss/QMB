#include <string.h>

#include "FileManager.h"
#include "quakedef.h"


searchpath_t *FileManager::searchpaths;

/**
 * If the requested file is inside a packfile, a new FILE * will be opened into
 *  the file.
 */
int FileManager::FOpenFile(const char *filename, FILE **file) {
	return FindFile(filename, NULL, file);
}

/**
 * filename never has a leading slash, but may contain directory walks 
 * returns a handle and a length
 * it may actually be inside a pak file
 */
int FileManager::OpenFile(const char *filename, int *handle) {
	return FindFile(filename, handle, NULL);
}

/**
 * Finds the file in the search path. Sets com_filesize and one of handle or file
 */
int FileManager::FindFile(const char *filename, int *handle, FILE **file) {
	char netpath[MAX_OSPATH];
	//char cachepath[MAX_OSPATH];
	pack_t *pak;
	int i;
	int findtime; //, cachetime;

	if (file && handle)
		Sys_Error("COM_FindFile: both handle and file set");
	if (!file && !handle)
		Sys_Error("COM_FindFile: neither handle or file set");

	// search through the path, one element at a time
	for (searchpath_t *search = searchpaths ; search; search = search->next) {
		// is the element a pak file?
		if (search->pack) {
			// look through all the pak file elements
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
				if (!Q_strcmp(pak->files[i].name, filename)) { // found it!
					Sys_Printf("PackFile: %s : %s\n", pak->filename, filename);
					if (handle) {
						*handle = pak->handle;
						Sys_FileSeek(pak->handle, pak->files[i].filepos);
					} else { // open a new file on the pakfile
						*file = fopen(pak->filename, "rb");
						if (*file)
							fseek(*file, pak->files[i].filepos, SEEK_SET);
					}
					com_filesize = pak->files[i].filelen;
					return com_filesize;
				}
		} else {
			// check a file in the directory tree
			sprintf(netpath, "%s/%s", search->filename, filename);

			findtime = Sys_FileTime(netpath);
			if (findtime == -1)
				continue;

			Sys_Printf("FindFile: %s\n", netpath);
			com_filesize = Sys_FileOpenRead(netpath, &i);
			if (handle)
				*handle = i;
			else {
				Sys_FileClose(i);
				*file = fopen(netpath, "rb");
			}
			return com_filesize;
		}
	}

	//Sys_Printf ("FindFile: can't find %s\n", filename);
	if (handle)
		*handle = -1;
	else
		*file = NULL;
	com_filesize = -1;
	return -1;
}

/**
 * The filename will be prefixed by the current game directory
 */
void FileManager::WriteFile(const char *filename, void *data, int len) {
	char name[MAX_OSPATH];

	sprintf(name, "%s/%s", com_gamedir, filename);

	int handle = Sys_FileOpenWrite(name);
	if (handle == -1) {
		Sys_Printf("COM_WriteFile: failed on %s\n", name);
		return;
	}

	Sys_Printf("COM_WriteFile: %s\n", name);
	Sys_FileWrite(handle, data, len);
	Sys_FileClose(handle);
}

/**
 * If it is a pak file handle, don't really close it
 */
void FileManager::CloseFile(int h) {
	for (searchpath_t *s = searchpaths; s; s = s->next)
		if (s->pack && s->pack->handle == h)
			return;

	Sys_FileClose(h);
}

void FileManager::MakeFilenameValid(char *data) {
	for (char *c = data; *c; c++)
		if (*c == '*')
			*c = '#';
}

void FileManager::StripExtension(const char *in, char *out) {
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

char *FileManager::FileExtension(char *in) {
	char *extension = strstr(in, ".");
	
	if (extension) {
		extension++;
	} else {
		int length = strlen(in);
		extension = in + length;
	}
	return extension;
}

void FileManager::FileBase(const char *in, char *out) {
	const char *s = in + Q_strlen(in) - 1;
	while (s != in && *s != '.')
		s--;

	const char *s2 = s;
	while (s2 != in && *s2 != '/')
		s2--;

	if (s - s2 < 2)
		Q_strcpy(out, "?model?");
	else {
		s--;
		Q_strncpy(out, s2 + 1, s - s2);
		out[s - s2] = 0;
	}
}

void FileManager::DefaultExtension(char *path, const char *extension) {
	char *src = path + Q_strlen(path) - 1;

	while (*src != '/' && src != path) {
		if (*src == '.')
			return; // it has an extension
		src--;
	}

	Q_strcat(path, extension);
}

void FileManager::PathCmd(void) {
	Con_Printf("Current search path:\n");
	for (searchpath_t *s = FileManager::searchpaths; s; s = s->next) {
		if (s->pack) {
			Con_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		} else
			Con_Printf("%s\n", s->filename);
	}
}

/**
 * Sets com_gamedir, adds the directory to the head of the path,
 * then loads and adds pak1.pak pak2.pak ...
 */
void FileManager::AddGameDirectory(char *dir) {
	char pakfile[MAX_OSPATH];

	Q_strcpy(com_gamedir, dir);

	// add the directory to the search path
	searchpath_t *search = (searchpath_t *) Hunk_Alloc(sizeof (searchpath_t));
	Q_strcpy(search->filename, dir);
	search->next = FileManager::searchpaths;
	FileManager::searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (int i = 0;; i++) {
		sprintf(pakfile, "%s/pak%i.pak", dir, i);
		pack_t *pak = LoadPackFile(pakfile);
		if (!pak)
			break;
		search = (searchpath_t *) Hunk_Alloc(sizeof (searchpath_t));
		search->pack = pak;
		search->next = FileManager::searchpaths;
		FileManager::searchpaths = search;
	}
	// add the contents of the parms.txt file to the end of the command line
}

void FileManager::AddPackToPath(char *pak) {
	searchpath_t *search = (searchpath_t *) Hunk_Alloc(sizeof (searchpath_t));
	if (!Q_strcmp(FileManager::FileExtension(pak), "pak")) {
		search->pack = LoadPackFile(pak);
		if (!search->pack)
			Sys_Error("Couldn't load packfile: %s", pak);
	} else
		Q_strcpy(search->filename, pak);
	search->next = FileManager::searchpaths;
	FileManager::searchpaths = search;
}

#define MAX_FILES_IN_PACK       2048
// if a packfile directory differs from this, it is assumed to be hacked
#define PAK0_COUNT              339
#define PAK0_CRC                32981


// on disk
typedef struct {
	char name[56];
	int filepos, filelen;
} dpackfile_t;

typedef struct {
	char id[4];
	int dirofs;
	int dirlen;
} dpackheader_t;

pack_t *FileManager::LoadPackFile(char *packfile) {
	dpackheader_t header;
	pack_t *pack;
	int packhandle;
	dpackfile_t info[MAX_FILES_IN_PACK];

	if (Sys_FileOpenRead(packfile, &packhandle) == -1) {
		//              Con_Printf ("Couldn't open %s\n", packfile);
		return NULL;
	}
	Sys_FileRead(packhandle, (void *) &header, sizeof (header));
	if (header.id[0] != 'P' || header.id[1] != 'A'
			|| header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error("%s is not a packfile", packfile);
	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	int numpackfiles = header.dirlen / sizeof (dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error("%s has %i files", packfile, numpackfiles);

//	if (numpackfiles != PAK0_COUNT)
//		com_modified = true; // not the original file

	packfile_t *newfiles = (packfile_t *) Hunk_AllocName(numpackfiles * sizeof (packfile_t), "packfile");

	Sys_FileSeek(packhandle, header.dirofs);
	Sys_FileRead(packhandle, (void *) info, header.dirlen);

	// crc the directory to check for modifications
	CRC crc;
	crc.process((byte *)info, header.dirlen);	
	if (crc.getResult() != PAK0_CRC) {
		Con_SafeDPrintf("PAK0 modified...");
//		com_modified = true;
	}

	// parse the directory
	for (int i = 0; i < numpackfiles; i++) {
		Q_strcpy(newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	pack = (pack_t *) Hunk_Alloc(sizeof (pack_t));
	Q_strcpy(pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Con_Printf("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}
