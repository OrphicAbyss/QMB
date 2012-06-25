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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
#include "mathlib.h"
#include "Texture.h"

#include <GL/glu.h>

#define GL_COLOR_INDEX8_EXT     0x80E5

//qmb :detail texture
extern int detailtexture;
extern int detailtexture2;
extern int quadtexture;

CVar gl_nobind("gl_nobind", "0");
CVar gl_max_size("gl_max_size", "4096");
CVar gl_quick_texture_reload("gl_quick_texture_reload", "1", true);
CVar gl_sincity("gl_sincity", "0", true);

qpic_t *draw_disc;
qpic_t *draw_backtile;

int translate_texture;
int char_texture;

typedef struct {
	int texnum;
	float sl, tl, sh, th;
} glpic_t;

byte conback_buffer[sizeof (qpic_t) + sizeof (glpic_t)];
qpic_t *conback = (qpic_t *) & conback_buffer;

int gl_lightmap_format = GL_BGRA;
int gl_alpha_format = GL_RGBA;

int gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int gl_filter_max = GL_LINEAR;

//=============================================================================

/* Support Routines */
typedef struct cachepic_s {
	char name[MAX_QPATH];
	qpic_t pic;
	byte padding[32]; // for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t menu_cachepics[MAX_CACHED_PICS];
int menu_numcachepics;

byte menuplyr_pixels[4096];

int pic_texels;
int pic_count;

qpic_t *Draw_PicFromWad(const char *name) {
	return Draw_PicFromWadXY(name,0,0);
}

qpic_t *Draw_PicFromWadXY(const char *name, int height, int width) {
	qpic_t *p;
	glpic_t *gl;
	int texnum;
	char texname[128];

	p = (qpic_t *) W_GetLumpName(name);
	gl = (glpic_t *) p->data;

	sprintf(texname, "textures/wad/%s", name);
	texnum = GL_LoadTexImage(&texname[0], false, false, gl_sincity.getBool());
	if (!texnum) {
		sprintf(texname, "gfx/%s", name);
		texnum = GL_LoadTexImage(&texname[0], false, false, gl_sincity.getBool());
	}

	if (texnum) {
		p->height = height;
		p->width = width;
		gl->texnum = texnum;
	} else {
		gl->texnum = GL_LoadTexture("", p->width, p->height, p->data, false, false, true, gl_sincity.getBool());
	}

	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return p;
}

/*
================
Draw_CachePic
================
 */
qpic_t *Draw_CachePic(const char *path) {
	cachepic_t *pic;
	int i;
	qpic_t *dat;
	glpic_t *gl;

	for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
		if (!Q_strcmp(path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	Q_strcpy(pic->name, path);

	//
	// load the pic from disk
	//
	dat = (qpic_t *) COM_LoadTempFile(path);
	if (!dat)
		Sys_Error("Draw_CachePic: failed to load %s", path);
	SwapPic(dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!Q_strcmp(path, "gfx/menuplyr.lmp"))
		Q_memcpy(menuplyr_pixels, dat->data, dat->width * dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *) pic->pic.data;
	gl->texnum = GL_LoadTexture("", dat->width, dat->height, dat->data, false, false, 1, gl_sincity.getBool());
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}

typedef struct {
	char *name;
	int minimize;
	int maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
 */
void Draw_TextureMode_f(void) {
	int i;

	if (CmdArgs::getArgCount() == 1) {
		for (i = 0; i < 6; i++)
			if (gl_filter_min == modes[i].minimize) {
				Con_Printf("%s\n", modes[i].name);
				return;
			}
		Con_Printf("current filter is unknown???\n");
		return;
	}

	for (i = 0; i < 6; i++) {
		if (!Q_strcasecmp(modes[i].name, CmdArgs::getArg(1)))
			break;
	}
	if (i == 6) {
		Con_Printf("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	TextureManager::resetTextureModes();
}

/*
===============
Draw_Init
===============
 */
void Draw_Init(void) {
	int i;
	qpic_t *cb;
	glpic_t *gl;
	int start;
	byte *ncdata;

	CVar::registerCVar(&gl_nobind);
	CVar::registerCVar(&gl_max_size);
	CVar::registerCVar(&gl_quick_texture_reload);
	CVar::registerCVar(&gl_sincity);

	Cmd::addCmd("gl_texturemode", &Draw_TextureMode_f);

	char_texture = GL_LoadTexImage("gfx/charset", false, true, gl_sincity.getBool());
	if (char_texture == 0) {
		byte *draw_chars = (byte *)W_GetLumpName("conchars");
		for (i = 0; i < 256 * 64; i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255; // proper transparent color

		// now turn them into textures
		char_texture = GL_LoadTexture("charset", 128, 128, draw_chars, false, true, 1, false);
	}

	gl = (glpic_t *) conback->data;
	gl->texnum = GL_LoadTexImage("gfx/conback", false, true, gl_sincity.getBool());
	if (gl->texnum == 0) {
		start = Hunk_LowMark();

		cb = (qpic_t *) COM_LoadTempFile("gfx/conback.lmp");
		if (!cb)
			Sys_Error("Couldn't load gfx/conback.lmp");
		SwapPic(cb);

		conback->width = cb->width;
		conback->height = cb->height;
		ncdata = cb->data;

		gl->texnum = GL_LoadTexture("gfx/conback", conback->width, conback->height, ncdata, false, false, 1, false);

		// free loaded console
		Hunk_FreeToLowMark(start);
	}
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	conback->width = vid.conwidth;
	conback->height = vid.conheight;

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWadXY("disc", 48, 48);
	draw_backtile = Draw_PicFromWad("backtile");
	//qmb :detail texture
	detailtexture = GL_LoadTexImage("textures/detail", false, true, gl_sincity.getBool());
	detailtexture2 = GL_LoadTexImage("textures/detail2", false, true, gl_sincity.getBool());
	quadtexture = GL_LoadTexImage("textures/quad", false, true, false);
}

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
 */
void Draw_Character(int x, int y, int num) {
	int row, col;
	float frow, fcol, size;

	if (num == 32)
		return; // space

	num &= 255;

	if (y <= -8)
		return; // totally off screen

	row = num >> 4;
	col = num & 15;

	//qmb :larger text??
	frow = row * 0.0625f;
	fcol = col * 0.0625f;
	size = 0.0625f;

	glBindTexture(GL_TEXTURE_2D, char_texture);

	glBegin(GL_QUADS);
	glTexCoord2f(fcol, frow);
	glVertex2f(x, y);
	glTexCoord2f(fcol + size, frow);
	glVertex2f(x + 8, y);
	glTexCoord2f(fcol + size, frow + size);
	glVertex2f(x + 8, y + 8);
	glTexCoord2f(fcol, frow + size);
	glVertex2f(x, y + 8);
	glEnd();
}

/*
================
Draw_String
================
 */
void Draw_String(int x, int y, const char *str) {
	while (*str) {
		Draw_Character(x, y, *str++);
		x += 8;
	}
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
 */
void Draw_DebugChar(char num) {
}

/*
=============
Draw_AlphaColourPic
=============
 */
void Draw_AlphaColourPic(int x, int y, qpic_t *pic, vec3_t colour, float alpha) {
	glpic_t *gl;

	gl = (glpic_t *) pic->data;

	if (alpha != 1) {
		glDisable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	glColor4f(colour[0], colour[1], colour[2], alpha);
	glBindTexture(GL_TEXTURE_2D, gl->texnum);
	glBegin(GL_QUADS);
	glTexCoord2f(gl->sl, gl->tl);
	glVertex2f(x, y);
	glTexCoord2f(gl->sh, gl->tl);
	glVertex2f(x + pic->width, y);
	glTexCoord2f(gl->sh, gl->th);
	glVertex2f(x + pic->width, y + pic->height);
	glTexCoord2f(gl->sl, gl->th);
	glVertex2f(x, y + pic->height);
	glEnd();
	glColor4f(1, 1, 1, 1);

	if (alpha != 1) {
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
	}
}

/*
=============
Draw_AlphaPic
=============
 */
void Draw_AlphaPic(int x, int y, qpic_t *pic, float alpha) {
	glpic_t *gl;

	gl = (glpic_t *) pic->data;

	if (alpha != 1) {
		glDisable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	glColor4f(1, 1, 1, alpha);
	glBindTexture(GL_TEXTURE_2D, gl->texnum);
	glBegin(GL_QUADS);
	glTexCoord2f(gl->sl, gl->tl);
	glVertex2f(x, y);
	glTexCoord2f(gl->sh, gl->tl);
	glVertex2f(x + pic->width, y);
	glTexCoord2f(gl->sh, gl->th);
	glVertex2f(x + pic->width, y + pic->height);
	glTexCoord2f(gl->sl, gl->th);
	glVertex2f(x, y + pic->height);
	glEnd();
	glColor4f(1, 1, 1, 1);

	if (alpha != 1) {
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
	}
}

/*
=============
Draw_TransPic
=============
 */
void Draw_TransPic(int x, int y, qpic_t *pic) {
	if (x < 0 || (unsigned) (x + pic->width) > vid.conwidth || y < 0 ||
			(unsigned) (y + pic->height) > vid.conheight) {
		//Sys_Error ("Draw_TransPic: bad coordinates");
	}

	Draw_AlphaPic(x, y, pic, 1);
}

/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
 */
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation) {
	int v, u, c;
	unsigned trans[64 * 64], *dest;
	byte *src;
	int p;

	glBindTexture(GL_TEXTURE_2D, translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v = 0; v < 64; v++, dest += 64) {
		src = &menuplyr_pixels[ ((v * pic->height) >> 6) * pic->width];
		for (u = 0; u < 64; u++) {
			p = src[(u * pic->width) >> 6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] = d_8to24table[translation[p]];
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glColor3f(1, 1, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(x, y);
	glTexCoord2f(1, 0);
	glVertex2f(x + pic->width, y);
	glTexCoord2f(1, 1);
	glVertex2f(x + pic->width, y + pic->height);
	glTexCoord2f(0, 1);
	glVertex2f(x, y + pic->height);
	glEnd();
}

/*
================
Draw_ConsoleBackground

================
 */
void Draw_ConsoleBackground(int lines) {
	int y = (vid.conheight * 3) >> 2;

	if (lines > y)
		Draw_AlphaPic(0, lines - vid.conheight, conback, 1);
	else
		Draw_AlphaPic(0, lines - vid.conheight, conback, (float) (gl_conalpha.getFloat() * lines) / y);
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
 */
void Draw_TileClear(int x, int y, int w, int h) {
	glColor4f(0, 0.5f, 0.5f, 0.5f);
	glBindTexture(GL_TEXTURE_2D, *(int *) draw_backtile->data);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBegin(GL_QUADS);
	//glTexCoord2f (x/64.0, y/64.0);
	glVertex2f(x, y);
	//glTexCoord2f ( (x+w)/64.0, y/64.0);
	glVertex2f(x + w, y);
	//glTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	glVertex2f(x + w, y + h);
	//glTexCoord2f ( x/64.0, (y+h)/64.0 );
	glVertex2f(x, y + h);
	glEnd();
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
 */
void Draw_Fill(int x, int y, int w, int h, int c) {
	glDisable(GL_TEXTURE_2D);
	glColor3f(host_basepal[c * 3] / 255.0,
			host_basepal[c * 3 + 1] / 255.0,
			host_basepal[c * 3 + 2] / 255.0);

	glBegin(GL_QUADS);

	glVertex2f(x, y);
	glVertex2f(x + w, y);
	glVertex2f(x + w, y + h);
	glVertex2f(x, y + h);

	glEnd();
	glColor3f(1, 1, 1);
	glEnable(GL_TEXTURE_2D);
}

/*
=============
Draw_AlphaFill

JHL: Fills a box of pixels with a single transparent color
=============
 */
void Draw_AlphaFill(int x, int y, int width, int height, vec3_t colour, float alpha) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glColor4f(colour[0] / 255, colour[1] / 255, colour[2] / 255, alpha / 255);
	glBegin(GL_QUADS);

	glVertex2f(x, y);
	glVertex2f(x + width, y);
	glVertex2f(x + width, y + height);
	glVertex2f(x, y + height);

	glEnd();
	glColor4f(1, 1, 1, 1);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

/*
=============
Draw_AlphaFillFade

JHL: Fills a box of pixels with a single transparent color
=============
 */
void Draw_AlphaFillFade(int x, int y, int width, int height, vec3_t colour, float alpha[2]) {
	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBegin(GL_QUADS);

	glColor4f(colour[0] / 255, colour[1] / 255, colour[2] / 255, alpha[1] / 255);
	glVertex2f(x, y);
	glVertex2f(x, y + height);
	glColor4f(colour[0] / 255, colour[1] / 255, colour[2] / 255, alpha[0] / 255);
	glVertex2f(x + width, y + height);
	glVertex2f(x + width, y);

	glEnd();

	glColor4f(1, 1, 1, 1);

	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

/*
===============
Draw_Crosshair

function that draws the crosshair to the center of the screen
===============
 */
void Draw_Crosshair(int texnum, vec3_t colour, float alpha) {
	int x = 0;
	int y = 0;
	float xsize, ysize;

	//
	// Default for if it isn't set...
	//
	xsize = 32;
	ysize = 32;

	//
	// Crosshair offset
	//
	x = (vid.conwidth / 2) - 16; // was 14
	y = (vid.conheight / 2) - 8; // was 14

	//
	// Start drawing
	//
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glColor4f(colour[0], colour[1], colour[2], alpha);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, texnum);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(x, y);
	glTexCoord2f(1, 0);
	glVertex2f(x + xsize, y);
	glTexCoord2f(1, 1);
	glVertex2f(x + xsize, y + ysize);
	glTexCoord2f(0, 1);
	glVertex2f(x, y + ysize);
	glEnd();

	// restore display settings
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);
	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
 */
void Draw_FadeScreen(void) {
	vec3_t colour;
	float alpha[2];
	//JHL; changed...
	int start_x, start_y,
			end_x, end_y;

	start_x = (vid.conwidth - 320) / 2;
	start_y = (vid.conheight - 240) / 2;
	end_x = (vid.conwidth + 320) / 2;
	end_y = (vid.conheight + 240) / 2;
	colour[0] = colour[1] = colour[2] = 0;
	alpha[0] = 0;
	alpha[1] = 178;

	Draw_AlphaFill(start_x, start_y, end_x - start_x, end_y - start_y, colour, alpha[1]);
	Draw_AlphaFillFade(start_x, start_y, -10, end_y - start_y, colour, alpha);
	Draw_AlphaFillFade(end_x, start_y, 10, end_y - start_y, colour, alpha);
}

//=============================================================================

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
 */
void GL_Set2D(void) {
	glViewport(glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, vid.conwidth, vid.conheight, 0, -1000, 1000);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	//	glDisable (GL_ALPHA_TEST);

	glColor4f(1, 1, 1, 1);
}

//====================================================================

void Image_Resample32LerpLine(const byte *in, byte *out, int inwidth, int outwidth) {
	int j, xi, oldx = 0, f, fstep, endx, lerp;
	fstep = (int) (inwidth * 65536.0f / outwidth);
	endx = (inwidth - 1);
	for (j = 0, f = 0; j < outwidth; j++, f += fstep) {
		xi = f >> 16;
		if (xi != oldx) {
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx) {
			lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (byte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		} else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

void Image_Resample24LerpLine(const byte *in, byte *out, int inwidth, int outwidth) {
	int j, xi, oldx = 0, f, fstep, endx, lerp;
	fstep = (int) (inwidth * 65536.0f / outwidth);
	endx = (inwidth - 1);
	for (j = 0, f = 0; j < outwidth; j++, f += fstep) {
		xi = f >> 16;
		if (xi != oldx) {
			in += (xi - oldx) * 3;
			oldx = xi;
		}
		if (xi < endx) {
			lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[3] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[4] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[5] - in[2]) * lerp) >> 16) + in[2]);
		} else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
		}
	}
}

int resamplerowsize = 0;
byte *resamplerow1 = NULL;
byte *resamplerow2 = NULL;

#define LERPBYTE(i) r = resamplerow1[i];out[i] = (byte) ((((resamplerow2[i] - r) * lerp) >> 16) + r)

void Image_Resample32Lerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight) {
	int i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight - 1), inwidth4 = inwidth * 4, outwidth4 = outwidth * 4;
	byte *out;
	const byte *inrow;
	out = (byte *) outdata;
	fstep = (int) (inheight * 65536.0f / outheight);

	inrow = (const byte *) indata;
	oldy = 0;
	Image_Resample32LerpLine(inrow, resamplerow1, inwidth, outwidth);
	Image_Resample32LerpLine(inrow + inwidth4, resamplerow2, inwidth, outwidth);
	for (i = 0, f = 0; i < outheight; i++, f += fstep) {
		yi = f >> 16;
		if (yi < endy) {
			lerp = f & 0xFFFF;
			if (yi != oldy) {
				inrow = (byte *) indata + inwidth4*yi;
				if (yi == oldy + 1)
					Q_memcpy(resamplerow1, resamplerow2, outwidth4);
				else
					Image_Resample32LerpLine(inrow, resamplerow1, inwidth, outwidth);
				Image_Resample32LerpLine(inrow + inwidth4, resamplerow2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while (j >= 0) {
				LERPBYTE(0);
				LERPBYTE(1);
				LERPBYTE(2);
				LERPBYTE(3);
				LERPBYTE(4);
				LERPBYTE(5);
				LERPBYTE(6);
				LERPBYTE(7);
				LERPBYTE(8);
				LERPBYTE(9);
				LERPBYTE(10);
				LERPBYTE(11);
				LERPBYTE(12);
				LERPBYTE(13);
				LERPBYTE(14);
				LERPBYTE(15);
				out += 16;
				resamplerow1 += 16;
				resamplerow2 += 16;
				j -= 4;
			}
			if (j & 2) {
				LERPBYTE(0);
				LERPBYTE(1);
				LERPBYTE(2);
				LERPBYTE(3);
				LERPBYTE(4);
				LERPBYTE(5);
				LERPBYTE(6);
				LERPBYTE(7);
				out += 8;
				resamplerow1 += 8;
				resamplerow2 += 8;
			}
			if (j & 1) {
				LERPBYTE(0);
				LERPBYTE(1);
				LERPBYTE(2);
				LERPBYTE(3);
				out += 4;
				resamplerow1 += 4;
				resamplerow2 += 4;
			}
			resamplerow1 -= outwidth4;
			resamplerow2 -= outwidth4;
		} else {
			if (yi != oldy) {
				inrow = (byte *) indata + inwidth4*yi;
				if (yi == oldy + 1)
					Q_memcpy(resamplerow1, resamplerow2, outwidth4);
				else
					Image_Resample32LerpLine(inrow, resamplerow1, inwidth, outwidth);
				oldy = yi;
			}
			Q_memcpy(out, resamplerow1, outwidth4);
		}
	}
}

void Image_Resample32Nearest(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight) {
	int i, j;
	unsigned frac, fracstep;
	// relies on int being 4 bytes
	int *inrow, *out;
	out = (int *) outdata;

	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++) {
		inrow = (int *) indata + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		j = outwidth - 4;
		while (j >= 0) {
			out[0] = inrow[frac >> 16];
			frac += fracstep;
			out[1] = inrow[frac >> 16];
			frac += fracstep;
			out[2] = inrow[frac >> 16];
			frac += fracstep;
			out[3] = inrow[frac >> 16];
			frac += fracstep;
			out += 4;
			j -= 4;
		}
		if (j & 2) {
			out[0] = inrow[frac >> 16];
			frac += fracstep;
			out[1] = inrow[frac >> 16];
			frac += fracstep;
			out += 2;
		}
		if (j & 1) {
			out[0] = inrow[frac >> 16];
			frac += fracstep;
			out += 1;
		}
	}
}

void Image_Resample24Lerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight) {
	int i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight - 1), inwidth3 = inwidth * 3, outwidth3 = outwidth * 3;
	byte *out;
	const byte *inrow;
	out = (byte *) outdata;
	fstep = (int) (inheight * 65536.0f / outheight);

	inrow = (const byte *) indata;
	oldy = 0;
	Image_Resample24LerpLine(inrow, resamplerow1, inwidth, outwidth);
	Image_Resample24LerpLine(inrow + inwidth3, resamplerow2, inwidth, outwidth);
	for (i = 0, f = 0; i < outheight; i++, f += fstep) {
		yi = f >> 16;
		if (yi < endy) {
			lerp = f & 0xFFFF;
			if (yi != oldy) {
				inrow = (byte *) indata + inwidth3*yi;
				if (yi == oldy + 1)
					Q_memcpy(resamplerow1, resamplerow2, outwidth3);
				else
					Image_Resample24LerpLine(inrow, resamplerow1, inwidth, outwidth);
				Image_Resample24LerpLine(inrow + inwidth3, resamplerow2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while (j >= 0) {
				LERPBYTE(0);
				LERPBYTE(1);
				LERPBYTE(2);
				LERPBYTE(3);
				LERPBYTE(4);
				LERPBYTE(5);
				LERPBYTE(6);
				LERPBYTE(7);
				LERPBYTE(8);
				LERPBYTE(9);
				LERPBYTE(10);
				LERPBYTE(11);
				out += 12;
				resamplerow1 += 12;
				resamplerow2 += 12;
				j -= 4;
			}
			if (j & 2) {
				LERPBYTE(0);
				LERPBYTE(1);
				LERPBYTE(2);
				LERPBYTE(3);
				LERPBYTE(4);
				LERPBYTE(5);
				out += 6;
				resamplerow1 += 6;
				resamplerow2 += 6;
			}
			if (j & 1) {
				LERPBYTE(0);
				LERPBYTE(1);
				LERPBYTE(2);
				out += 3;
				resamplerow1 += 3;
				resamplerow2 += 3;
			}
			resamplerow1 -= outwidth3;
			resamplerow2 -= outwidth3;
		} else {
			if (yi != oldy) {
				inrow = (byte *) indata + inwidth3*yi;
				if (yi == oldy + 1)
					Q_memcpy(resamplerow1, resamplerow2, outwidth3);
				else
					Image_Resample24LerpLine(inrow, resamplerow1, inwidth, outwidth);
				oldy = yi;
			}
			Q_memcpy(out, resamplerow1, outwidth3);
		}
	}
}

void Image_Resample24Nolerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight) {
	int i, j, f, inwidth3 = inwidth * 3;
	unsigned frac, fracstep;
	byte *inrow, *out;
	out = (byte *) outdata;

	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++) {
		inrow = (byte *) indata + inwidth3 * (i * inheight / outheight);
		frac = fracstep >> 1;
		j = outwidth - 4;
		while (j >= 0) {
			f = (frac >> 16)*3;
			*out++ = inrow[f + 0];
			*out++ = inrow[f + 1];
			*out++ = inrow[f + 2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f + 0];
			*out++ = inrow[f + 1];
			*out++ = inrow[f + 2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f + 0];
			*out++ = inrow[f + 1];
			*out++ = inrow[f + 2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f + 0];
			*out++ = inrow[f + 1];
			*out++ = inrow[f + 2];
			frac += fracstep;
			j -= 4;
		}
		if (j & 2) {
			f = (frac >> 16)*3;
			*out++ = inrow[f + 0];
			*out++ = inrow[f + 1];
			*out++ = inrow[f + 2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f + 0];
			*out++ = inrow[f + 1];
			*out++ = inrow[f + 2];
			frac += fracstep;
			out += 2;
		}
		if (j & 1) {
			f = (frac >> 16)*3;
			*out++ = inrow[f + 0];
			*out++ = inrow[f + 1];
			*out++ = inrow[f + 2];
			frac += fracstep;
			out += 1;
		}
	}
}

/*
================
Image_Resample
================
 */
void Image_Resample(const void *indata, int inwidth, int inheight, int indepth, void *outdata, int outwidth, int outheight, int outdepth, int bytesperpixel, int quality) {
	if (indepth != 1 || outdepth != 1)
		Sys_Error("Image_Resample: 3D resampling not supported\n");
	if (resamplerowsize < outwidth * 4) {
		if (resamplerow1)
			free(resamplerow1);
		resamplerowsize = outwidth * 4;
		resamplerow1 = (byte *) malloc(resamplerowsize * 2);
		resamplerow2 = resamplerow1 + resamplerowsize;
	}
	if (bytesperpixel == 4) {
		if (quality)
			Image_Resample32Lerp(indata, inwidth, inheight, outdata, outwidth, outheight);
		else
			Image_Resample32Nearest(indata, inwidth, inheight, outdata, outwidth, outheight);
	} else if (bytesperpixel == 3) {
		if (quality)
			Image_Resample24Lerp(indata, inwidth, inheight, outdata, outwidth, outheight);
		else
			Image_Resample24Nolerp(indata, inwidth, inheight, outdata, outwidth, outheight);
	} else
		Sys_Error("Image_Resample: unsupported bytesperpixel %i\n", bytesperpixel);
}

// in can be the same as out
void Image_MipReduce(const byte *in, byte *out, int *width, int *height, int *depth, int destwidth, int destheight, int destdepth, int bytesperpixel) {
	int x, y, nextrow;
	if (*depth != 1 || destdepth != 1)
		Sys_Error("Image_Resample: 3D resampling not supported\n");
	nextrow = *width * bytesperpixel;
	if (*width > destwidth) {
		*width >>= 1;
		if (*height > destheight) {
			// reduce both
			*height >>= 1;
			if (bytesperpixel == 4) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[4] + in[nextrow ] + in[nextrow + 4]) >> 2);
						out[1] = (byte) ((in[1] + in[5] + in[nextrow + 1] + in[nextrow + 5]) >> 2);
						out[2] = (byte) ((in[2] + in[6] + in[nextrow + 2] + in[nextrow + 6]) >> 2);
						out[3] = (byte) ((in[3] + in[7] + in[nextrow + 3] + in[nextrow + 7]) >> 2);
						out += 4;
						in += 8;
					}
					in += nextrow; // skip a line
				}
			} else if (bytesperpixel == 3) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[3] + in[nextrow ] + in[nextrow + 3]) >> 2);
						out[1] = (byte) ((in[1] + in[4] + in[nextrow + 1] + in[nextrow + 4]) >> 2);
						out[2] = (byte) ((in[2] + in[5] + in[nextrow + 2] + in[nextrow + 5]) >> 2);
						out += 3;
						in += 6;
					}
					in += nextrow; // skip a line
				}
			} else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		} else {
			// reduce width
			if (bytesperpixel == 4) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[4]) >> 1);
						out[1] = (byte) ((in[1] + in[5]) >> 1);
						out[2] = (byte) ((in[2] + in[6]) >> 1);
						out[3] = (byte) ((in[3] + in[7]) >> 1);
						out += 4;
						in += 8;
					}
				}
			} else if (bytesperpixel == 3) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[3]) >> 1);
						out[1] = (byte) ((in[1] + in[4]) >> 1);
						out[2] = (byte) ((in[2] + in[5]) >> 1);
						out += 3;
						in += 6;
					}
				}
			} else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		}
	} else {
		if (*height > destheight) {
			// reduce height
			*height >>= 1;
			if (bytesperpixel == 4) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[nextrow ]) >> 1);
						out[1] = (byte) ((in[1] + in[nextrow + 1]) >> 1);
						out[2] = (byte) ((in[2] + in[nextrow + 2]) >> 1);
						out[3] = (byte) ((in[3] + in[nextrow + 3]) >> 1);
						out += 4;
						in += 4;
					}
					in += nextrow; // skip a line
				}
			} else if (bytesperpixel == 3) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[nextrow ]) >> 1);
						out[1] = (byte) ((in[1] + in[nextrow + 1]) >> 1);
						out[2] = (byte) ((in[2] + in[nextrow + 2]) >> 1);
						out += 3;
						in += 3;
					}
					in += nextrow; // skip a line
				}
			} else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		} else
			Sys_Error("Image_MipReduce: desired size already achieved\n");
	}
}

