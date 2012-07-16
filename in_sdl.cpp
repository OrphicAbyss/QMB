/*
Copyright (C) 1996-1997 Chris Hallson.
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

#include "quakedef.h"
#include "input.h"

static bool mouse_avail;
static float mouse_x, mouse_y;
static int mouse_oldbuttonstate = 0;

void IN_Init(void){
	if (COM_CheckParm("-nomouse"))
		return;
	mouse_x = mouse_y = 0.0;
	mouse_avail = 1;
}

void IN_Shutdown(void){
	mouse_avail = 0;
}

void IN_Commands(void){

}

void IN_Move(usercmd_t *cmd){
	if (!mouse_avail)
		return;

	mouse_x *= sensitivity.getFloat();
	mouse_y *= sensitivity.getFloat();

	if ((in_strafe.state & 1) || (lookstrafe.getBool() && in_mlook.getBool()))
		cmd->sidemove += m_side.getFloat() * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.getFloat() * mouse_x;

	if (in_mlook.getBool())
		V_StopPitchDrift();

	if (in_mlook.getBool() && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] += m_pitch.getFloat() * mouse_y;
		if (cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	} else {
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.getFloat() * mouse_y;
		else
			cmd->forwardmove -= m_forward.getFloat() * mouse_y;
	}
	mouse_x = mouse_y = 0.0;
}

void IN_ClearStates(void){

}

void IN_ShowMouse(void){
	SDL_ShowCursor(SDL_ENABLE);
}

void IN_HideMouse(void){
	SDL_ShowCursor(SDL_DISABLE);
}

void IN_DeactivateMouse(void){

}

void IN_ActivateMouse(void){

}

void IN_RestoreOriginalMouseState(void){

}

void IN_SetQuakeMouseState(void){

}

void IN_MouseEvent	(SDL_Event event){
	if ((event.motion.x != (vid.width / 2)) ||
			(event.motion.y != (vid.height / 2))) {
		mouse_x = event.motion.xrel * 10;
		mouse_y = event.motion.yrel * 10;
		if ((event.motion.x < ((vid.width / 2)-(vid.width / 4))) ||
				(event.motion.x > ((vid.width / 2)+(vid.width / 4))) ||
				(event.motion.y < ((vid.height / 2)-(vid.height / 4))) ||
				(event.motion.y > ((vid.height / 2)+(vid.height / 4)))) {
			SDL_WarpMouse(vid.width / 2, vid.height / 2);
		}
	}
}
