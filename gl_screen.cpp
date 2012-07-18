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

// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "Texture.h"
#include "FileManager.h"
#include "CaptureHelpers.h"

/*
background clear
rendering
turtle/net icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions

syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?

async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net
turn off messages option

the refresh is allways rendered, unless the console is full screen


console is:
	notify lines
	half
	full
 */

int glx, gly, glwidth, glheight;

// only the refresh window will be updated unless these variables are flagged
int scr_copytop;
int scr_copyeverything;

float scr_con_current;
float scr_conlines; // lines of console to display

float oldscreensize, oldfov;

CVar scr_viewsize("viewsize", "100", true);
CVar scr_fov("fov", "90", true); // 10 - 170
CVar scr_conspeed("scr_conspeed", "600", true);
CVar scr_centertime("scr_centertime", "2");
CVar scr_showturtle("showturtle", "0");
CVar scr_showpause("showpause", "1");
CVar scr_printspeed("scr_printspeed", "8");
CVar gl_triplebuffer("gl_triplebuffer", "1", true);

extern CVar crosshair;

bool scr_initialized; // ready to draw

qpic_t *scr_ram;
qpic_t *scr_net;
qpic_t *scr_turtle;

int scr_fullupdate;

int clearconsole;
int clearnotify;

vrect_t scr_vrect;

bool scr_disabled_for_loading;
bool scr_drawloading;
float scr_disabled_time;

void SCR_ScreenShot_f(void);

extern CVar hud;

/*
===============================================================================
CENTER PRINTING
===============================================================================
 */

char scr_centerstring[1024];
float scr_centertime_start; // for slow victory printing
float scr_centertime_off;
int scr_center_lines;
int scr_erase_lines;
int scr_erase_center;

/**
 * Called for important messages that should stay in the center of the screen
 * for a few moments
 */