/*
===============
GL_Upload32

first converts strange sized textures
to ones in the form 2^n*2^m where n is the height and m is the width
===============
 */
static unsigned char tobig[] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};

__inline unsigned RGBAtoGrayscale(unsigned rgba) {
	unsigned char *rgb, value;
	unsigned output;
	double shift;

	output = rgba;
	rgb = ((unsigned char *) &output);

	value = min(255, rgb[0] * 0.2125 + rgb[1] * 0.7154 + rgb[2] * 0.0721);
	//shift = sqrt(max(0,(rgb[0])-(rgb[1]+rgb[2])/2))/16;
	//value = min(255,rgb[0] * 0.299 + rgb[1] * 0.587 + rgb[2] * 0.114);

#if 1
	rgb[0] = value;
	rgb[1] = value;
	rgb[2] = value;
#else
	rgb[0] = rgb[0] + (value - rgb[0]) * shift;
	rgb[1] = rgb[1] + (value - rgb[1]) * shift;
	rgb[2] = rgb[2] + (value - rgb[2]) * shift;
#endif

	return output;
}

void GL_Upload32(unsigned int *data, int width, int height, qboolean mipmap, qboolean grayscale) {
	static unsigned temp_buffer[512 * 512];
	int samples;
	unsigned *scaled;
	int scaled_width, scaled_height;

	if (gl_texture_non_power_of_two) {
		scaled_width = width;
		scaled_height = height;

		//this seems buggered (will squash really large textures, but then again, not may huge textures around)
		//if (scaled_width > gl_max_size.value) scaled_width = gl_max_size.value; //make sure its not bigger than the max size
		//if (scaled_height > gl_max_size.value) scaled_height = gl_max_size.value;//make sure its not bigger than the max size
	} else {
		scaled_width = 1 << (int) ceil(log(width) / log(2.0));
		scaled_height = 1 << (int) ceil(log(height) / log(2.0));

		//this seems buggered (will squash really large textures, but then again, not may huge textures around)
		scaled_width = min(scaled_width, gl_max_size.getInt()); //make sure its not bigger than the max size
		scaled_height = min(scaled_height, gl_max_size.getInt()); //make sure its not bigger than the max size
	}

	samples = gl_alpha_format; //internal format

	if (scaled_width * scaled_height < 512 * 512) //see if we can use our buffer
		scaled = &temp_buffer[0];
	else {
		scaled = (unsigned *) malloc(sizeof (unsigned) *scaled_width * scaled_height);
		Con_SafeDPrintf("&c500Upload32:&r Needed Dynamic Buffer for texture resize...\n");

		if (scaled == NULL)
			Sys_Error("GL_LoadTexture: texture is too big, cannot resample textures bigger than %i", scaled_width * scaled_height);
	}


	if (scaled_width == width && scaled_height == height) {
		//not being scaled
		if (grayscale && gl_sincity.getBool()) {
			//go through data and convert
			int size = width * height;
			for (int i = 0; i < size; i++) {
				data[i] = RGBAtoGrayscale(data[i]);
			}
		}
		if (!mipmap) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		} else { //else build mipmaps for it
			if (gl_sgis_mipmap) {
				glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
			} else {
				gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, scaled_width, scaled_height, GL_RGBA, GL_UNSIGNED_BYTE, data);
			}
		}
	} else { //if we need to scale
		Con_SafeDPrintf("&c500Upload32:&r Textures too big or not a power of two in size: %ix%i...\n", width, height);

		//fix size so its a power of 2
		Image_Resample(data, width, height, 1, scaled, scaled_width, scaled_height, 1, 4, 1);

		//was scaled
		if (grayscale && gl_sincity.getBool()) {
			//go through data and convert
			int size = scaled_width * scaled_height;
			for (int i = 0; i < size; i++) {
				scaled[i] = RGBAtoGrayscale(scaled[i]);
			}
		}

		if (gl_sgis_mipmap) {
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
			glTexImage2D(GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
		} else {
			gluBuild2DMipmaps(GL_TEXTURE_2D, samples, scaled_width, scaled_height, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}

		if (scaled != &temp_buffer[0])
			free(scaled);
	}

	if (mipmap) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic.getBool());
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic.getBool());
	}
}

