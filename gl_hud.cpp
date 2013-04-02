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

/*
QC hud stuff

Written by DrLabman
 */

#include "quakedef.h"

//copied from gl_draw.c

typedef struct glpic_s {
	int texnum;
	float sl, tl, sh, th;
} glpic_t;

typedef struct qmb_hud_s {
	float x, y;
	float width, height;
	vec3_t colour;
	float alpha;
	char *text;
	byte used;
	struct glpic_s *pic;
	struct qmb_hud_s *next;
} qmb_hud_t;

#define MAX_QMB_HUD 256

qmb_hud_t *huds;

void R_ClearHuds(void) {
	for (int i = 0; i < MAX_QMB_HUD; i++) {
		huds[i].next = &huds[i + 1];
		if (huds[i].text) {
			free(huds[i].text);
		}
		huds[i].text = NULL;
		if (huds[i].pic) {
			free(huds[i].pic);
		}
		huds[i].pic = NULL;
	}
}

void R_InitHuds(void) {
	huds = (qmb_hud_t *) Hunk_AllocName(MAX_QMB_HUD * sizeof (qmb_hud_t), "qmb_hud");
	R_ClearHuds();
}

extern void Draw_AlphaColourPic(int x, int y, qpic_t *pic, vec3_t colour, float alpha);

void R_DrawQmbHud(void) {
	int i;

	for (i = 0; i < MAX_QMB_HUD; i++) {
		if (huds[i].used = 1) {
			//draw the texture, if their is one
			if (huds[i].pic) {
				Draw_AlphaColourPic(huds[i].x, huds[i].y, NULL, huds[i].colour, huds[i].alpha);
			}

			//draw the text if their is some
			if (huds[i].text) {
				Draw_String(huds[i].x, huds[i].y, huds[i].text);
			}
		}
	}
}

void R_AddTextHudItem(byte id, float x, float y, char *string) {
	//VectorCopy(colour, h->colour);
	huds[id].colour[0] = 1.0;
	huds[id].colour[1] = 1.0;
	huds[id].colour[2] = 1.0;
	huds[id].alpha = 1;

	huds[id].text = string;
	huds[id].used = 1;
}

void R_AddPicHudItem(byte id, float x, float y, float w, float h, vec3_t colour, float alpha, int gl_texnum) {
	glpic_t *p;

	huds[id].x = x;
	huds[id].y = y;
	huds[id].width = w;
	huds[id].height = h;
	VectorCopy(colour, huds[id].colour);
	huds[id].alpha = alpha;

	//allocate memory for pic data
	p = (glpic_t *) malloc(sizeof (glpic_t));
	p->texnum = gl_texnum;
	p->sl = 0;
	p->tl = 0;
	p->sh = 1;
	p->th = 1;

	//not sure why i need to make it a void pointer, as they are the same type of pointer...
	huds[id].pic = p;
	huds[id].used = 1;
}

void R_RemoveHudItem(byte id) {
	huds[id].used = 0;

	if (huds[id].text) {
		free(huds[id].text);
	}
	huds[id].text = NULL;

	if (huds[id].pic) {
		free(huds[id].pic);
	}
	huds[id].pic = NULL;
}

void CL_DecodeHudRemove(void) {
	byte id;

	id = MSG_ReadByte();
	R_RemoveHudItem(id);
}

