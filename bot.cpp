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
//#include "java_vm.h"

#include "quakedef.h"
#include "bot.h"

extern void AddTrail(vec3_t start, vec3_t end, int type, float time, float size, vec3_t dir);
extern vec3_t zerodir;

globot_t globot; // This struct is used to store global stuff that aint client specific

//take an angle delta and the angle delta of last frame and blend them together with some extra noise

float aimangle(float angledelta, float lastangledelta, float blend) {
	float beta;

	beta = 1.0f + (Random()*2.0f - 1.0f)*0.1f;
	return (angledelta * beta) * blend + lastangledelta * (1 - blend);
}

/**
 * This function is used to cycle thro all avaliable clients
 * (this is just a recoded PF_nextent)
 */
edict_t *Nextent(edict_t *edict) {
	int e;
	edict_t *ent;

	e = NUM_FOR_EDICT(edict); // Get the edictnum

	while (true) { // Loop until we get a return
		e++; // Increase e with 1

		if (e == sv.num_edicts) // If gone through all edict's
			return sv.edicts; // then return

		ent = EDICT_NUM(e); // Get the edict from the new edictnum

		if (!ent->free) // If it exists
			return ent; // then return it
	}
}

/**
 * Called by bots program code.
 * The move will be adjusted for slopes and stairs, but if the move isn't
 * possible, no move is done, false is returned, and
 * pr_global_struct->trace_normal is set to the normal of the blocking wall
 */
bool bot_movestep(edict_t *ent, vec3_t move) {
	//	float		dz;
	vec3_t oldorg, neworg, end;
	trace_t trace;
	//	int			i;
	//	edict_t		*enemy;

	// try the move

	//get the bots org
	VectorCopy(ent->v.origin, oldorg);
	//work out new position
	VectorAdd(ent->v.origin, move, neworg);

	// push down from a step height above the wished position to a step below
	neworg[2] += sv_stepheight.getFloat();
	VectorCopy(neworg, end);
	end[2] -= sv_stepheight.getFloat()*2;

	//trace down to check if there was a floor
	trace = SV_Move(neworg, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);

	if (trace.allsolid) {
		//inside a wall?
		//Con_Printf("Trace all solid...\n");
		return false;
	}

	if (trace.startsolid) {
		//start inside a wall?
		neworg[2] -= sv_stepheight.getFloat();
		trace = SV_Move(neworg, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
		if (trace.allsolid || trace.startsolid) {
			//Con_Printf("Trace start solid...\n");
			return false;
		}
	}
	if (trace.fraction == 1) {
		// if monster had the ground pulled out, go ahead and fall
		if ((int) ent->v.flags & FL_PARTIALGROUND) {
			//			ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;
			//	Con_Printf ("fall down\n");
			return true;
		}

		//Con_Printf("Walked off edge...\n");
		return false; // walked off an edge
	}

	// check point traces down for dangling corners
	//	VectorCopy (trace.endpos, ent->v.origin);

	/*	if (!SV_CheckBottom (ent))
		{
			if ( (int)ent->v.flags & FL_PARTIALGROUND )
			{	// entity had floor mostly pulled out from underneath it
				// and is trying to correct
				return true;
			}
			//VectorCopy (oldorg, ent->v.origin);
			Con_Printf("No floor?...\n");
			return false;
		}

		if ( (int)ent->v.flags & FL_PARTIALGROUND )
		{
	//		Con_Printf ("back on ground\n");
			//ent->v.flags = (int)ent->v.flags & ~FL_PARTIALGROUND;
		}
	//	ent->v.groundentity = EDICT_TO_PROG(trace.ent);
	 */
	// the move is ok
	return true;
}

/**
 * float(float yaw, float dist) walkmove
 * returns true if it can move
 */
bool bot_walkmove(edict_t *ent, float yaw, float dist) {
	vec3_t move;

	if (!((int) ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
		return false;

	yaw = yaw * M_PI / 180;

	move[0] = cos(yaw) * dist;
	move[1] = sin(yaw) * dist;
	move[2] = 0;

	return bot_movestep(ent, move);
}

/**
 * This function is used to decide if a bot can see his enemy or not
 * (this is just a recoded PF_traceline)
 */
bool Traceline(vec3_t start, vec3_t end, edict_t *self, edict_t *enemy) {
	trace_t trace = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, self);

	if (trace.fraction >= 0.999f) // If there were no walls
	{
		//AddTrail(start,trace.endpos,3,0.05f,1.5f,zerodir);

		trace = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, self);
		if (trace.ent == enemy) // And there were no other entities in the way
			return true; // Then the bot can see him
	}

	return false; // Otherwise he cant
}

/**
 * This function is used to decide if a bot can see his enemy or not
 * (this is just a recoded PF_traceline)
 */
float BotTraceline(vec3_t start, vec3_t end, edict_t *self) {
	trace_t trace = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, self);

	return trace.fraction;
}

