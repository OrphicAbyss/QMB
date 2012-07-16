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
	texnum = TextureManager::LoadExternTexture(&texname[0], false, true, gl_sincity.getBool());
	if (!texnum) {
		sprintf(texname, "gfx/%s", name);
		texnum = TextureManager::LoadExternTexture(&texname[0], false, true, gl_sincity.getBool());
	}

	if (texnum) {
		p->height = height;
		p->width = width;
		gl->texnum = texnum;
	} else {
		gl->texnum = TextureManager::LoadInternTexture("", p->width, p->height, p->data, true, false, 1, gl_sincity.getBool());
	}

	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return p;
}

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

	// load the pic from disk
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
	gl->texnum = TextureManager::LoadInternTexture("", dat->width, dat->height, dat->data, false, false, 1, gl_sincity.getBool());
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}

/**
 * Initalise the draw/texture details
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

	Cmd::addCmd("gl_texturemode", &TextureManager::setTextureModeCMD);

	char_texture = TextureManager::LoadExternTexture("gfx/charset", false, true, gl_sincity.getBool());
	if (char_texture == 0) {
		byte *draw_chars = (byte *)W_GetLumpName("conchars");
		for (i = 0; i < 256 * 64; i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255; // proper transparent color

		// now turn them into textures
		char_texture = TextureManager::LoadInternTexture("charset", 128, 128, draw_chars, false, true, 1, false);
	}

	gl = (glpic_t *) conback->data;
	gl->texnum = TextureManager::LoadExternTexture("gfx/conback", false, true, gl_sincity.getBool());
	if (gl->texnum == 0) {
		start = Hunk_LowMark();

		cb = (qpic_t *) COM_LoadTempFile("gfx/conback.lmp");
		if (!cb)
			Sys_Error("Couldn't load gfx/conback.lmp");
		SwapPic(cb);

		conback->width = cb->width;
		conback->height = cb->height;
		ncdata = cb->data;

		gl->texnum = TextureManager::LoadInternTexture("gfx/conback", conback->width, conback->height, ncdata, false, false, 1, false);

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
	translate_texture = TextureManager::getTextureId();

	// get the other pics we need
	draw_disc = Draw_PicFromWadXY("disc", 48, 48);
	draw_backtile = Draw_PicFromWad("backtile");
	//qmb :detail texture
	detailtexture = TextureManager::LoadExternTexture("textures/detail", false, true, gl_sincity.getBool());
	detailtexture2 = TextureManager::LoadExternTexture("textures/detail2", false, true, gl_sincity.getBool());
	quadtexture = TextureManager::LoadExternTexture("textures/quad", false, true, false);
}

/**
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be 
 * smoothly scrolled off.
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

void Draw_String(int x, int y, const char *str) {
	while (*str) {
		Draw_Character(x, y, *str++);
		x += 8;
	}
}

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

void Draw_TransPic(int x, int y, qpic_t *pic) {
	if (x < 0 || (unsigned) (x + pic->width) > vid.conwidth || y < 0 ||
			(unsigned) (y + pic->height) > vid.conheight) {
		//Sys_Error ("Draw_TransPic: bad coordinates");
	}

	Draw_AlphaPic(x, y, pic, 1);
}

/**
 * Only used for the player color selection menu
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

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
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

void Draw_ConsoleBackground(int lines) {
	int y = (vid.conheight * 3) >> 2;

	if (lines > y)
		Draw_AlphaPic(0, lines - vid.conheight, conback, 1);
	else
		Draw_AlphaPic(0, lines - vid.conheight, conback, (float) (gl_conalpha.getFloat() * lines) / y);
}

/**
 * This repeats a 64*64 tile graphic to fill the screen around a sized down 
 * refresh window.
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

/**
 * Fills a box of pixels with a single color
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

/**
 * Fills a box of pixels with a single transparent color
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

/**
 * Fills a box of pixels with a single transparent color
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

/**
 * function that draws the crosshair to the center of the screen
 */
void Draw_Crosshair(int texnum, vec3_t colour, float alpha) {
	int x = 0;
	int y = 0;
	float xsize, ysize;

	// Default for if it isn't set...
	xsize = 32;
	ysize = 32;

	// Crosshair offset
	x = (vid.conwidth / 2) - 16; // was 14
	y = (vid.conheight / 2) - 8; // was 14

	// Start drawing
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

void Draw_FadeScreen(void) {
	vec3_t colour;
	float alpha[2];
	int start_x, start_y, end_x, end_y;

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

/**
 * Setup as if the screen was 320*200 (or overridden hud res
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
