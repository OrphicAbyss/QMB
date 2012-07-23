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
// console.c

#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <fcntl.h>
#include "quakedef.h"

int con_linewidth;
float con_cursorspeed = 4;

#define		CON_TEXTSIZE	16384

bool con_forcedup; // because no entities to refresh
int con_totallines; // total lines in console scrollback
int con_backscroll; // lines up from bottom to display
int con_current; // where next message will be printed
int con_x; // offset in current line for next print
char *con_text = 0;

CVar con_notifytime("con_notifytime", "3"); //seconds

#define	NUM_CON_TIMES 4
float con_times[NUM_CON_TIMES]; // realtime time the line was generated
								// for transparent notify lines
int con_vislines;

bool con_debuglog;

#define		MAXCMDLINE	256
extern char key_lines[32][MAXCMDLINE];
extern int edit_line;
extern int key_linepos;

bool con_initialized;
int con_notifylines; // scan lines to clear for notify lines
extern void M_Menu_Main_f(void);

/**
 * Called to toggle the console
 */
void Con_ToggleConsole_f(void) {
	if (key_dest == key_console) {
		if (cls.state == ca_connected) {
			key_dest = key_game;
			key_lines[edit_line][1] = 0; // clear any typing
			key_linepos = 1;
		} else {
			M_Menu_Main_f();
		}
	} else
		key_dest = key_console;

	SCR_EndLoadingPlaque();
	memset(con_times, 0, sizeof (con_times));
}

/**
 * Clear the console
 */
void Con_Clear_f(void) {
	if (con_text)
		memset(con_text, ' ', CON_TEXTSIZE);
}

/**
 * Clears any notify messages
 */
void Con_ClearNotify(void) {
	int i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}

void Con_MessageMode_f(void) {
	extern bool team_message;
	key_dest = key_message;
	team_message = false;
}

void Con_MessageMode2_f(void) {
	extern bool team_message;
	key_dest = key_message;
	team_message = true;
}

/**
 * If the line width has changed, reformat the buffer.
 */
