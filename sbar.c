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
// sbar.c -- status bar code

#include "quakedef.h"

#define STAT_MINUS		10	// num frame for '-' stats digit
qpic_t		*sb_nums[2][11];
qpic_t		*sb_colon, *sb_slash;
qpic_t		*sb_ibar;
qpic_t		*sb_sbar;
qpic_t		*sb_scorebar;

qpic_t      *sb_weapons[7][8];   // 0 is active, 1 is owned, 2-5 are flashes
qpic_t      *sb_ammo[4];
qpic_t		*sb_sigil[4];
qpic_t		*sb_armor[3];
qpic_t		*sb_items[32];

qpic_t	*sb_faces[7][2];		// 0 is gibbed, 1 is dead, 2-6 are alive
							// 0 is static, 1 is temporary animation
qpic_t	*sb_face_invis;
qpic_t	*sb_face_quad;
qpic_t	*sb_face_invuln;
qpic_t	*sb_face_invis_invuln;

qboolean	sb_showscores;

int			sb_lines;			// scan lines to draw

qpic_t      *rsb_invbar[2];
qpic_t      *rsb_weapons[5];
qpic_t      *rsb_items[2];
qpic_t      *rsb_ammo[3];
qpic_t      *rsb_teambord;		// PGM 01/19/97 - team color border

//MED 01/04/97 added two more weapons + 3 alternates for grenade launcher
qpic_t      *hsb_weapons[7][5];   // 0 is active, 1 is owned, 2-5 are flashes
//MED 01/04/97 added array to simplify weapon parsing
int         hipweapons[4] = {HIT_LASER_CANNON_BIT,HIT_MJOLNIR_BIT,4,HIT_PROXIMITY_GUN_BIT};
//MED 01/04/97 added hipnotic items array
qpic_t      *hsb_items[2];

void Sbar_MiniDeathmatchOverlay (void);
void Sbar_DeathmatchOverlay (void);
void M_DrawPic (int x, int y, qpic_t *pic);

//JHL:QMB; stuff
void Draw_AlphaFill (int x, int y, int w, int h, vec3_t c, float alpha);
void Draw_AlphaFillFade (int x, int y, int width, int height, vec3_t colour, float alpha[2]);

cvar_t	hud = {"hud","2", true};
cvar_t	hud_r = {"hud_red","255", true};
cvar_t	hud_g = {"hud_green","255", true};
cvar_t	hud_b = {"hud_blue","255", true};
cvar_t	hud_a = {"hud_alpha","178", true};

//#define	SBAR_COLOR_BG	5

/*
===============
Sbar_ShowScores

Tab key down
===============
*/
void Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;
	sb_showscores = true;
}

/*
===============
Sbar_DontShowScores

Tab key up
===============
*/
void Sbar_DontShowScores (void)
{
	sb_showscores = false;
}

