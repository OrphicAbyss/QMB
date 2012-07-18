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
// common.c -- misc functions used in client and server

#include "quakedef.h"
#include "FileManager.h"

#define NUM_SAFE_ARGVS  7

static char *largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static char *argvdummy = " ";
static char *safeargvs[NUM_SAFE_ARGVS] = {"-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse"};

CVar registered("registered", "0");
CVar cmdline("cmdline", "0", false, true);

bool com_modified; // set true if using non-id files
bool nomod;

void COM_InitFilesystem(void);

// if a packfile directory differs from this, it is assumed to be hacked
#define PAK0_COUNT              339
#define PAK0_CRC                32981

char com_token[1024];
int com_argc;
char **com_argv;

//JHL: QMB mod global
int qmb_mod;

#define CMDLINE_LENGTH	256
char com_cmdline[CMDLINE_LENGTH];

bool standard_quake = true, rogue, hipnotic;

/*
 * All of Quake's data access is through a hierchal file system, but the
 * contents of the file system can be transparently merged from several sources.
 * 
 * The "base directory" is the path to the directory holding the quake.exe and
 *  all game directories.  The sys_* files pass this to host_init in
 *  quakeparms_t->basedir.  This can be overridden with the "-basedir" command
 *  line parm to allow code debugging in a different directory.
 * 
 *   The base directory is only used during filesystem initialization.
 * 
 * The "game directory" is the first tree on the search path and directory that
 * all generated files (savegames, screenshots, demos, config files) will be
 * saved to.
 * 
 * This can be overridden with the "-game" command line parameter.
 * 
 * The game directory can never be changed while quake is executing.
 * 
 * This is a precacution against having a malicious server instruct clients to
 * write files over areas they shouldn't.
 * 
 * The "cache directory" is only used during development to save network
 * bandwidth, especially over ISDN / T1 lines.
 * 
 * If there is a cache directory specified, when a file is found by the normal
 *  search path, it will be mirrored into the cache directory, then opened there.
 * 
 * FIXME: The file "parms.txt" will be read out of the game directory and
 * appended to the current command line arguments to allow different games to
 * initialize startup parms differently.
 * 
 * This could be used to add a "-sspeed 22050" for the high quality sound edition.
 * 
 * Because they are added at the end, they will not override an explicit setting
 * on the original command line.
 */

//============================================================================
// ClearLink is used for new headnodes

void ClearLink(link_t *l) {
	l->prev = l->next = l;
}