/*
===============
GL_Upload8
===============
 */
int GL_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean fullbright) {
	static unsigned temp_buffer[512 * 256];
	unsigned *trans;
	
	int s = width*height;
	if (s < 512 * 256) {
		trans = &temp_buffer[0];
		//Q_memset(trans, 0, 512 * 256 * sizeof (unsigned));
	} else {
		trans = (unsigned *) malloc(s * sizeof (unsigned));
		Con_SafeDPrintf("&c500GL_Upload8:&r Needed Dynamic Buffer for 8bit to 24bit texture convert...\n");
	}
	// if there are no transparent pixels, make it a 3 component (ie: RGB not RGBA)
	// texture even if it was specified as otherwise
	if (fullbright) {
		// this is a fullbright mask, so make all non-fullbright colors transparent
		bool hasFullbright = false;
		for (int i = 0; i < s; i++) {
			int p = data[i];
			if (p < 224) {
				trans[i] = d_8to24table[255]; // transparent
			} else {
				trans[i] = d_8to24table[p]; // fullbright
				hasFullbright = true;
			}
		}
		if (!hasFullbright)
			return false;
	} else {
		for (int i = 0; i < s; i += 4) {
			if (gl_sincity.getBool()) {
				trans[i] = RGBAtoGrayscale(d_8to24table[data[i]]);
				trans[i + 1] = RGBAtoGrayscale(d_8to24table[data[i + 1]]);
				trans[i + 2] = RGBAtoGrayscale(d_8to24table[data[i + 2]]);
				trans[i + 3] = RGBAtoGrayscale(d_8to24table[data[i + 3]]);
			} else {
				trans[i] = d_8to24table[data[i]];
				trans[i + 1] = d_8to24table[data[i + 1]];
				trans[i + 2] = d_8to24table[data[i + 2]];
				trans[i + 3] = d_8to24table[data[i + 3]];
			}
		}
	}

	GL_Upload32(trans, width, height, mipmap, false);

	if (trans != &temp_buffer[0])
		free(trans);

	return true;
}