/*
===============
Sbar_Init
===============
*/
void Sbar_Init (void)
{
	int		i;

	for (i=0 ; i<10 ; i++)
	{
		sb_nums[0][i] = Draw_PicFromWadXY (va("num_%i",i),32,32);
		sb_nums[1][i] = Draw_PicFromWadXY (va("anum_%i",i),32,32);
	}

	sb_nums[0][10] = Draw_PicFromWadXY ("num_minus",32,32);
	sb_nums[1][10] = Draw_PicFromWadXY ("anum_minus",32,32);

	sb_colon = Draw_PicFromWadXY ("num_colon",32,32);
	sb_slash = Draw_PicFromWadXY ("num_slash",32,32);

	sb_weapons[0][0] = Draw_PicFromWadXY ("inv_shotgun",32,44);
	sb_weapons[0][1] = Draw_PicFromWadXY ("inv_sshotgun",32,44);
	sb_weapons[0][2] = Draw_PicFromWadXY ("inv_nailgun",32,44);
	sb_weapons[0][3] = Draw_PicFromWadXY ("inv_snailgun",32,44);
	sb_weapons[0][4] = Draw_PicFromWadXY ("inv_rlaunch",32,44);
	sb_weapons[0][5] = Draw_PicFromWadXY ("inv_srlaunch",32,44);
	sb_weapons[0][6] = Draw_PicFromWadXY ("inv_lightng",32,44);

	sb_weapons[1][0] = Draw_PicFromWadXY ("inv2_shotgun",32,44);
	sb_weapons[1][1] = Draw_PicFromWadXY ("inv2_sshotgun",32,44);
	sb_weapons[1][2] = Draw_PicFromWadXY ("inv2_nailgun",32,44);
	sb_weapons[1][3] = Draw_PicFromWadXY ("inv2_snailgun",32,44);
	sb_weapons[1][4] = Draw_PicFromWadXY ("inv2_rlaunch",32,44);
	sb_weapons[1][5] = Draw_PicFromWadXY ("inv2_srlaunch",32,44);
	sb_weapons[1][6] = Draw_PicFromWadXY ("inv2_lightng",32,44);

	for (i=0 ; i<5 ; i++)
	{
		sb_weapons[2+i][0] = Draw_PicFromWadXY (va("inva%i_shotgun",i+1),32,44);
		sb_weapons[2+i][1] = Draw_PicFromWadXY (va("inva%i_sshotgun",i+1),32,44);
		sb_weapons[2+i][2] = Draw_PicFromWadXY (va("inva%i_nailgun",i+1),32,44);
		sb_weapons[2+i][3] = Draw_PicFromWadXY (va("inva%i_snailgun",i+1),32,44);
		sb_weapons[2+i][4] = Draw_PicFromWadXY (va("inva%i_rlaunch",i+1),32,44);
		sb_weapons[2+i][5] = Draw_PicFromWadXY (va("inva%i_srlaunch",i+1),32,44);
		sb_weapons[2+i][6] = Draw_PicFromWadXY (va("inva%i_lightng",i+1),32,44);
	}

	sb_ammo[0] = Draw_PicFromWadXY ("sb_shells",48,48);
	sb_ammo[1] = Draw_PicFromWadXY ("sb_nails",48,48);
	sb_ammo[2] = Draw_PicFromWadXY ("sb_rocket",48,48);
	sb_ammo[3] = Draw_PicFromWadXY ("sb_cells",48,48);

	sb_armor[0] = Draw_PicFromWadXY ("sb_armor1",48,48);
	sb_armor[1] = Draw_PicFromWadXY ("sb_armor2",48,48);
	sb_armor[2] = Draw_PicFromWadXY ("sb_armor3",48,48);

	sb_items[0] = Draw_PicFromWadXY ("sb_key1",32,32);
	sb_items[1] = Draw_PicFromWadXY ("sb_key2",32,32);
	sb_items[2] = Draw_PicFromWadXY ("sb_invis",32,32);
	sb_items[3] = Draw_PicFromWadXY ("sb_invuln",32,32);
	sb_items[4] = Draw_PicFromWadXY ("sb_suit",32,32);
	sb_items[5] = Draw_PicFromWadXY ("sb_quad",32,32);

	sb_sigil[0] = Draw_PicFromWadXY ("sb_sigil1",32,32);
	sb_sigil[1] = Draw_PicFromWadXY ("sb_sigil2",32,32);
	sb_sigil[2] = Draw_PicFromWadXY ("sb_sigil3",32,32);
	sb_sigil[3] = Draw_PicFromWadXY ("sb_sigil4",32,32);

	sb_faces[4][0] = Draw_PicFromWadXY ("face1",48,48);
	sb_faces[4][1] = Draw_PicFromWadXY ("face_p1",48,48);
	sb_faces[3][0] = Draw_PicFromWadXY ("face2",48,48);
	sb_faces[3][1] = Draw_PicFromWadXY ("face_p2",48,48);
	sb_faces[2][0] = Draw_PicFromWadXY ("face3",48,48);
	sb_faces[2][1] = Draw_PicFromWadXY ("face_p3",48,48);
	sb_faces[1][0] = Draw_PicFromWadXY ("face4",48,48);
	sb_faces[1][1] = Draw_PicFromWadXY ("face_p4",48,48);
	sb_faces[0][0] = Draw_PicFromWadXY ("face5",48,48);
	sb_faces[0][1] = Draw_PicFromWadXY ("face_p5",48,48);

	sb_face_invis = Draw_PicFromWadXY ("face_invis",48,48);
	sb_face_invuln = Draw_PicFromWadXY ("face_invul2",48,48);
	sb_face_invis_invuln = Draw_PicFromWadXY ("face_inv2",48,48);
	sb_face_quad = Draw_PicFromWadXY ("face_quad",48,48);

	Cmd_AddCommand ("+showscores", Sbar_ShowScores);
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores);

  //JHL; load from flat lump file
	sb_sbar = Draw_CachePic ("gfx/qmb_hud_sbar_slot.lmp");
	sb_ibar = Draw_PicFromWad ("ibar");
	sb_scorebar = Draw_PicFromWad ("scorebar");