void SCR_CenterPrint(const char *str) {
	Q_strncpy(scr_centerstring, str, sizeof (scr_centerstring) - 1);
	scr_centertime_off = scr_centertime.getFloat();
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	while (*str) {
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString(void) {
	char *start;
	int l;
	int j;
	int x, y;
	int remaining;

	// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.getFloat() * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.conheight * 0.35;
	else
		y = 48;

	do {
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.conwidth - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8) {
			Draw_Character(x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++; // skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString(void) {
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString();
}

//=============================================================================

float CalcFov(float fov_x, float width, float height) {
	float a;
	float x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error("Bad fov: %f", fov_x);

	x = width / tan(fov_x / 360 * M_PI);

	a = atan(height / x);

	a = a * 360 / M_PI;

	return a;
}

/**
 * Must be called whenever vid changes
 * 
 * Internal use only
 */
static void SCR_CalcRefdef(void) {
	float size, fov;
	int h;
	bool full = false;

	scr_fullupdate = 0; // force a background redraw
	vid.recalc_refdef = 0;

	//========================================
	size = scr_viewsize.getFloat();
	fov = scr_fov.getFloat();

	// bound viewsize
	size = max(30, size);
	size = min(120, size);

	// bound field of view
	fov = max(10, fov);
	fov = min(170, fov);

	if (size != scr_viewsize.getFloat())
		scr_viewsize.set(size);
	if (fov != scr_fov.getFloat())
		scr_fov.set(fov);

	sb_lines = 24 + 16 + 8;
	if (size >= 100.0) {
		full = true;
		size = 100.0;
	}

	if (cl.intermission) {
		full = true;
		size = 100;
		sb_lines = 0;
	}

	size /= 100.0f;

	// TODO: remove sb_lines variable
	h = vid.height; // - sb_lines;

	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96) {
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96; // min for icons
	}

	r_refdef.vrect.height = (signed)vid.height * size;
	//if (r_refdef.vrect.height > (signed)vid.height - sb_lines)
	//	r_refdef.vrect.height = (signed)vid.height - sb_lines;
	if (r_refdef.vrect.height > (signed)vid.height)
		r_refdef.vrect.height = vid.height;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;

	if (full)
		r_refdef.vrect.y = 0;
	else
		r_refdef.vrect.y = (h - r_refdef.vrect.height) / 2;

	r_refdef.fov_x = fov;
	r_refdef.fov_y = CalcFov(r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;
}

void SCR_SizeUp_f(void) {
	//JHL:HACK; changed to affect the HUD, not SCR size
	if (hud.getInt() < 3) {
		hud.set(hud.getFloat() + 1);
		vid.recalc_refdef = 1;
	}
	//qmb :hud
	//Cvar_SetValue ("viewsize",scr_viewsize.value+10);
	//vid.recalc_refdef = 1;
}

void SCR_SizeDown_f(void) {
	//JHL:HACK; changed to affect the HUD, not SCR size
	if (hud.getInt() > 0) {
		hud.set(hud.getFloat() - 1);
		vid.recalc_refdef = 1;
	}
	//qmb :hud
	//Cvar_SetValue ("viewsize",scr_viewsize.value-10);
	//vid.recalc_refdef = 1;
}

//============================================================================

void SCR_Init(void) {
	CVar::registerCVar(&scr_fov);
	CVar::registerCVar(&scr_viewsize);
	CVar::registerCVar(&scr_conspeed);
	CVar::registerCVar(&scr_showturtle);
	CVar::registerCVar(&scr_showpause);
	CVar::registerCVar(&scr_centertime);
	CVar::registerCVar(&scr_printspeed);
	CVar::registerCVar(&gl_triplebuffer);

	// register our commands
	Cmd::addCmd("screenshot", SCR_ScreenShot_f);
	Cmd::addCmd("sizeup", SCR_SizeUp_f);
	Cmd::addCmd("sizedown", SCR_SizeDown_f);

	scr_net = Draw_PicFromWad("net");
	scr_turtle = Draw_PicFromWad("turtle");

	// CAPTURE <anthony@planetquake.com>
	CaptureHelper_Init();

	scr_initialized = true;
}

void SCR_DrawTurtle(void) {
	static int count;

	if (!scr_showturtle.getBool())
		return;

	if (host_frametime < 0.1) {
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_AlphaPic(scr_vrect.x, scr_vrect.y, scr_turtle, 1);
}

void SCR_DrawNet(void) {
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_AlphaPic(scr_vrect.x + 64, scr_vrect.y, scr_net, 1);
}

void SCR_DrawFPS(void) {
	extern CVar show_fps;
	static double lastframetime;
	double t;
	extern int fps_count;
	static int lastfps;
	static int totalfps;
	static int lastsecond;
	int x, y;
	char st[80];

	if (!show_fps.getBool())
		return;

	t = Sys_FloatTime();
	lastfps = 1 / (t - lastframetime);
	if (((int) (t) % 100) > ((int) (lastframetime) % 100)) {
		lastsecond = totalfps;
		totalfps = 0;
	}
	lastframetime = t;
	totalfps += 1;

	sprintf(st, "%3d FPS", lastfps);

	x = vid.conwidth - Q_strlen(st) * 8 - 16;
	y = 0;
	Draw_String(x, y, st);

	sprintf(st, "%3d Last second", lastsecond);
	x = vid.conwidth - Q_strlen(st) * 8 - 16;
	y = 8;
	Draw_String(x, y, st);
}

void Scr_ShowNumP(void) {
	extern CVar show_fps;
	int x, y;
	char st[80];
	extern int numParticles;

	if (!show_fps.getBool())
		return;

	sprintf(st, "%i Particles in world", numParticles);

	x = vid.conwidth - Q_strlen(st) * 8 - 16;
	y = 16; //vid.conheight - (sb_lines * (vid.conheight/240) )- 16;
	//Draw_TileClear(x, y, Q_strlen(st)*16, 16);
	Draw_String(x, y, st);
}

void SCR_DrawPause(void) {
	qpic_t *pic;

	if (!scr_showpause.getBool()) // turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic("gfx/pause.lmp");
	Draw_AlphaPic((vid.conwidth - pic->width) / 2, (vid.conheight - 48 - pic->height) / 2, pic, 1);
}

void SCR_DrawLoading(void) {
	qpic_t *pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic("gfx/loading.lmp");
	Draw_AlphaPic((vid.conwidth - pic->width) / 2, (vid.conheight - 48 - pic->height) / 2, pic, 1);
}

//=============================================================================

void SCR_SetUpToDrawConsole(void) {
	Con_CheckResize();

	if (scr_drawloading)
		return; // never a console with loading plaque

	// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup) {
		scr_conlines = vid.conheight; // full screen
		scr_con_current = scr_conlines;
	} else if (key_dest == key_console)
		scr_conlines = vid.conheight / 2; // half screen
	else
		scr_conlines = 0; // none visible

	if (scr_conlines < scr_con_current) {
		scr_con_current -= scr_conspeed.getFloat() * host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed.getFloat() * host_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (!(clearconsole++ < vid.numpages || clearnotify++ < vid.numpages))
		con_notifylines = 0;
}

void SCR_DrawConsole(void) {
	if (scr_con_current) {
		scr_copyeverything = 1;
		Con_DrawConsole(scr_con_current, true);
		clearconsole = 0;
	} else {
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify(); // only draw notify in game
	}
}

/*
==============================================================================
						SCREEN SHOTS
==============================================================================
 */

typedef struct _TargaHeader {
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

void SCR_ScreenShot_f(void) {
	byte *buffer;
	char pcxname[80];
	char checkname[MAX_OSPATH];
	int i, c, temp;
	//
	// find a file name to save it to
	//
	Q_strcpy(pcxname, "quake000.tga");

	for (i = 0; i <= 999; i++) {
		pcxname[5] = '0' + i / 100;
		pcxname[6] = '0' + i % 100 / 10;
		pcxname[7] = '0' + i % 10;
		snprintf(checkname, MAX_OSPATH, "%s/%s", com_gamedir, pcxname);
		if (Sys_FileTime(checkname) == -1)
			break; // file doesn't exist
	}
	if (i == 1000) {
		Con_Printf("SCR_ScreenShot_f: Couldn't create a PCX file, more than %i screenshots. \n", i);
		return;
	}

	buffer = (byte *) malloc(glwidth * glheight * 3 + 18);
	Q_memset(buffer, 0, 18);
	buffer[2] = 2; // uncompressed type
	buffer[12] = glwidth & 255;
	buffer[13] = glwidth >> 8;
	buffer[14] = glheight & 255;
	buffer[15] = glheight >> 8;
	buffer[16] = 24; // pixel size

	glReadPixels(glx, gly, glwidth, glheight, GL_BGR, GL_UNSIGNED_BYTE, buffer + 18);

	FileManager::WriteFile(pcxname, buffer, glwidth * glheight * 3 + 18);

	free(buffer);
	Con_Printf("Wrote %s\n", pcxname);
}

//=============================================================================

void SCR_BeginLoadingPlaque(void) {
	S_StopAllSounds(true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;

	// redraw with no console and the loading plaque
	Con_ClearNotify();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	scr_fullupdate = 0;
	SCR_UpdateScreen();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
	scr_fullupdate = 0;
}

void SCR_EndLoadingPlaque(void) {
	scr_disabled_for_loading = false;
	scr_fullupdate = 0;
	Con_ClearNotify();
}

//=============================================================================

const char *scr_notifystring;
bool scr_drawdialog;

void SCR_DrawNotifyString(void) {
	const char *start;
	int l;
	int j;
	int x, y;

	start = scr_notifystring;

	y = vid.conheight * 0.35;

	do {
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.conwidth - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8)
			Draw_Character(x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++; // skip the \n
	} while (1);
}

/**
 * Displays a text string in the center of the screen and waits for a Y or N
 * keypress.
 */
int SCR_ModalMessage(const char *text) {
	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;

	// draw a fresh screen
	scr_fullupdate = 0;
	scr_drawdialog = true;
	SCR_UpdateScreen();
	scr_drawdialog = false;

	S_ClearBuffer(); // so dma doesn't loop current sound

	do {
		key_count = -1; // wait for a key down and up
		Sys_SendKeyEvents();
	} while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

	scr_fullupdate = 0;
	SCR_UpdateScreen();

	return key_lastpress == 'y';
}

//=============================================================================

/**
 * Brings the console down and fades the palettes back to normal
 */
void SCR_BringDownConsole(void) {
	int i;

	scr_centertime_off = 0;

	for (i = 0; i < 20 && scr_conlines != scr_con_current; i++)
		SCR_UpdateScreen();

	cl.cshifts[0].percent = 0; // no area contents palette on next frame
	VID_SetPalette(host_basepal);
}

void SCR_TileClear(void) {
	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear(0, 0, r_refdef.vrect.x, vid.conheight - sb_lines);
		// right
		Draw_TileClear(r_refdef.vrect.x + r_refdef.vrect.width, 0,
				vid.conwidth - r_refdef.vrect.x + r_refdef.vrect.width,
				vid.conheight - sb_lines);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear(r_refdef.vrect.x, 0,
				r_refdef.vrect.x + r_refdef.vrect.width,
				r_refdef.vrect.y);
		// bottom
		Draw_TileClear(r_refdef.vrect.x,
				r_refdef.vrect.y + r_refdef.vrect.height,
				r_refdef.vrect.width,
				vid.conheight - sb_lines -
				(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

void Draw_Crosshair(int texnum, vec3_t colour, float alpha);

/**
 * This is called every frame, and can also be called explicitly to flush
 * text to the screen.
 * 
 * WARNING: be very careful calling this from elsewhere, because the refresh
 * needs almost the entire 256k of stack space!
 */
void SCR_UpdateScreen(void) {
	extern CVar hud_r, hud_g, hud_b, hud_a;
	vec3_t colour;

	colour[0] = hud_r.getFloat();
	colour[1] = hud_g.getFloat();
	colour[2] = hud_b.getFloat();

	vid.numpages = 2 + gl_triplebuffer.getInt();

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading) {
		if (realtime - scr_disabled_time > 60) {
			scr_disabled_for_loading = false;
			Con_Printf("load failed.\n");
		} else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return; // not initialized yet

	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);

	// determine size of refresh window
	if (oldfov != scr_fov.getInt()) {
		oldfov = scr_fov.getInt();
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.getFloat()) {
		oldscreensize = scr_viewsize.getFloat();
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef();

	// do 3D refresh drawing, and then update the screen
	SCR_SetUpToDrawConsole();

	V_RenderView();

	GL_Set2D();

	// draw any areas not covered by the refresh
	SCR_TileClear();
	Sbar_Draw();

	if (scr_drawdialog) {
		//Sbar_Draw ();
		Draw_FadeScreen();
		SCR_DrawNotifyString();
		scr_copyeverything = true;
	} else if (scr_drawloading) {
		SCR_DrawLoading();
		//Sbar_Draw ();
	} else if (cl.intermission == 1 && key_dest == key_game) {
		Sbar_IntermissionOverlay();
	} else if (cl.intermission == 2 && key_dest == key_game) {
		Sbar_FinaleOverlay();
		SCR_CheckDrawCenterString();
	} else {
		if (crosshair.getInt() < 32 && TextureManager::crosshair_tex[crosshair.getInt()] != 0)
			Draw_Crosshair(TextureManager::crosshair_tex[crosshair.getInt()], colour, 0.7f);
		else
			if (crosshair.getBool()) {
			float x = (vid.conwidth / 2) - 0;
			float y = (vid.conheight / 2) - 0;
			Draw_Character(x, y, '+');
		}

		SCR_DrawNet();
		SCR_DrawTurtle();
		SCR_DrawFPS();
		Scr_ShowNumP();
		SCR_DrawPause();
		SCR_CheckDrawCenterString();
		SCR_DrawConsole();
		M_Draw();
	}

	if (r_errors.getBool() && developer.getBool())
		checkGLError("After drawing HUD:");

	V_UpdatePalette();

	CaptureHelper_OnUpdateScreen();

	GL_EndRendering();
}