Texture *GL_LoadTexture(Texture *t) {
	// See if the texture has already been loaded
	t->calculateHash();
	Texture *found = TextureManager::findTexture(t);
	if (found != NULL) {
		if (found->isMissMatch(t)) {
			Con_DPrintf("GL_LoadTexture: cache mismatch\n");
			TextureManager::removeTexture(found);
			delete found;
		} else {
			delete t;
			return found;
		}
	}
	
	t->textureId = texture_extension_number++;
	
	if (!isDedicated) {
		glBindTexture(GL_TEXTURE_2D, t->textureId);
		
		t->upload();
	}
	
	TextureManager::addTexture(t);
	return t;
}

int GL_LoadTexture(const char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean fullbright, int bytesperpixel, qboolean grayscale) {
	if (fullbright)
		return 0;

	Texture *t = new Texture(identifier);
	t->data = data;
	t->height = height;
	t->width = width;
	t->bytesPerPixel = bytesperpixel;
	t->mipmap = mipmap;
	t->grayscale = grayscale;
	
	t = GL_LoadTexture(t);
	
	return t->textureId;
}

int GL_LoadTexImage(char* filename, qboolean complain, qboolean mipmap, qboolean grayscale) {
	Texture *t;

	if (gl_quick_texture_reload.getBool()) {
		t = TextureManager::findTexture(filename);
		if (t != NULL)
			return t->textureId;
	}

	t = TextureManager::LoadFile(filename, complain);
	
	if (!t)
		return 0;
	
	if (!t->data) {
		delete t;
		return 0;
	}
	
	t->mipmap = mipmap;
	t->bytesPerPixel = 4;
	t->grayscale = grayscale;
	
	t = GL_LoadTexture(t);
	return t->textureId;
}