//MED 01/04/97 added new hipnotic weapons
	if (hipnotic)
	{
	  hsb_weapons[0][0] = Draw_PicFromWad ("inv_laser");
	  hsb_weapons[0][1] = Draw_PicFromWad ("inv_mjolnir");
	  hsb_weapons[0][2] = Draw_PicFromWad ("inv_gren_prox");
	  hsb_weapons[0][3] = Draw_PicFromWad ("inv_prox_gren");
	  hsb_weapons[0][4] = Draw_PicFromWad ("inv_prox");

	  hsb_weapons[1][0] = Draw_PicFromWad ("inv2_laser");
	  hsb_weapons[1][1] = Draw_PicFromWad ("inv2_mjolnir");
	  hsb_weapons[1][2] = Draw_PicFromWad ("inv2_gren_prox");
	  hsb_weapons[1][3] = Draw_PicFromWad ("inv2_prox_gren");
	  hsb_weapons[1][4] = Draw_PicFromWad ("inv2_prox");

	  for (i=0 ; i<5 ; i++)
	  {
		 hsb_weapons[2+i][0] = Draw_PicFromWad (va("inva%i_laser",i+1));
		 hsb_weapons[2+i][1] = Draw_PicFromWad (va("inva%i_mjolnir",i+1));
		 hsb_weapons[2+i][2] = Draw_PicFromWad (va("inva%i_gren_prox",i+1));
		 hsb_weapons[2+i][3] = Draw_PicFromWad (va("inva%i_prox_gren",i+1));
		 hsb_weapons[2+i][4] = Draw_PicFromWad (va("inva%i_prox",i+1));
	  }

	  hsb_items[0] = Draw_PicFromWad ("sb_wsuit");
	  hsb_items[1] = Draw_PicFromWad ("sb_eshld");
	}

	if (rogue)
	{
		rsb_invbar[0] = Draw_PicFromWad ("r_invbar1");
		rsb_invbar[1] = Draw_PicFromWad ("r_invbar2");

		rsb_weapons[0] = Draw_PicFromWad ("r_lava");
		rsb_weapons[1] = Draw_PicFromWad ("r_superlava");
		rsb_weapons[2] = Draw_PicFromWad ("r_gren");
		rsb_weapons[3] = Draw_PicFromWad ("r_multirock");
		rsb_weapons[4] = Draw_PicFromWad ("r_plasma");

		rsb_items[0] = Draw_PicFromWad ("r_shield1");
        rsb_items[1] = Draw_PicFromWad ("r_agrav1");

// PGM 01/19/97 - team color border
        rsb_teambord = Draw_PicFromWad ("r_teambord");
// PGM 01/19/97 - team color border

		rsb_ammo[0] = Draw_PicFromWad ("r_ammolava");
		rsb_ammo[1] = Draw_PicFromWad ("r_ammomulti");
		rsb_ammo[2] = Draw_PicFromWad ("r_ammoplasma");
	}
	//JHL:QMB; the hud size cvar...
	Cvar_RegisterVariable (&hud);
	Cvar_RegisterVariable (&hud_r);
	Cvar_RegisterVariable (&hud_g);
	Cvar_RegisterVariable (&hud_b);
	Cvar_RegisterVariable (&hud_a);
}


//=============================================================================

// drawing routines are relative to the status bar location

/*
=============
Sbar_DrawPic
=============
*/
void Sbar_DrawPic (int x, int y, qpic_t *pic)
{
	//JHL; Completely recreated HUD, no need for separate drawing styles
	Draw_AlphaPic (x, y + (vid.height-SBAR_HEIGHT), pic, 1);
}

/*
=============
Sbar_DrawTransPic
=============
*/
void Sbar_DrawTransPic (int x, int y, qpic_t *pic)
{
	//JHL; Completely recreated HUD, no need for separate drawing styles
	Draw_TransPic (x, y + (vid.height-SBAR_HEIGHT), pic);
}

/*
================
Sbar_DrawCharacter

Draws one solid graphics character
================
*/
void Sbar_DrawCharacter (int x, int y, int num)
{
	//JHL; Completely recreated HUD, no need for separate drawing styles
	Draw_Character ( x + 4 , y + vid.height-SBAR_HEIGHT, num);
}

/*
================
Sbar_DrawString
================
*/
void Sbar_DrawString (int x, int y, char *str)
{
	//JHL; Completely recreated HUD, no need for separate drawing styles
	Draw_String (x, y+ vid.height-SBAR_HEIGHT, str);
}

