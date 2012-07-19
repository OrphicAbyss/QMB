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