/**
 * This function is used to decide if the bot should turn around or not
 * (this is just a recoded PF_vectoangles)
 */
void CalcAngles(vec3_t oldvector, vec3_t newvector) {
	float forward;
	float yaw, pitch;

	if (oldvector[1] == 0 && oldvector[0] == 0) {
		yaw = 0;
		if (oldvector[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	} else {
		yaw = (int) (atan2(oldvector[1], oldvector[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrt(oldvector[0] * oldvector[0] + oldvector[1] * oldvector[1]);
		pitch = (int) (atan2(oldvector[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	newvector[0] = pitch;
	newvector[1] = yaw;
	newvector[2] = 0;
}

/**
 * This function is used to calcualte if an entity can be seen.
 * Returns the angle in test and true if the entity can be seen, else it returns false
 */
bool IsInView(vec3_t eyes1, vec3_t eyes2, edict_t *bot, edict_t *nmy, int fov, vec3_t test) {
	vec3_t origin;
	float test2;

	if (Traceline(eyes1, eyes2, bot, nmy)) // If the bot can see his enemy
	{
		VectorSubtract(eyes1, eyes2, origin); // Get a nice vector
		CalcAngles(origin, test); // And use it to see in what direction the enemy is
		test2 = test[1] - bot->v.angles[1]; // Another shortcut

		return (test2 > -fov && test2 < fov); // If enemy is in front of the bot so he can see him
	}
	return false;
}

/**
 * This is the function where the bot checks what ammo and weapons he has...
 * And switches to the best weapon avaliable...
 */
void SwitchWeapon(edict_t *bot) {
	int items = (int) bot->v.items;
	int weapon = (int) bot->v.weapon;

	//qmb :fixed bot changing weapons between best 2 weapons.
	if (bot->v.ammo_rockets >= 1 && // If the bot has some rockets
			items & IT_ROCKET_LAUNCHER) // and the Rocket Launcher
	{
		if (weapon != IT_ROCKET_LAUNCHER) // and ain't using it
			bot->v.impulse = 7; // Then use the Rocket Launcher
		return;
	}

	if (bot->v.ammo_cells >= 1 && // If the bot has some cells
			bot->v.waterlevel <= 1 && // and is not in water
			items & IT_LIGHTNING) // and the Lightning Gun
	{
		if (weapon != IT_LIGHTNING) // and ain't using it
			bot->v.impulse = 8; // Then use the Lightning Gun
		return;
	}

	if (bot->v.ammo_nails >= 2 && // If the bot has some nails
			items & IT_SUPER_NAILGUN) // and the Super Nailgun
	{
		if (weapon != IT_SUPER_NAILGUN) // and ain't using it
			bot->v.impulse = 5; // Then use the Super Nailgun
		return;
	}

	if (bot->v.ammo_rockets >= 1 && // If the bot has some rockets
			items & IT_GRENADE_LAUNCHER) // and the Grenade Launcher
	{
		if (weapon != IT_GRENADE_LAUNCHER) // and ain't using it
			bot->v.impulse = 6; // Then use the Grenade Launcher
		return;
	}

	if (bot->v.ammo_nails >= 1 && // If the bot has some nails
			items & IT_NAILGUN) // and the Nailgun
	{
		if (weapon != IT_NAILGUN) // and ain't using it
			bot->v.impulse = 4; // Then use the Nailgun
		return;
	}

	if (bot->v.ammo_shells >= 2 && // If the bot has some shells
			items & IT_SUPER_SHOTGUN) // and the Super Shotgun
	{
		if (weapon != IT_SUPER_SHOTGUN) // and ain't using it
			bot->v.impulse = 3; // Then use the Super Shotgun
		return;
	}

	if (bot->v.ammo_shells >= 1 && // If the bot has some shells
			items & IT_SHOTGUN) // and the Shotgun
	{
		if (weapon != IT_SHOTGUN) // and ain't using it
			bot->v.impulse = 2; // Then use the Shotgun
		return;
	}
	if (weapon != IT_AXE)
		bot->v.impulse = 1; //Bring on the AXE
}

float onLedge(vec3_t org, vec3_t angle, edict_t *bot) {
	vec3_t ledgestart[3], ledgeend[3];
	float tempfraction[3];
	int i;

	tempfraction[0] = 1.0f;
	for (i = 0; i < 3; i++) {
		VectorCopy(org, ledgestart[i]); //start position
		VectorMA(ledgestart[i], 20 * (i + 1), angle, ledgestart[i]); //add forward vector scaled
		VectorCopy(ledgestart[i], ledgeend[i]); //copy to end position

		ledgestart[i][2] += sv_stepheight.getFloat(); //move up a step
		ledgeend[i][2] -= sv_stepheight.getFloat() * 4; //move down 4 steps

		//AddTrail(ledgestart[i],ledgeend[i],0,0.05f,0.75f,zerodir);	//add trails for each

		tempfraction[i] = BotTraceline(ledgestart[i], ledgeend[i], bot);
	}

	return tempfraction[0] + tempfraction[1] + tempfraction[2];
}

/**
 * This function is used to get the bot to actually
 * move and shoot and kill and destroy and wreak havoc and
 * slaughter and... oh sorry
 */
void AttackMoveBot(client_t *client, bool fire, bool move, bool strafe, edict_t *enemy) {
	if (fire) { // If he has an enemy
		vec3_t origin;

		client->edict->v.button0 = 1; // Shoot, slaughter, kill, destroy!!!!

		if (move) {
			VectorSubtract(enemy->v.origin, client->edict->v.origin, origin);

			if (VectorLength(origin) > 200 || // If further away then 200 units from enemy
					(int) client->edict->v.weapon & 4096) // or is using the axe
				client->cmd.forwardmove = 400; // Then chase after the enemy
			else // If closer then 200 units to enemy and not using the axe
				client->cmd.forwardmove = -400; // Then stay at a distance
		}

		if (strafe)
			client->cmd.sidemove += (rand()&127) - 64; // Make him strafe

		if (client->cmd.sidemove > 400) // If strafing to fast
			client->cmd.sidemove = 400; // Then limit the strafe speed
		else if (client->cmd.sidemove < -400) // If strafing to fast
			client->cmd.sidemove = -400; // Then limit the strafe speed
	} else { // If he has no enemy
		vec3_t origin;

		client->edict->v.button0 = 0; // Shoot, slaughter, kill, destroy!!!!

		VectorSubtract(enemy->v.origin, client->edict->v.origin, origin);

		if (move) {
			if (VectorLength(origin) > 80)
				client->cmd.forwardmove = 400; // Let him run
			else if (VectorLength(origin) < 60)
				client->cmd.forwardmove = -400; // Let him run
		}
		client->cmd.sidemove = 0; // Let him run stright
	}

	switch (skill.getInt()) {
		case 1: // Medium
			client->edict->v.v_angle[0] = client->edict->v.angles[0] + (rand()&15) - 8;
			client->edict->v.v_angle[1] = client->edict->v.angles[1] + (rand()&15) - 8; // Adjust him to aim where he looks and make it not 100% accurate
			client->edict->v.v_angle[2] = client->edict->v.angles[2] + (rand()&15) - 8;
			break;

		case 2: // Hard
			client->edict->v.v_angle[0] = client->edict->v.angles[0] + (rand()&7) - 4;
			client->edict->v.v_angle[1] = client->edict->v.angles[1] + (rand()&7) - 4; // Adjust him to aim where he looks and make it not 100% accurate
			client->edict->v.v_angle[2] = client->edict->v.angles[2] + (rand()&7) - 4;
			break;

		case 3: // Nightmare
			client->edict->v.v_angle[0] = client->edict->v.angles[0] + (rand()&3) - 2;
			client->edict->v.v_angle[1] = client->edict->v.angles[1] + (rand()&3) - 2; // Adjust him to aim where he looks and make it not 100% accurate
			client->edict->v.v_angle[2] = client->edict->v.angles[2] + (rand()&3) - 2;
			break;

		default: // Easy
			client->edict->v.v_angle[0] = client->edict->v.angles[0] + (rand()&31) - 16;
			client->edict->v.v_angle[1] = client->edict->v.angles[1] + (rand()&31) - 16; // Adjust him to aim where he looks and make it not 100% accurate
			client->edict->v.v_angle[2] = client->edict->v.angles[2] + (rand()&31) - 16;
			break;
	}

	if (!bot_walkmove(client->edict, client->edict->v.angles[1], VectorLength(client->edict->v.velocity)))
		client->cmd.forwardmove = 0;
}

/**
 * This function is used to search for
 * something for the bot to shoot at
 */
void SearchForEnemy(client_t *client) {//for a normal game
	edict_t *bot = client->edict;
	edict_t *nmy = bot->bot.enemy;
	vec3_t eyes1, eyes2, test;
	int num;

	// If he has an enemy that ain't himself
	if (nmy != bot && nmy->v.health > 0){ // and has some health
		VectorAdd(nmy->v.origin, nmy->v.view_ofs, eyes1); // We want the origin of the enemies eyes
		VectorAdd(bot->v.origin, bot->v.view_ofs, eyes2); // We want the origin of the bots eyes

		if (IsInView(eyes1, eyes2, bot, nmy, 80, test)) { // test if its in view
			VectorCopy(test, bot->v.angles); // Then turn towards the enemy
			AttackMoveBot(client, true, true, true, nmy); // and start running and shooting
			return; // We are done here now...
		}
	}

	// Guess we had no enemy or couldn't see him anymore
	bot->bot.enemy = bot; // Set enemy to the bot himself again
	nmy = Nextent(globot.world); // Prepare to loop through clients
	num = 0;

	//save up to 31 calculations
	VectorAdd(bot->v.origin, bot->v.view_ofs, eyes2); // We want the origin of the bots eyes

	while (num < globot.MaxClients) { // Keep looping as long as there are clients
		if (nmy->bot.Active && // Enemy is playing
				nmy != bot && // and is not the bot himself
				nmy->v.health > 0)//	&&		// and is alive
			//nmy->v.team != bot->v.team)	// and in another team
			{
			VectorAdd(nmy->v.origin, nmy->v.view_ofs, eyes1); // We want the origin of the clients eyes

			if (IsInView(eyes1, eyes2, bot, nmy, 90, test)) // test if its in view
			{
				bot->bot.enemy = nmy; // Then set him as the enemy
				VectorCopy(test, bot->v.angles); // Then turn to the enemy
				AttackMoveBot(client, true, true, true, nmy); // and start running, jumping and shooting
				return; // We are done here now...
			}
		}
		num++;
		nmy = Nextent(nmy); // If the client was'nt visible then continue the loop with the next client
	}

	// Guess we found no enemies
	bot->v.angles[0] = 0;
	bot->v.angles[2] = 0;
	bot->v.button0 = 0; // So why waste ammo?
}

void BotMove(client_t *client) {
	edict_t *bot = client->edict;

	vec3_t org, end[5], anglevec;
	float fraction[5], ledgefraction;
	float angle;
	int i;
	float desired_angle, actual_angle;

	desired_angle = 0;

	//get our forward trace
	VectorCopy(bot->v.origin, org);

	//copy current position
	for (i = 0; i < 5; i++)
		VectorCopy(bot->v.origin, end[i]);

	//todo: test with a line forward or something to get better check if anything is in the way

	//this should give me the direction
	angle = DEG2RAD(bot->v.angles[1]);
	end[0][0] += cos(angle)*50; //x
	end[0][1] += sin(angle)*50; //y
	end[0][2] += 0; //z

	//this should give me the direction
	angle = DEG2RAD(bot->v.angles[1]) + 0.524f;
	end[1][0] += cos(angle)*60; //x
	end[1][1] += sin(angle)*60; //y
	end[1][2] += 0; //z

	//this should give me the direction
	angle = DEG2RAD(bot->v.angles[1]) - 0.524f;
	end[2][0] += cos(angle)*60; //x
	end[2][1] += sin(angle)*60; //y
	end[2][2] += 0; //z

	//this should give me the direction
	angle = DEG2RAD(bot->v.angles[1]) + 1.047f;
	end[3][0] += cos(angle)*60; //x
	end[3][1] += sin(angle)*60; //y
	end[3][2] += 0; //z

	//this should give me the direction
	angle = DEG2RAD(bot->v.angles[1]) - 1.047f;
	end[4][0] += cos(angle)*60; //x
	end[4][1] += sin(angle)*60; //y
	end[4][2] += 0; //z

	//loop through all the end positions and work out fractions
	for (i = 0; i < 5; i++) {
		fraction[i] = BotTraceline(org, end[i], bot);
	}

	//check to make sure they arnt walking off a cliff
	angle = DEG2RAD(bot->v.angles[1]);
	anglevec[0] = cos(angle);
	anglevec[1] = sin(angle);
	anglevec[2] = 0;

	ledgefraction = onLedge(org, anglevec, bot);

	if (0.999f > fraction[0]) { //something blocks our way or
		Con_DPrintf("Walking into something.\n");
		desired_angle = RandomRange(0, 36000) / 100.0f;
	} else if (2.5f < ledgefraction) {
		Con_DPrintf("Walking off ledge.\n");
		desired_angle = RandomRange(0, 36000) / 100.0f;
	} else {
		desired_angle -= (1 - fraction[1]) + (1 - fraction[3]);
		desired_angle += (1 - fraction[2]) + (1 - fraction[4]);
	}

	//add noise to movement to stop from moving in direct lines
	if (RandomRange(0, 10) > 9) {
		desired_angle += RandomRange(0, 100) / 100.0f - 0.5f;
	}

	//if no change to angle this time continue to try for the desired angle.
	if (desired_angle == 0) {
		desired_angle = bot->bot.desired_angle[1];
	}

	//try to get new angle (using skill as extra momentum and friction
	actual_angle = aimangle(desired_angle, bot->bot.prev_angle_delta, 0.25f);
	bot->bot.prev_angle_delta = actual_angle;
	bot->v.angles[1] += actual_angle;

	//work out new desired angle
	bot->bot.desired_angle[1] = desired_angle - actual_angle;

	//get our side traces
	//AddTrail(org,end[0],0,0.05f,0.5f,zerodir);
	//AddTrail(org,end[1],2,0.05f,0.5f,zerodir);
	//AddTrail(org,end[2],4,0.05f,0.5f,zerodir);
	//AddTrail(org,end[3],4,0.05f,0.5f,zerodir);
	//AddTrail(org,end[4],2,0.05f,0.5f,zerodir);

	VectorCopy(org, bot->bot.prev_org);

	client->cmd.forwardmove = 400 * fraction[0];
	client->cmd.sidemove = 0;
}

/**
 * This function is where everything starts...
 * From here we search for enemies to hunt and shoot
 * and make the bot to respawn if killed...
 */
void BotPreFrame(client_t *client) {
	edict_t *bot = client->edict;

	client->cmd.forwardmove = 0; // Stop all bots from running

	SwitchWeapon(bot);

	if (bot->v.deadflag == DEAD_NO) // If the bot is alive
	{
		SearchForEnemy(client);
		if (bot->bot.enemy == bot) {
			BotMove(client);
		}
		//if (coop.value != 1)			// if not coop find a player	//qmb :bot
		//	SearchForEnemy (client);	// Then search for an enemy or someone to chase
		//else
		//	SearchForEnemyCoop (client);// Then search for an enemy or someone to chase
	} else {
		bot->v.button0 = 0; // If dead then clear all buttons
		bot->v.button1 = 0;
		bot->v.button2 = 0;
	}

	if (bot->v.deadflag == DEAD_RESPAWNABLE) // If dead and ready to respawn
	{
		if (bot->bot.delaytime == 0) // and hasnt been delayed
			bot->bot.delaytime = cl.time + rand() % 33 / 10.0; // time died + random
		else if (bot->bot.delaytime <= cl.time) // and they have delayed
		{
			bot->v.button1 = 1; // Then respawn
			bot->bot.delaytime = 0; // Reset delay time
		}
	}
}

/**
 * This function is used to check
 * if the bot is running into a wall
 */
void BotPostFrame(client_t *client) {
	edict_t *bot = client->edict;

	if (bot->bot.chase) { // If we are chasing someone
		if ((bot->v.velocity[0] < 20 && bot->v.velocity[0] > -20) && // And if our speed is slow
				(bot->v.velocity[1] < 20 && bot->v.velocity[1] > -20)) { // (running against a wall)
			bot->bot.chase = NULL; // Then find a new client to chase
		}
	}

	// This last piece here is only used by Team Fortress

	if (!bot->bot.menudone) { // If we have not gone past the class selection menu
		if (bot->v.health > 5) // If we have more health then 5
			bot->bot.menudone = true; // Then this is not Team Fortress so there is no menu

			// Looks like we are playing Team Fortress after all

		else if (sv.time > bot->bot.connecttime + 2) // If bot has been active more then 2 seconds then choose a random class and leave the menu
			bot->v.impulse = 10;
		else if (sv.time > bot->bot.connecttime + 1 && // If bot has been active between 1 and 2 secs
				teamplay.getBool()) // and we are playing teamplay
			bot->v.impulse = 5; // Then choose a random team
	}
}

/**
 * This function is what allows us to use
 * the command "addbot" from the console
 */
void Bot_Init(void) {
	Cmd::addCmd("addbot", NextFreeClient);
}