/*
=============
Sbar_itoa
=============
*/
int Sbar_itoa (int num, char *buf)
{
	char	*str;
	int		pow10;
	int		dig;

	str = buf;

	if (num < 0)
	{
		*str++ = '-';
		num = -num;
	}

	for (pow10 = 10 ; num >= pow10 ; pow10 *= 10)
	;

	do
	{
		pow10 /= 10;
		dig = num/pow10;
		*str++ = '0'+dig;
		num -= dig*pow10;
	} while (pow10 != 1);

	*str = 0;

	return str-buf;
}

/*
=============
Sbar_DrawFade

JHL;ADD; fade out the sbar bg
=============
*/
void Sbar_DrawFade (int x, int y, int height, int side)
{
	float	alpha[2];
	vec3_t	colour;

	colour[0] = hud_r.value;
	colour[1] = hud_g.value;
	colour[2] = hud_b.value;

	if (side)
	{
		alpha[0] = 0;
		alpha[1] = hud_a.value;
		Draw_AlphaFillFade (x, y, 10, height, colour, alpha);
	}
	else
	{
		alpha[0] = hud_a.value;
		alpha[1] = 0;
		Draw_AlphaFillFade (x-10, y, 10, height, colour, alpha);
	}
}

/*
=============
Sbar_DrawNum
=============
*/
void Sbar_DrawNum (int x, int y, int num, int digits, int color)
{
	char			str[12];
	char			*ptr;
	int				l, frame;
	vec3_t	colour;

	colour[0] = hud_r.value;
	colour[1] = hud_g.value;
	colour[2] = hud_b.value;

	//Sbar_DrawFade (x, y+vid.height-24, 24, false);
	//Draw_AlphaFill(x, y+vid.height-24, 24*digits, 24, colour, hud_a.value);
	//Sbar_DrawFade (x+(24*digits), y+vid.height-24, 24, true);

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l)*32;

	while (*ptr)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		Sbar_DrawTransPic (x,y,sb_nums[color][frame]);
		x += 32;
		ptr++;
	}
}

//=============================================================================

int		fragsort[MAX_SCOREBOARD];

char	scoreboardtext[MAX_SCOREBOARD][20];
int		scoreboardtop[MAX_SCOREBOARD];
int		scoreboardbottom[MAX_SCOREBOARD];
int		scoreboardcount[MAX_SCOREBOARD];
int		scoreboardlines;

/*
===============
Sbar_SortFrags
===============
*/
void Sbar_SortFrags (void)
{
	int		i, j, k;

// sort by frags
	scoreboardlines = 0;
	for (i=0 ; i<cl.maxclients ; i++)
	{
		if (cl.scores[i].name[0])
		{
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i=0 ; i<scoreboardlines ; i++)
		for (j=0 ; j<scoreboardlines-1-i ; j++)
			if (cl.scores[fragsort[j]].frags < cl.scores[fragsort[j+1]].frags)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j+1];
				fragsort[j+1] = k;
			}
}

int	Sbar_ColorForMap (int m)
{
	return m < 128 ? m + 8 : m + 8;
}

/*
===============
Sbar_UpdateScoreboard
===============
*/
void Sbar_UpdateScoreboard (void)
{
	int		i, k;
	int		top, bottom;
	scoreboard_t	*s;

	Sbar_SortFrags ();

// draw the text
	Q_memset (scoreboardtext, 0, sizeof(scoreboardtext));

	for (i=0 ; i<scoreboardlines; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		sprintf (&scoreboardtext[i][1], "%3i %s", s->frags, s->name);

		top = s->colors & 0xf0;
		bottom = (s->colors & 15) <<4;
		scoreboardtop[i] = Sbar_ColorForMap (top);
		scoreboardbottom[i] = Sbar_ColorForMap (bottom);
	}
}