void RemoveLink(link_t *l) {
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore(link_t *l, link_t *before) {
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter(link_t *l, link_t *after) {
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================
					LIBRARY REPLACEMENT FUNCTIONS
============================================================================
 */
#define uselib
#ifdef uselib
#include <string.h>
#endif

void Q_memset(void *dest, int fill, int count) {
#ifdef uselib
	memset(dest, fill, count);
#else
	int i;

	if ((((long) dest | count) & 3) == 0) {
		count >>= 2;
		fill = fill | (fill << 8) | (fill << 16) | (fill << 24);
		for (i = 0; i < count; i++)
			((int *) dest)[i] = fill;
	} else
		for (i = 0; i < count; i++)
			((byte *) dest)[i] = fill;
#endif
}

void Q_memcpy(void *dest, const void *src, int count) {
#ifdef uselib
	memcpy(dest, src, count);
#else
	int i;

	if ((((long) dest | (long) src | count) & 3) == 0) {
		count >>= 2;
		for (i = 0; i < count; i++)
			((int *) dest)[i] = ((int *) src)[i];
	} else
		for (i = 0; i < count; i++)
			((byte *) dest)[i] = ((byte *) src)[i];
#endif
}

int Q_memcmp(void *m1, void *m2, int count) {
#ifdef uselib
	return memcmp(m1, m2, count);
#else
	while (count) {
		count--;
		if (((byte *) m1)[count] != ((byte *) m2)[count])
			return -1;
	}
	return 0;
#endif
}

void Q_strcpy(char *dest, const char *src) {
#ifdef uselib
	strcpy(dest, src);
#else
	while (*src) {
		*dest++ = *src++;
	}
	*dest++ = 0;
#endif
}

void Q_strncpy(char *dest, const char *src, int count) {
#ifdef uselib
	strncpy(dest, src, count);
#else
	while (*src && count--) {
		*dest++ = *src++;
	}
	if (!count)
		*dest++ = 0;
#endif
}

int Q_strlen(const char *str) {
#ifdef uselib
	return strlen(str);
#else
	int count;

	count = 0;
	while (str[count])
		count++;

	return count;
#endif
}

char *Q_strrchr(char *s, char c) {
#ifdef uselib
	return strrchr(s, c);
#else
	int len = Q_strlen(s);
	s += len;
	while (len--)
		if (*--s == c) return s;
	return 0;
#endif
}

void Q_strcat(char *dest, const char *src) {
#ifdef uselib
	strcat(dest, src);
#else
	dest += Q_strlen(dest);
	Q_strcpy(dest, src);
#endif
}

int Q_strcmp(const char *s1, const char *s2) {
#ifdef uselib
	return strcmp(s1, s2);
#else
	while (1) {
		if (*s1 != *s2)
			return -1; // strings not equal
		if (!*s1)
			return 0; // strings are equal
		s1++;
		s2++;
	}

	return -1;
#endif
}

int Q_strncmp(const char *s1, const char *s2, int count) {
#ifdef uselib
	return strncmp(s1, s2, count);
#else
	while (1) {
		if (!count--)
			return 0;
		if (*s1 != *s2)
			return -1; // strings not equal
		if (!*s1)
			return 0; // strings are equal
		s1++;
		s2++;
	}

	return -1;
#endif
}

int Q_strncasecmp(const char *s1, const char *s2, int n) {
#ifdef uselib
	return strncasecmp(s1, s2, n);
#else
	int c1, c2;

	while (1) {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0; // strings are equal until end point

		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1; // strings not equal
		}
		if (!c1)
			return 0; // strings are equal
	}

	return -1;
#endif
}

int Q_strcasecmp(const char *s1, const char *s2) {
#ifdef uselib
	return strcasecmp(s1, s2);
#else
	int c1, c2;

	while (1) {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1; // strings not equal
		}
		if (!c1)
			return 0; // strings are equal
	}

	return -1;
#endif
}

int Q_atoi(const char *str) {
#ifdef uselib
	return atoi(str);
#else
	int val;
	int sign;
	int c;

	if (*str == '-') {
		sign = -1;
		str++;
	} else
		sign = 1;

	val = 0;

	//
	// check for hex
	//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		str += 2;
		while (1) {
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val << 4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val << 4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val << 4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	//
	// check for character
	//
	if (str[0] == '\'') {
		return sign * str[1];
	}

	//
	// assume decimal
	//
	while (1) {
		c = *str++;
		if (c < '0' || c > '9')
			return val * sign;
		val = val * 10 + c - '0';
	}

	return 0;
#endif
}

float Q_atof(const char *str) {
#ifdef uselib
	return atof(str);
#else
	double val;
	int sign;
	int c;
	int decimal, total;

	if (*str == '-') {
		sign = -1;
		str++;
	} else
		sign = 1;

	val = 0;

	//
	// check for hex
	//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		str += 2;
		while (1) {
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val * 16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val * 16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	//
	// check for character
	//
	if (str[0] == '\'') {
		return sign * str[1];
	}

	//
	// assume decimal
	//
	decimal = -1;
	total = 0;
	while (1) {
		c = *str++;
		if (c == '.') {
			decimal = total;
			continue;
		}
		if (c < '0' || c > '9')
			break;
		val = val * 10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val * sign;
	while (total > decimal) {
		val /= 10;
		total--;
	}

	return val*sign;
#endif
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
 */

bool bigendien;

short (*BigShort) (short l);
short (*LittleShort) (short l);
int (*BigLong) (int l);
int (*LittleLong) (int l);
float (*BigFloat) (float l);
float (*LittleFloat) (float l);

short ShortSwap(short l) {
	byte b1, b2;

	b1 = l & 255;
	b2 = (l >> 8)&255;

	return (b1 << 8) +b2;
}

short ShortNoSwap(short l) {
	return l;
}

int LongSwap(int l) {
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8)&255;
	b3 = (l >> 16)&255;
	b4 = (l >> 24)&255;

	return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

int LongNoSwap(int l) {
	return l;
}

float FloatSwap(float f) {

	union {
		float f;
		byte b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap(float f) {
	return f;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
 */

//
// writing functions
//
#define MSG_ReadVector(v) {(v)[0] = MSG_ReadCoord();(v)[1] = MSG_ReadCoord();(v)[2] = MSG_ReadCoord();}

void MSG_WriteChar(sizebuf_t *sb, int c) {
	byte *buf;

#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error("MSG_WriteChar: range error");
#endif

	buf = (byte *) SZ_GetSpace(sb, 1);
	buf[0] = c;
}

void MSG_WriteByte(sizebuf_t *sb, int c) {
	byte *buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error("MSG_WriteByte: range error");
#endif

	buf = (byte *) SZ_GetSpace(sb, 1);
	buf[0] = c;
}

void MSG_WriteShort(sizebuf_t *sb, int c) {
	byte *buf;

#ifdef PARANOID
	if (c < ((short) 0x8000) || c > (short) 0x7fff)
		Sys_Error("MSG_WriteShort: range error");
#endif

	buf = (byte *) SZ_GetSpace(sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void MSG_WriteLong(sizebuf_t *sb, int c) {
	byte *buf;

	buf = (byte *) SZ_GetSpace(sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8)&0xff;
	buf[2] = (c >> 16)&0xff;
	buf[3] = c >> 24;
}

void MSG_WriteFloat(sizebuf_t *sb, float f) {

	union {
		float f;
		int l;
	} dat;


	dat.f = f;
	dat.l = LittleLong(dat.l);

	SZ_Write(sb, &dat.l, 4);
}

void MSG_WriteString(sizebuf_t *sb, const char *s) {
	if (!s)
		SZ_Write(sb, "", 1);
	else
		SZ_Write(sb, s, Q_strlen(s) + 1);
}

void MSG_WriteCoord(sizebuf_t *sb, float f) {
	MSG_WriteShort(sb, (int) (f * 8));
}

// used by server (always latest dpprotocol)

void MSG_WriteDPCoord(sizebuf_t *sb, float f) {
	if (f >= 0)
		MSG_WriteShort(sb, (int) (f + 0.5f));
	else
		MSG_WriteShort(sb, (int) (f - 0.5f));
}

void MSG_WriteAngle(sizebuf_t *sb, float f) {
	MSG_WriteByte(sb, ((int) f * 256 / 360) & 255);
}

//
// reading functions
//
int msg_readcount;
bool msg_badread;

void MSG_BeginReading(void) {
	msg_readcount = 0;
	msg_badread = false;
}

// returns -1 and sets msg_badread if no more characters are available

int MSG_ReadChar(void) {
	int c;

	if (msg_readcount + 1 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (signed char) net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadByte(void) {
	int c;

	if (msg_readcount + 1 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (unsigned char) net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadShort(void) {
	int c;

	if (msg_readcount + 2 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (short) (net_message.data[msg_readcount]
			+ (net_message.data[msg_readcount + 1] << 8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong(void) {
	int c;

	if (msg_readcount + 4 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
			+ (net_message.data[msg_readcount + 1] << 8)
			+ (net_message.data[msg_readcount + 2] << 16)
			+ (net_message.data[msg_readcount + 3] << 24);

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat(void) {

	union {
		byte b[4];
		float f;
		int l;
	} dat;

	dat.b[0] = net_message.data[msg_readcount];
	dat.b[1] = net_message.data[msg_readcount + 1];
	dat.b[2] = net_message.data[msg_readcount + 2];
	dat.b[3] = net_message.data[msg_readcount + 3];
	msg_readcount += 4;

	dat.l = LittleLong(dat.l);

	return dat.f;
}

char *MSG_ReadString(void) {
	static char string[2048];
	unsigned int l;
	int c;

	l = 0;
	do {
		c = MSG_ReadChar();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof (string) - 1);

	string[l] = 0;

	return string;
}

float MSG_ReadCoord(void) {
	return MSG_ReadShort() * (1.0 / 8);
}

float MSG_ReadAngle(void) {
	return MSG_ReadChar() * (360.0 / 256);
}



//===========================================================================

void SZ_Alloc(sizebuf_t *buf, int startsize) {
	if (startsize < 256)
		startsize = 256;
	buf->data = (byte *) Hunk_AllocName(startsize, "sizebuf");
	buf->maxsize = startsize;
	buf->cursize = 0;
}

void SZ_Clear(sizebuf_t *buf) {
	buf->cursize = 0;
}

void *SZ_GetSpace(sizebuf_t *buf, int length) {
	void *data;

	if (buf->cursize + length > buf->maxsize) {
		if (!buf->allowoverflow)
			Sys_Error("SZ_GetSpace: overflow without allowoverflow set");

		if (length > buf->maxsize)
			Sys_Error("SZ_GetSpace: %i is > full buffer size", length);

		buf->overflowed = true;
		Con_Printf("SZ_GetSpace: overflow");
		SZ_Clear(buf);
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write(sizebuf_t *buf, const void *data, int length) {
	Q_memcpy(SZ_GetSpace(buf, length), data, length);
}

void SZ_Print(sizebuf_t *buf, const char *data) {
	int len;

	len = Q_strlen(data) + 1;

	// byte * cast to keep VC++ happy
	if (buf->data[buf->cursize - 1])
		Q_memcpy((byte *) SZ_GetSpace(buf, len), data, len); // no trailing 0
	else
		Q_memcpy((byte *) SZ_GetSpace(buf, len - 1) - 1, data, len); // write over trailing 0
}


//============================================================================

/*
============
COM_SkipPath
============
 */
char *COM_SkipPath(char *pathname) {
	char *last;

	last = pathname;
	while (*pathname) {
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

/**
 * Converts any illegal file characeters into legal ones.
 * 
 * @param data to check and fix
 */
void COM_MakeFilenameValid(char *data) {
	for (char *c = data; *c; c++)
		if (*c == '*')
			*c = '#';
}

/*
============
COM_StripExtension
============
 */
void COM_StripExtension(const char *in, char *out) {
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/*
============
COM_FileBase
============
 */
void COM_FileBase(const char *in, char *out) {
	const char *s;
	const char *s2;

	s = in + Q_strlen(in) - 1;

	while (s != in && *s != '.')
		s--;

	for (s2 = s; *s2 && *s2 != '/'; s2--)
		;

	if (s - s2 < 2)
		Q_strcpy(out, "?model?");
	else {
		s--;
		Q_strncpy(out, s2 + 1, s - s2);
		out[s - s2] = 0;
	}
}

/*
==================
COM_DefaultExtension
==================
 */
void COM_DefaultExtension(char *path, const char *extension) {
	char *src;
	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	src = path + Q_strlen(path) - 1;

	while (*src != '/' && src != path) {
		if (*src == '.')
			return; // it has an extension
		src--;
	}

	Q_strcat(path, extension);
}

/**
 * Parse a token out of a string
 */
char *COM_Parse(char *data) {
	int c;
	int len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

	// skip whitespace
skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0)
			return NULL; // end of file;
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}


	// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (1) {
			c = *data++;
			if (c == '\"' || !c) {
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':') {
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do {
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}

/**
 * Returns the position (1 to argc-1) in the program's argument list where the
 *  given parameter apears, or 0 if not present
 */
int COM_CheckParm(const char *parm) {
	for (int i = 1; i < com_argc; i++) {
		if (!com_argv[i])
			continue; // NEXTSTEP sometimes clears appkit vars.
		if (!Q_strcmp(parm, com_argv[i]))
			return i;
	}
	return 0;
}

/**
 * Looks for the pop.txt file and verifies it.
 * Sets the "registered" cvar.
 * Immediately exits out if an alternate game was attempted to be started without
 * being registered.
 */
void COM_CheckRegistered(void) {
	CVar::setValue("cmdline", com_cmdline);
	CVar::setValue("registered", "1");
	Con_Printf("Playing registered version.\n");
}

void COM_Path_f(void);

/*
================
COM_InitArgv
================
 */
void COM_InitArgv(int argc, char **argv) {
	// reconstitute the command line for the cmdline externally visible cvar
	int n = 0;

	for (int j = 0; (j < MAX_NUM_ARGVS) && (j < argc); j++) {
		int i = 0;

		while ((n < (CMDLINE_LENGTH - 1)) && argv[j][i]) {
			com_cmdline[n++] = argv[j][i++];
		}

		if (n < (CMDLINE_LENGTH - 1))
			com_cmdline[n++] = ' ';
		else
			break;
	}

	com_cmdline[n] = 0;

	bool safe = false;
	for (com_argc = 0; (com_argc < MAX_NUM_ARGVS) && (com_argc < argc);
			com_argc++) {
		largv[com_argc] = argv[com_argc];
		if (!Q_strcmp("-safe", argv[com_argc]))
			safe = true;
	}

	if (safe) {
		// force all the safe-mode switches. Note that we reserved extra space in
		// case we need to add these, so we don't need an overflow check
		for (int i = 0; i < NUM_SAFE_ARGVS; i++) {
			largv[com_argc] = safeargvs[i];
			com_argc++;
		}
	}

	largv[com_argc] = argvdummy;
	com_argv = largv;

	if (COM_CheckParm("-rogue")) {
		rogue = true;
		standard_quake = false;
	}

	if (COM_CheckParm("-hipnotic")) {
		hipnotic = true;
		standard_quake = false;
	}
}

void COM_Init(char *basedir) {
	byte swaptest[2] = {1, 0};

	// set the byte swapping variables in a portable manner
	if (*(short *) swaptest == 1) {
		bigendien = false;
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	} else {
		bigendien = true;
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

	CVar::registerCVar(&registered);
	CVar::registerCVar(&cmdline);
	Cmd::addCmd("path", COM_Path_f);

	COM_InitFilesystem();
	COM_CheckRegistered();
}

/**
 * does a varargs printf into a temp buffer, so I don't need to have
 * varargs versions of all text functions.
 */
char *va(const char *format, ...) {
	va_list argptr;
	static char string[1024];

	va_start(argptr, format);
	vsnprintf(string, 1024, format, argptr);
	va_end(argptr);

	return string;
}


/// just for debugging

int memsearch(byte *start, int count, int search) {
	int i;

	for (i = 0; i < count; i++)
		if (start[i] == search)
			return i;
	return -1;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
 */

int com_filesize;

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

#define MAX_FILES_IN_PACK       2048

char com_gamedir[MAX_OSPATH];

void COM_Path_f(void) {
	Con_Printf("Current search path:\n");
	for (searchpath_t *s = FileManager::searchpaths; s; s = s->next) {
		if (s->pack) {
			Con_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		} else
			Con_Printf("%s\n", s->filename);
	}
}

/**
 * Filename are reletive to the quake directory. Always appends a 0 byte.
 */
static MemoryObj *loadcache;
static byte *loadbuf;
static int loadsize;

byte *COM_LoadFile(const char *path, int usehunk) {
	int h;
	void *buf = NULL;
	char base[32];
	int len;

	// look for it in the filesystem or pack files
	len = FileManager::OpenFile(path, &h);
	if (h == -1)
		return NULL;

	// extract the filename base name for hunk tag
	COM_FileBase(path, base);

	if (usehunk == 1)
		buf = Hunk_AllocName(len + 1, base);
	else if (usehunk == 2)
		buf = Hunk_TempAlloc(len + 1);
	else if (usehunk == 0)
		buf = MemoryObj::ZAlloc(len + 1);
	else if (usehunk == 3) {
		// TODO: Shouldn't pass back the buffer, should only pass around CacheObj
		// TODO: Require memory object base
		loadcache = MemoryObj::Alloc(MemoryObj::CACHE, base, len + 1);
		buf = loadcache->getData();
	} else if (usehunk == 4) {
		if (len + 1 > loadsize)
			buf = Hunk_TempAlloc(len + 1);
		else
			buf = loadbuf;
	} else
		Sys_Error("COM_LoadFile: bad usehunk");

	if (!buf)
		Sys_Error("COM_LoadFile: not enough space for %s", path);

	Q_memset(buf, 0, len + 1);

	Sys_FileRead(h, buf, len);
	FileManager::CloseFile(h);

	return (byte *) buf;
}

byte *COM_LoadHunkFile(const char *path) {
	return COM_LoadFile(path, 1);
}

byte *COM_LoadTempFile(const char *path) {
	return COM_LoadFile(path, 2);
}

void COM_LoadCacheFile(const char *path, MemoryObj cu) {
	loadcache = &cu;
	COM_LoadFile(path, 3);
}

// uses temp hunk if larger than bufsize

byte *COM_LoadStackFile(const char *path, void *buffer, int bufsize) {
	byte *buf;

	loadbuf = (byte *) buffer;
	loadsize = bufsize;
	buf = COM_LoadFile(path, 4);

	return buf;
}

/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
 */
pack_t *COM_LoadPackFile(char *packfile) {
	dpackheader_t header;
	int i;
	packfile_t *newfiles;
	int numpackfiles;
	pack_t *pack;
	int packhandle;
	dpackfile_t info[MAX_FILES_IN_PACK];
	unsigned short crc;

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

	numpackfiles = header.dirlen / sizeof (dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error("%s has %i files", packfile, numpackfiles);

	if (numpackfiles != PAK0_COUNT)
		com_modified = true; // not the original file

	newfiles = (packfile_t *) Hunk_AllocName(numpackfiles * sizeof (packfile_t), "packfile");

	Sys_FileSeek(packhandle, header.dirofs);
	Sys_FileRead(packhandle, (void *) info, header.dirlen);

	// crc the directory to check for modifications
	CRC_Init(&crc);
	for (i = 0; i < header.dirlen; i++)
		CRC_ProcessByte(&crc, ((byte *) info)[i]);
	if (crc != PAK0_CRC)
		com_modified = true;

	// parse the directory
	for (i = 0; i < numpackfiles; i++) {
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

/**
 * Sets com_gamedir, adds the directory to the head of the path,
 * then loads and adds pak1.pak pak2.pak ...
 */
void COM_AddGameDirectory(char *dir) {
	int i;
	searchpath_t *search;
	pack_t *pak;
	char pakfile[MAX_OSPATH];

	Q_strcpy(com_gamedir, dir);

	// add the directory to the search path
	search = (searchpath_t *) Hunk_Alloc(sizeof (searchpath_t));
	Q_strcpy(search->filename, dir);
	search->next = FileManager::searchpaths;
	FileManager::searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (i = 0;; i++) {
		sprintf(pakfile, "%s/pak%i.pak", dir, i);
		pak = COM_LoadPackFile(pakfile);
		if (!pak)
			break;
		search = (searchpath_t *) Hunk_Alloc(sizeof (searchpath_t));
		search->pack = pak;
		search->next = FileManager::searchpaths;
		FileManager::searchpaths = search;
	}

	// add the contents of the parms.txt file to the end of the command line
}

void COM_InitFilesystem(void) {
	int i, j;
	char basedir[MAX_OSPATH];
	searchpath_t *search;

	// -basedir <path>
	// Overrides the system supplied base directory (under GAMENAME)
	i = COM_CheckParm("-basedir");
	if (i && i < com_argc - 1)
		Q_strcpy(basedir, com_argv[i + 1]);
	else
		Q_strcpy(basedir, host_parms.basedir);

	j = Q_strlen(basedir);

	if (j > 0) {
		if ((basedir[j - 1] == '\\') || (basedir[j - 1] == '/'))
			basedir[j - 1] = 0;
	}

	// start up with GAMENAME by default (id1)
	COM_AddGameDirectory(va("%s/"GAMENAME, basedir));
	// add in the QMB mod
	COM_AddGameDirectory(va("%s/qmb", basedir));

	if (COM_CheckParm("-rogue"))
		COM_AddGameDirectory(va("%s/rogue", basedir));
	if (COM_CheckParm("-hipnotic"))
		COM_AddGameDirectory(va("%s/hipnotic", basedir));

	// -game <gamedir>
	// Adds basedir/gamedir as an override game
	i = COM_CheckParm("-game");
	if (i && i < com_argc - 1) {
		com_modified = true;
		COM_AddGameDirectory(va("%s/%s", basedir, com_argv[i + 1]));
	}

	// -path <dir or packfile> [<dir or packfile>] ...
	// Fully specifies the exact serach path, overriding the generated one
	i = COM_CheckParm("-path");
	if (i) {
		com_modified = true;
		FileManager::searchpaths = NULL;
		while (++i < com_argc) {
			if (!com_argv[i] || com_argv[i][0] == '+' || com_argv[i][0] == '-')
				break;

			search = (searchpath_t *) Hunk_Alloc(sizeof (searchpath_t));
			if (!Q_strcmp(FileManager::FileExtension(com_argv[i]), "pak")) {
				search->pack = COM_LoadPackFile(com_argv[i]);
				if (!search->pack)
					Sys_Error("Couldn't load packfile: %s", com_argv[i]);
			} else
				Q_strcpy(search->filename, com_argv[i]);
			search->next = FileManager::searchpaths;
			FileManager::searchpaths = search;
		}
	}

	if (COM_CheckParm("-nomod"))
		nomod = true;
}