void Con_CheckResize(void) {
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char tbuf[CON_TEXTSIZE];

	width = (vid.conwidth >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1) // video hasn't been initialized yet
	{
		width = 80;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset(con_text, ' ', CON_TEXTSIZE);
	} else {
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		memcpy(tbuf, con_text, CON_TEXTSIZE);
		memset(con_text, ' ', CON_TEXTSIZE);

		for (i = 0; i < numlines; i++) {
			for (j = 0; j < numchars; j++) {
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
						tbuf[((con_current - i + oldtotallines) %
						oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

void Con_OpenDebugLog(void);
#define MAXGAMEDIRLEN	1000

void Con_Init(void) {
	char temp[MAXGAMEDIRLEN + 1];
	const char *t2 = "/qconsole.log";

	con_debuglog = COM_CheckParm("-condebug");

	if (con_debuglog) {
		if (strlen(com_gamedir) < (MAXGAMEDIRLEN - strlen(t2))) {
			sprintf(temp, "%s%s", com_gamedir, t2);
			unlink(temp);
		}
		Con_OpenDebugLog();
	}

	con_text = (char *) Hunk_AllocName(CON_TEXTSIZE, "context");
	memset(con_text, ' ', CON_TEXTSIZE);
	con_linewidth = -1;
	Con_CheckResize();

	Con_Printf("Console initialized.\n");

	// register our commands
	CVar::registerCVar(&con_notifytime);

	Cmd::addCmd("toggleconsole", Con_ToggleConsole_f);
	Cmd::addCmd("messagemode", Con_MessageMode_f);
	Cmd::addCmd("messagemode2", Con_MessageMode2_f);
	Cmd::addCmd("clear", Con_Clear_f);
	con_initialized = true;
}

void Con_Linefeed(void) {
	con_x = 0;
	con_current++;
	memset(&con_text[(con_current % con_totallines) * con_linewidth], ' ', con_linewidth);
}

/**
 * Handles cursor positioning, line wrapping, etc
 * All console printing must go through this in order to be logged to disk
 * If no console is visible, the notify window will pop up.
 */
void Con_Print(const char *txt) {
	int y;
	int c, l;
	static int cr;
	int mask;

	con_backscroll = 0;

	if (txt[0] == 1) {
		mask = 128; // go to colored text
		S_LocalSound("misc/talk.wav");
		// play talk wav
		txt++;
	} else if (txt[0] == 2) {
		mask = 128; // go to colored text
		txt++;
	} else
		mask = 0;


	while ((c = *txt)) {
		// count word length
		for (l = 0; l < con_linewidth; l++)
			if (txt[l] <= ' ')
				break;

		// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth))
			con_x = 0;

		txt++;

		if (cr) {
			con_current--;
			cr = false;
		}


		if (!con_x) {
			Con_Linefeed();
			// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c) {
			case '\n':
				con_x = 0;
				break;

			case '\r':
				con_x = 0;
				cr = 1;
				break;

			default: // display character and advance
				y = con_current % con_totallines;
				con_text[y * con_linewidth + con_x] = c | mask;
				con_x++;
				if (con_x >= con_linewidth)
					con_x = 0;
				break;
		}

	}
}

FILE *logfile;
void Con_OpenDebugLog(void) {
	logfile = fopen(va("%s/QMB.log", com_gamedir), "w");
}

void Con_DebugLog(const char *msg) {
	fprintf(logfile, msg);
}

void Con_CloseDebugLog(void) {
	fclose(logfile);
}

int Con_Print_Real(const char *msg) {
	static bool inupdate;
		
	// also echo to debugging console
	Sys_Printf("%s", msg); // also echo to debugging console

	// log all messages to file
	if (con_debuglog)
		Con_DebugLog(msg);

	if (!con_initialized)
		return -1;

	if (cls.state == ca_dedicated)
		return -1; // no graphics mode

	// write it to the scrollable buffer
	Con_Print(msg);

	// update the screen if the console is displayed
	if (cls.signon != SIGNONS && !scr_disabled_for_loading) {
		// protect against infinite loop if something in SCR_UpdateScreen calls
		// Con_Printd
		if (!inupdate) {
			inupdate = true;
			SCR_UpdateScreen();
			inupdate = false;
		}
	}
	return 0;	
}

/**
 * Handles cursor positioning, line wrapping, etc
 */
#define	MAXPRINTMSG	4096
void Con_Printf(const char *fmt, ...) {
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	Con_Print_Real(msg);
}

#ifdef JAVA
#include <jni.h>
jint JNICALL Con_fPrintf(FILE *empty, char *fmt, ...) {
	va_list argptr;
	char msg[MAXPRINTMSG];
	static bool inupdate;

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	Con_Print_Real(msg);
	Con_Print_Real("JavaMsg\n");
}
#endif

/**
 * A Con_Printf that only shows up if the "developer" cvar is set
 */
void Con_DPrintf(const char *fmt, ...) {
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!developer.getBool())
		return; // don't confuse non-developers with techie stuff...

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	Con_Print_Real(msg);
}

void Con_SafePrint(const char *msg) {
	int temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	Con_Print_Real(msg);
	scr_disabled_for_loading = temp;
}

/**
 * Okay to call even when the screen can't be updated
 */
void Con_SafePrintf(const char *fmt, ...) {
	va_list argptr;
	char msg[MAXPRINTMSG];
	
	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	Con_SafePrint(msg);
}

/**
 * A Con_Printf that only shows up if the "developer" cvar is set
 * Okay to call even when the screen can't be updated
 */
void Con_SafeDPrintf(const char *fmt, ...) {
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!developer.getBool())
		return; // don't confuse non-developers with techie stuff...

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);
	
	Con_SafePrint(msg);
}

/*
==============================================================================
DRAWING
==============================================================================
 */

/**
 * The input line scrolls horizontally if typing goes beyond the right edge
 */
void Con_DrawInput(void) {
	int y;
	int i;
	char *text;

	if (key_dest != key_console && !con_forcedup)
		return; // don't draw anything

	text = key_lines[edit_line];

	// add the cursor frame
	text[key_linepos] = 10 + ((int) (realtime * con_cursorspeed)&1);

	// fill out remainder with spaces
	for (i = key_linepos + 1; i < con_linewidth; i++)
		text[i] = ' ';

	//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw it
	y = con_vislines - 16;

	for (i = 0; i < con_linewidth; i++)
		Draw_Character((i + 1) << 3, con_vislines - 16, text[i]);

	// remove cursor
	key_lines[edit_line][key_linepos] = 0;
}

/**
 * Detects a text colour code in console text and sets the glColor3f with
 * the given colour;
 */
int Con_DetectColour(char *text) {
	char marker = '&';
	int skip = 0;

	if (text[0] == '&') {
		if (text[1] == 'c' || text[1] == 'C') {
			glColor3f((float) (text[2] - '0') / 9.0f,
					(float) (text[3] - '0') / 9.0f,
					(float) (text[4] - '0') / 9.0f);
			skip += 5;
		} else if (text[1] == 'r') {
			glColor3f(1, 1, 1);
			skip += 2;
		}
	}

	return skip;
}

/**
 * Draws the last few lines of output transparently over the game top
 */
void Con_DrawNotify(void) {
	int x, v;
	char *text;
	int i;
	float time;
	int j;
	extern char chat_buffer[];
	v = 0;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	for (i = con_current - NUM_CON_TIMES + 1; i <= con_current; i++) {
		if (i < 0)
			continue;

		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;

		time = realtime - time;
		if (time > con_notifytime.getFloat())
			continue;

		text = con_text + (i % con_totallines) * con_linewidth;
		clearnotify = 0;
		scr_copytop = 1;
		j = 0;
		for (x = 0; x < con_linewidth; x++) {
			x += Con_DetectColour(&text[x]);

			Draw_Character((j + 1) << 3, v, text[x]);
			j++;
		}
		glColor3f(1, 1, 1);
		v += 8;
	}

	if (key_dest == key_message) {
		clearnotify = 0;
		scr_copytop = 1;

		x = 0;

		Draw_String(8, v, "say:");
		while (chat_buffer[x]) {
			Draw_Character((x + 5) << 3, v, chat_buffer[x]);
			x++;
		}
		Draw_Character((x + 5) << 3, v, 10 + ((int) (realtime * con_cursorspeed)&1));
		v += 8;
	}
	if (v > con_notifylines)
		con_notifylines = v;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/**
 * Draws the console with the solid background
 * The typing input line at the bottom should only be drawn if typing is allowed
 */
void Con_DrawConsole(int lines, bool drawinput) {
	int i, charPos, x, y;
	int rows;
	char *text;
	//	char   dlbar[1024];

	//no console shown
	if (lines <= 0)
		return;

	// draw the background
	Draw_ConsoleBackground(lines);
	con_vislines = lines;
	rows = (lines - 22) >> 3; // rows of text to draw
	//y = lines - 30;
	y = lines - 16 - (rows << 3); // may start slightly negative

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor3f(1, 1, 1);

	// If we are backscrolled, reserve a row for use after drawing the rest of the console
	if (con_backscroll) {
		rows--;
	}

	for (i = con_current - rows + 1; i <= con_current; i++, y += 8) {
		charPos = i - con_backscroll;
		if (charPos < 0)
			charPos = 0;

		text = con_text + (charPos % con_totallines) * con_linewidth;
		charPos = 0;
		for (x = 0; x < con_linewidth; x++) {
			x += Con_DetectColour(&text[x]);

			Draw_Character((charPos + 1) << 3, y, text[x]);
			charPos++;
		}
	}

	// Draw arrows to show that the buffer is backscrolled
	if (con_backscroll) {
		for (x = 0; x < con_linewidth; x += 4)
			Draw_Character((x + 1) << 3, y, '^');
		y += 8;
	}

	// draw the input prompt, user text, and cursor if desired
	if (drawinput)
		Con_DrawInput();

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void Con_NotifyBox(char *text) {
	double t1, t2;

	// during startup for sound / cd warnings
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	Con_Printf(text);

	Con_Printf("Press a key.\n");
	Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	key_count = -2; // wait for a key down and up
	key_dest = key_console;

	do {
		t1 = Sys_FloatTime();
		SCR_UpdateScreen();
		Sys_SendKeyEvents();
		t2 = Sys_FloatTime();
		realtime += t2 - t1; // make the cursor blink
	} while (key_count < 0);

	Con_Printf("\n");
	key_dest = key_game;
	realtime = 0; // put the cursor back to invisible
}