/*
===============
Sbar_SoloScoreboard
===============
*/
void Sbar_SoloScoreboard (void)
{
	char	str[80];
	int		minutes, seconds, tens, units;
	int		l, x;
	vec3_t	colour;

	colour[0] = hud_r.value;
	colour[1] = hud_g.value;
	colour[2] = hud_b.value;

	x = vid.width/2-160;

	Sbar_DrawFade (x, vid.height-24, 24, false);
	Draw_AlphaFill(x, vid.height-24, 320, 24, colour, hud_a.value);
	Sbar_DrawFade (x+320, vid.height-24, 24, true);

	sprintf (str,"Monsters:%3i /%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	Sbar_DrawString (x+8, 4+24, str);

	sprintf (str,"Secrets :%3i /%3i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
	Sbar_DrawString (x+8, 12+24, str);

// time
	minutes = cl.time / 60;
	seconds = cl.time - 60*minutes;
	tens = seconds / 10;
	units = seconds - 10*tens;
	sprintf (str,"Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (x+184, 4+24, str);

// draw level name
	l = Q_strlen (cl.levelname);
	Sbar_DrawString (x + 232 - l*4, 12+24, cl.levelname);
}

/*
===============
Sbar_DrawScoreboard
===============
*/
void Sbar_DrawScoreboard (void)
{
	Sbar_SoloScoreboard ();
	if (cl.gametype == GAME_DEATHMATCH)
		Sbar_DeathmatchOverlay ();
}

//=============================================================================

/*
===============
Sbar_DrawInventory
===============
*/
void Sbar_DrawInventory (void)
{
	int		i;
	char	num[6];
	float	time;
	int		flashon;
	vec3_t	colour;

	colour[0] = hud_r.value;
	colour[1] = hud_g.value;
	colour[2] = hud_b.value;

	if (rogue)
	{
		if ( cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
			Sbar_DrawPic (0, -24, rsb_invbar[0]);
		else
			Sbar_DrawPic (0, -24, rsb_invbar[1]);
	}

// weapons
	if (hud.value >= 3 || sb_showscores) { //JHL - start

		//Sbar_DrawFade (vid.width-50, vid.height/2 - 24, 112, false);
		//Draw_AlphaFill(vid.width-50, vid.height/2 - 24, 48, 112, colour, hud_a.value);

		for (i=0 ; i<7 ; i++)
		{
			if (cl.items & (IT_SHOTGUN<<i) )
			{
				time = cl.item_gettime[i];
				flashon = (int)((cl.time - time)*10);
				if (flashon >= 10)
				{
					if ( cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN<<i)  )
						flashon = 1;
					else
						flashon = 0;
				}
				else
					flashon = (flashon%5) + 2;

				Sbar_DrawPic (vid.width-sb_weapons[flashon][i]->width, ((vid.height/4*3)*(-1))+i*24, sb_weapons[flashon][i]);
			}
		}

		// MED 01/04/97
		// hipnotic weapons
		if (hipnotic)
		{
		  int grenadeflashing=0;
		  for (i=0 ; i<4 ; i++)
		  {
			 if (cl.items & (1<<hipweapons[i]) )
			 {
				time = cl.item_gettime[hipweapons[i]];
				flashon = (int)((cl.time - time)*10);
				if (flashon >= 10)
				{
				   if ( cl.stats[STAT_ACTIVEWEAPON] == (1<<hipweapons[i])  )
					  flashon = 1;
				   else
					  flashon = 0;
				}
				else
				   flashon = (flashon%5) + 2;

				// check grenade launcher
				if (i==2)
				{
				   if (cl.items & HIT_PROXIMITY_GUN)
				   {
					  if (flashon)
					  {
						 grenadeflashing = 1;
						 Sbar_DrawPic (96, -16, hsb_weapons[flashon][2]);
					  }
				   }
				}
				else if (i==3)
				{
				   if (cl.items & (IT_SHOTGUN<<4))
				   {
					  if (flashon && !grenadeflashing)
					  {
						 Sbar_DrawPic (96, -16, hsb_weapons[flashon][3]);
					  }
					  else if (!grenadeflashing)
					  {
						 Sbar_DrawPic (96, -16, hsb_weapons[0][3]);
					  }
				   }
				   else
					  Sbar_DrawPic (96, -16, hsb_weapons[flashon][4]);
				}
				else
				   Sbar_DrawPic (176 + (i*24), -16, hsb_weapons[flashon][i]);
			 }
		  }
		}

		if (rogue)
		{
		// check for powered up weapon.
			if ( cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
			{
				for (i=0;i<5;i++)
				{
					if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i))
					{
						Sbar_DrawPic ((i+2)*24, -16, rsb_weapons[i]);
					}
				}
			}
		}

		// ammo counts
		for (i=0 ; i<4 ; i++)
		{
			sprintf (num, "%3i",cl.stats[STAT_SHELLS+i] );
			if (num[0] != ' ')
				Sbar_DrawCharacter ( vid.width-58, ((vid.height/4*3)*(-1))+i*48+24, 18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter ( vid.width-52, ((vid.height/4*3)*(-1))+i*48+24, 18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter ( vid.width-44, ((vid.height/4*3)*(-1))+i*48+24, 18 + num[2] - '0');
		}
	} //JHL - end

	//JHL - start
	if (hud.value >= 2 || sb_showscores) {

		//Sbar_DrawFade (vid.width-96, vid.height-40, 16, false);
		//Draw_AlphaFill(vid.width-96, vid.height-40, 96, 16, colour, hud_a.value);

		flashon = 0;
		// items
		for (i=0 ; i<6 ; i++)
		  if (cl.items & (1<<(17+i)))
		  {
			 time = cl.item_gettime[17+i];
			 if (time && time > cl.time - 2 && flashon )
			 {  // flash frame

			 } else {
			 //MED 01/04/97 changed keys
				if (!hipnotic || (i>1)){
				   Sbar_DrawPic (vid.width-192+i*32, -32, sb_items[i]);
				}
			 }
		  }
		//MED 01/04/97 added hipnotic items
		// hipnotic items
		if (hipnotic)
		{
		  for (i=0 ; i<2 ; i++)
			 if (cl.items & (1<<(24+i)))
			 {
				time = cl.item_gettime[24+i];
				if (time && time > cl.time - 2 && flashon )
				{  // flash frame

				} else
				   Sbar_DrawPic (288 + i*16, -16, hsb_items[i]);
			 }
		}

		if (rogue)
		{
		// new rogue items
			for (i=0 ; i<2 ; i++)
			{
				if (cl.items & (1<<(29+i)))
				{
					time = cl.item_gettime[29+i];

					if (time &&	time > cl.time - 2 && flashon )
					{	// flash frame
					} else
						Sbar_DrawPic (288 + i*16, -16, rsb_items[i]);
				}
			}
		}
		else
		{
		// sigils
			for (i=0 ; i<4 ; i++)
			{
				if (cl.items & (1<<(28+i)))
				{
					time = cl.item_gettime[28+i];
					if (time &&	time > cl.time - 2 && flashon )
					{	// flash frame
					} else
						Sbar_DrawPic (vid.width-128 + i*32, -SBAR_HEIGHT, sb_sigil[i]);
				}
			}
		}
	} //JHL - end
}

//=============================================================================

/*
===============
Sbar_DrawFrags
===============
*/
void Sbar_DrawFrags (void)
{
	int				i, k, l;
	int				top, bottom;
	int				x, y, f;
	int				xofs;
	char			num[12];
	scoreboard_t	*s;

	Sbar_SortFrags ();

// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 23;
	//JHL; Completely recreated HUD, no need for separate drawing styles
	xofs = 0;

	y = vid.height - SBAR_HEIGHT - 23;

	for (i=0 ; i<l ; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

	// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill (xofs + x*8 + 10, y, 28, 4, top);
		Draw_Fill (xofs + x*8 + 10, y+4, 28, 3, bottom);

	// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		Sbar_DrawCharacter ( (x+1)*8 , -24, num[0]);
		Sbar_DrawCharacter ( (x+2)*8 , -24, num[1]);
		Sbar_DrawCharacter ( (x+3)*8 , -24, num[2]);

		if (k == cl.viewentity - 1)
		{
			Sbar_DrawCharacter (x*8+2, -24, 16);
			Sbar_DrawCharacter ( (x+4)*8-4, -24, 17);
		}
		x+=4;
	}
}

//=============================================================================


/*
===============
Sbar_DrawFace
===============
*/
void Sbar_DrawFace (int x, int y)
{
	int		f, anim;

// PGM 01/19/97 - team color drawing
// PGM 03/02/97 - fixed so color swatch only appears in CTF modes
	if (rogue &&
        (cl.maxclients != 1) &&
        (teamplay.value>3) &&
        (teamplay.value<7))
	{
		int				top, bottom;
		int				xofs;
		char			num[12];
		scoreboard_t	*s;

		s = &cl.scores[cl.viewentity - 1];
		// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		//JHL; Completely recreated HUD, no need for separate drawing styles
		xofs = 113;

		Sbar_DrawPic (112, 0, rsb_teambord);
		Draw_Fill (xofs, vid.height-SBAR_HEIGHT+3, 22, 9, top);
		Draw_Fill (xofs, vid.height-SBAR_HEIGHT+12, 22, 9, bottom);

		// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		if (top==8)
		{
			if (num[0] != ' ')
				Sbar_DrawCharacter(109, 3, 18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter(116, 3, 18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter(123, 3, 18 + num[2] - '0');
		}
		else
		{
			Sbar_DrawCharacter ( 109, 3, num[0]);
			Sbar_DrawCharacter ( 116, 3, num[1]);
			Sbar_DrawCharacter ( 123, 3, num[2]);
		}

		return;
	}
// PGM 01/19/97 - team color drawing

	if ( (cl.items & (IT_INVISIBILITY | IT_INVULNERABILITY) )
	== (IT_INVISIBILITY | IT_INVULNERABILITY) )
	{
		Sbar_DrawPic (x, y, sb_face_invis_invuln);
		return;
	}
	if (cl.items & IT_QUAD)
	{
		Sbar_DrawPic (x, y, sb_face_quad );
		return;
	}
	if (cl.items & IT_INVISIBILITY)
	{
		Sbar_DrawPic (x, y, sb_face_invis );
		return;
	}
	if (cl.items & IT_INVULNERABILITY)
	{
		Sbar_DrawPic (x, y, sb_face_invuln);
		return;
	}

	if (cl.stats[STAT_HEALTH] >= 100)
		f = 4;
	else
		f = cl.stats[STAT_HEALTH] / 20;

	if (cl.time <= cl.faceanimtime)
		anim = 1;
	else
		anim = 0;

	Sbar_DrawPic (x, y, sb_faces[f][anim]);
}

/*
===============
Sbar_Draw
===============
*/
void Sbar_Draw (void)
{
	if (scr_con_current == vid.height)
		return;		// console is full screen

	scr_copyeverything = 1;

	if (sb_lines && vid.width > 320)
		Draw_TileClear (0, vid.height - sb_lines, vid.width, sb_lines);

	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
	{
		Sbar_DrawScoreboard ();
	}

	if ((hud.value || sb_showscores) && cl.stats[STAT_HEALTH] > 0)
	{
   // keys (hipnotic only)
      //MED 01/04/97 moved keys here so they would not be overwritten
      if (hipnotic)
      {
         if (cl.items & IT_KEY1)
            Sbar_DrawPic (209, 3, sb_items[0]);
         if (cl.items & IT_KEY2)
            Sbar_DrawPic (209, 12, sb_items[1]);
      }
   // armor
		if (cl.items & IT_INVULNERABILITY)
		{
			Sbar_DrawNum (SBAR_HEIGHT, -SBAR_HEIGHT+16, 666, 3, 1);
			Sbar_DrawPic (0, -SBAR_HEIGHT, draw_disc);
		}
		else
		{
			if (rogue)
			{
				Sbar_DrawNum (24, -SBAR_HEIGHT, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
				if (cl.items & RIT_ARMOR3)
					Sbar_DrawPic (0, -SBAR_HEIGHT, sb_armor[2]);
				else if (cl.items & RIT_ARMOR2)
					Sbar_DrawPic (0, -SBAR_HEIGHT, sb_armor[1]);
				else if (cl.items & RIT_ARMOR1)
					Sbar_DrawPic (0, -SBAR_HEIGHT, sb_armor[0]);
			}
			else
			{
				//if (cl.stats[STAT_ARMOR] > 0)
				{
					Sbar_DrawNum (SBAR_HEIGHT, -32, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
					if (cl.items & IT_ARMOR3)
						Sbar_DrawPic (0, -SBAR_HEIGHT, sb_armor[2]);
					else if (cl.items & IT_ARMOR2)
						Sbar_DrawPic (0, -SBAR_HEIGHT, sb_armor[1]);
					else if (cl.items & IT_ARMOR1)
						Sbar_DrawPic (0, -SBAR_HEIGHT, sb_armor[0]);
					//else
						//Sbar_DrawPic (0, -SBAR_HEIGHT, sb_sbar);
				}
			}
		}

	// health
		Sbar_DrawNum (SBAR_HEIGHT, 16, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

	// face
		Sbar_DrawFace (0, 0);

		if (hud.value >= 2 || sb_showscores)
		{
	// ammo count
			Sbar_DrawNum (vid.width-48-32-32-32, 16, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);

	// ammo icon
			if (rogue)
			{
				if (cl.items & RIT_SHELLS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, sb_ammo[0]);
				else if (cl.items & RIT_NAILS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, sb_ammo[1]);
				else if (cl.items & RIT_ROCKETS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, sb_ammo[2]);
				else if (cl.items & RIT_CELLS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, sb_ammo[3]);
				else if (cl.items & RIT_LAVA_NAILS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, rsb_ammo[0]);
				else if (cl.items & RIT_PLASMA_AMMO)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, rsb_ammo[1]);
				else if (cl.items & RIT_MULTI_ROCKETS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, rsb_ammo[2]);
			}
			else
			{
				if (cl.items & IT_SHELLS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, sb_ammo[0]);
				else if (cl.items & IT_NAILS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, sb_ammo[1]);
				else if (cl.items & IT_ROCKETS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, sb_ammo[2]);
				else if (cl.items & IT_CELLS)
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, sb_ammo[3]);
				else
					Sbar_DrawPic (vid.width-SBAR_HEIGHT, 0, sb_sbar);
			}
		}

	// draw inventory
		Sbar_DrawInventory ();

		//JHL; QMB is OpenGL only, so we can assume we are always in hi-res
		if (cl.gametype == GAME_DEATHMATCH && !sb_showscores)
			Sbar_MiniDeathmatchOverlay ();
	}
}

//=============================================================================

/*
==================
Sbar_IntermissionNumber

==================
*/
void Sbar_IntermissionNumber (int x, int y, int num, int digits, int color)
{
	char			str[12];
	char			*ptr;
	int				l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l)*24;

	while (*ptr)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		Draw_TransPic (x,y,sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

/*
==================
Sbar_DeathmatchOverlay

==================
*/
void Sbar_DeathmatchOverlay (void)
{
	qpic_t			*pic;
	int				i, k, l;
	int				top, bottom;
	int				x, y, f;
	char			num[12];
	scoreboard_t	*s;

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	pic = Draw_CachePic ("gfx/ranking.lmp");
	Draw_AlphaPic ((vid.width-pic->width)/2, 8, pic, 1);

// scores
	Sbar_SortFrags ();

// draw the text
	l = scoreboardlines;

	x = 80 + ((vid.width - 320)>>1);
	y = 40;
	for (i=0 ; i<l ; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

	// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill ( x, y, 40, 4, top);
		Draw_Fill ( x, y+4, 40, 4, bottom);

	// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		Draw_Character ( x+8 , y, num[0]);
		Draw_Character ( x+16 , y, num[1]);
		Draw_Character ( x+24 , y, num[2]);

		if (k == cl.viewentity - 1)
		{
			Draw_Character ( x, y, 16);
			Draw_Character ( x + 32, y, 17);
		}

	// draw name
		Draw_String (x+64, y, s->name);

		y += 10;
	}
}

/*
==================
Sbar_MiniDeathmatchOverlay

==================
*/
void Sbar_MiniDeathmatchOverlay (void)
{
	int				i, k, l;
	int				top, bottom;
	int				x, y, f;
	char			num[12];
	scoreboard_t	*s;

// scores
	Sbar_SortFrags ();

// draw the text
	l = scoreboardlines;

	x = 0;
	y = vid.height/3;
	for (i=0 ; i<l ; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

	// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill ( x, y, 40, 4, top);
		Draw_Fill ( x, y+4, 40, 4, bottom);

	// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		Draw_Character ( x+8 , y, num[0]);
		Draw_Character ( x+16 , y, num[1]);
		Draw_Character ( x+24 , y, num[2]);

		if (k == cl.viewentity - 1)
		{
			Draw_Character ( x, y, 16);
			Draw_Character ( x + 32, y, 17);
		}

	// draw name
		Draw_String (x+48, y, s->name);

		y += 10;
	}
}

/*
==================
Sbar_IntermissionOverlay

==================
*/
void Sbar_IntermissionOverlay (void)
{
	qpic_t	*pic;
	int		dig;
	int		num;

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	if (cl.gametype == GAME_DEATHMATCH)
	{
		Sbar_DeathmatchOverlay ();
		return;
	}

	pic = Draw_CachePic ("gfx/complete.lmp");
	Draw_AlphaPic (64, 24, pic, 1);

	pic = Draw_CachePic ("gfx/inter.lmp");
	Draw_TransPic (0, 56, pic);

// time
	dig = cl.completed_time/60;
	Sbar_IntermissionNumber (160, 64, dig, 3, 0);
	num = cl.completed_time - dig*60;
	Draw_TransPic (234,64,sb_colon);
	Draw_TransPic (246,64,sb_nums[0][num/10]);
	Draw_TransPic (266,64,sb_nums[0][num%10]);

	Sbar_IntermissionNumber (160, 104, cl.stats[STAT_SECRETS], 3, 0);
	Draw_TransPic (232,104,sb_slash);
	Sbar_IntermissionNumber (240, 104, cl.stats[STAT_TOTALSECRETS], 3, 0);

	Sbar_IntermissionNumber (160, 144, cl.stats[STAT_MONSTERS], 3, 0);
	Draw_TransPic (232,144,sb_slash);
	Sbar_IntermissionNumber (240, 144, cl.stats[STAT_TOTALMONSTERS], 3, 0);

}


/*
==================
Sbar_FinaleOverlay

==================
*/
void Sbar_FinaleOverlay (void)
{
	qpic_t	*pic;

	scr_copyeverything = 1;

	pic = Draw_CachePic ("gfx/finale.lmp");
	Draw_TransPic ( (vid.width-pic->width)/2, 16, pic);
}
