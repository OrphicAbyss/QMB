
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifndef __WIN32__
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#endif

#include <SDL/SDL.h>

#include "quakedef.h"

qboolean isDedicated;

int noconinput = 0;

char *basedir = ".";
char *cachedir = "/tmp";

CVar sys_nostdout("sys_nostdout", "0");

// =======================================================================
// General routines
// =======================================================================

/**
 * Prints out to the stdout the console text.
 *
 * Removes any colour control codes for cleaner output.
 *
 * @param fmt
 * @param ...
 */
void Sys_Printf(const char *fmt, ...) {
	va_list argptr;
	char text[1024];
	char output[1024];

	int inputPos, outputPos, inputLen;

	va_start(argptr, fmt);
	vsnprintf(text, 1024, fmt, argptr);
	va_end(argptr);

	inputLen = strlen(text) + 1;
	outputPos = 0;
	for (inputPos = 0; inputPos < inputLen; inputPos++) {
		if (text[inputPos] == '&' && text[inputPos + 1] == 'r') {
			//skip the code
			inputPos += 2;
		}
		if (text[inputPos] == '&' && text[inputPos + 1] == 'c') {
			inputPos += 4;
		} else {
			output[outputPos++] = text[inputPos];
		}
	}

	fprintf(stdout, "%s", output);
}

void Sys_Quit(void) {
	Host_Shutdown();
	exit(0);
}

void Sys_Init(void) {
	Math_Init();
}

void Sys_Error(const char *error, ...) {
	va_list argptr;
	char string[1024];

	va_start(argptr, error);
	vsprintf(string, error, argptr);
	va_end(argptr);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown();
	exit(1);

}

/*
===============================================================================

FILE IO

===============================================================================
 */

#define	MAX_HANDLES		10
FILE *sys_handles[MAX_HANDLES];

int findhandle(void) {
	int i;

	for (i = 1; i < MAX_HANDLES; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error("out of handles");
	return -1;
}

/**
 * Returns the length of a file
 */
int Sys_FileLength(FILE *f) {
	int pos;
	int end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead(char *path, int *hndl) {
	FILE *f;
	int i;

	i = findhandle();

	f = fopen(path, "rb");
	if (!f) {
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;

	return Sys_FileLength(f);
}

int Sys_FileOpenWrite(char *path) {
	FILE *f;
	int i;

	i = findhandle();

	f = fopen(path, "wb");
	if (!f)
		Sys_Error("Error opening %s: %s", path, strerror(errno));
	sys_handles[i] = f;

	return i;
}

void Sys_FileClose(int handle) {
	if (handle >= 0) {
		fclose(sys_handles[handle]);
		sys_handles[handle] = NULL;
	}
}

void Sys_FileSeek(int handle, int position) {
	if (handle >= 0) {
		fseek(sys_handles[handle], position, SEEK_SET);
	}
}

int Sys_FileRead(int handle, void *dst, int count) {
	char *data;
	int size, done;

	size = 0;
	if (handle >= 0) {
		data = (char *) dst;
		while (count > 0) {
			done = fread(data, 1, count, sys_handles[handle]);
			if (done == 0) {
				break;
			}
			data += done;
			count -= done;
			size += done;
		}
	}
	return size;

}

int Sys_FileWrite(int handle, void *src, int count) {
	fwrite(src, 1, count, sys_handles[handle]);
}

int Sys_FileTime(char *path) {
	FILE *f;

	f = fopen(path, "rb");
	if (f) {
		fclose(f);
		return 1;
	}

	return -1;
}

void Sys_mkdir(char *path) {
#ifdef __WIN32__
	mkdir(path);
#else
	mkdir(path, 0777);
#endif
}

void Sys_DebugLog(char *file, char *fmt, ...) {
	va_list argptr;
	static char data[1024];
	FILE *fp;

	va_start(argptr, fmt);
	vsprintf(data, fmt, argptr);
	va_end(argptr);
	fp = fopen(file, "a");
	fwrite(data, Q_strlen(data), 1, fp);
	fclose(fp);
}

double Sys_FloatTime(void) {
#ifdef __WIN32__

	static int starttime = 0;

	if (!starttime)
		starttime = clock();

	return (clock() - starttime)*1.0 / 1024;

#else

	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0;
	}

	return (tp.tv_sec - secbase) +tp.tv_usec / 1000000.0;

#endif
}

#ifdef WIN32
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	int rc;

	rc = main(__argc, __argv);

	return rc;
}
#endif

int main(int c, char **v) {
	double time, oldtime, newtime;
	quakeparms_t parms;
	extern int vcrFile;
	extern int recording;
	int value;

	//signal(SIGFPE, SIG_IGN);

	value = COM_CheckParm("-mem");
	if (value)
		parms.memsize = (int) (Q_atof(com_argv[value + 1]) * 1024 * 1024);
	else
		parms.memsize = 16 * 1024 * 1024;

	parms.membase = malloc(parms.memsize);
	parms.basedir = basedir;
	parms.cachedir = cachedir;

	COM_InitArgv(c, v);
	parms.argc = com_argc;
	parms.argv = com_argv;

	Sys_Init();
	Host_Init(&parms);
	CVar::registerCVar(&sys_nostdout);

	oldtime = Sys_FloatTime() - 0.1;
	while (1) {
		// find time spent rendering last frame
		newtime = Sys_FloatTime();
		time = newtime - oldtime;

		if (cls.state == ca_dedicated) { // play vcrfiles at max speed
			if (time < sys_ticrate.getFloat() && (vcrFile == -1 || recording)) {
				SDL_Delay(1);
				continue; // not time to run a server only tic yet
			}
			time = sys_ticrate.getFloat();
		}

		if (time > sys_ticrate.getFloat()*2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame(time);
	}

}

/*
================
Sys_ConsoleInput
================
 */
char *Sys_ConsoleInput(void) {
	return 0;
}
