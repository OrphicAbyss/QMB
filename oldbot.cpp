#include "quakedef.h"
#include "bot.h"

void SearchForEnemyCoop (client_t *client) {
	//its a coop game
	edict_t	*bot = client->edict;
	edict_t	*nmy = bot->bot.enemy;
	edict_t *bestent;
	vec3_t	eyes1, eyes2, origin, test, bestdir;
	int		test2, i;
	int		num;
	float	bestdist;

	VectorAdd (bot->v.origin, bot->v.view_ofs, eyes2);// We want the origin of the bots eyes

	// try all possible entities
	if (nmy != bot			&&	// If he has an enemy that aint himself
		nmy->v.health > 0) {	// and has some health
		VectorAdd (nmy->v.mins, nmy->v.maxs, eyes1);
		VectorScale (eyes1, 0.5, eyes1);
		VectorAdd (nmy->v.origin, eyes1, eyes1);

		VectorAdd (bot->v.origin, bot->v.view_ofs, eyes2);		// We want the origin of the bots eyes

		if (IsInView(eyes1, eyes2, bot, nmy, 80, test)) {		// If the bot can see his enemy
				VectorCopy (test, bot->v.angles);				// Then turn towards the enemy
				AttackMoveBot (client, true, true, false, nmy);	// and start shooting
				return;											// We are done here now...
		}
	}

	VectorCopy (bot->v.angles, bestdir);
	bestdist = 1000;
	bestent = NULL;

	//save calculations
	nmy = NEXT_EDICT(sv.edicts);
	for (i=1 ; i<sv.num_edicts ; i++, nmy = NEXT_EDICT(nmy)) {
		if (nmy->v.takedamage != DAMAGE_AIM	||				//takes damage
			nmy == bot						||				//isnt the bot
			!(nmy->v.health > 0)			||				// and is alive
			!Q_strcmp(pr_strings + nmy->v.classname, "player"))	//not a player
			continue;

		//work out position of other object
		VectorAdd (nmy->v.mins, nmy->v.maxs, eyes1);
		VectorScale (eyes1, 0.5, eyes1);
		VectorAdd (nmy->v.origin, eyes1, eyes1);

		if (IsInView(eyes1, eyes2, bot, nmy, bestdist, test)) {// If the bot can see his client
			test2 = test[1] - bot->v.angles[1];		// Another shortcut
			bestdist = abs(test2);
			bestent = nmy;
		}
	}

	if (bestent) {
		nmy = bestent;
		VectorSubtract (bestent->v.origin, bot->v.origin, origin);
		CalcAngles (origin, test);		// And use it to see in what direction the client is
		bot->v.angles[0] = -test[0];			// This is reset so it doesnt look like hes running into the floor if the chase client is below him
		bot->v.angles[1] = test[1];		// Then turn the bot that way
		bot->v.angles[2] = test[2];
		bot->bot.enemy = nmy;
		AttackMoveBot (client, true, true, false, nmy);				// and start running and shooting
		return;
	}

	// Guess we found no enemies

	bot->v.button0 = 0;		// So why waste ammo?

	nmy = bot->bot.chase;	// Are we already chasing someone?

	if (nmy					&&
		nmy != bot			&&		// and is not the bot himself
		nmy->v.health > 0	&&		// and is alive
		!nmy->bot.isbot) {			// and is not a bot
		bot->bot.chase = nmy;									// Then start chasing him
		VectorSubtract (nmy->v.origin, bot->v.origin, origin);	// Get a nice vector

		CalcAngles (origin, test);		// And use it to see in what direction the client is
		bot->v.angles[0] = 0;			// This is reset so it doesnt look like hes running into the floor if the chase client is below him
		bot->v.angles[1] = test[1];		// Then turn the bot that way
		bot->v.angles[2] = 0;
		AttackMoveBot (client, false, true, false, nmy);	// And get him running
		return;
	}

	bot->bot.enemy = bot;				// Set enemy to the bot himself again
	nmy	= Nextent(globot.world);		// Prepare to loop through clients
	num	= 0;
	while (num < globot.MaxClients) {	// Keep looping as long as there are clients
		if (nmy != bot			&&		// and is not the bot himself
			nmy->v.health > 0	&&		// and is alive
			!nmy->bot.isbot) {			// and is not a bot
			VectorAdd (nmy->v.origin, nmy->v.view_ofs, eyes1);	// We want the origin of the clients eyes

			if (IsInView(eyes1, eyes2, bot, nmy, 1000, test)) {
				bot->bot.chase = nmy;						// start him chasing
				bot->v.angles[0] = 0;						// This is reset so it doesnt look like hes running into the floor if the chase client is below him
				bot->v.angles[1] = test[1];					// Then turn the bot that way
				bot->v.angles[2] = 0;
				AttackMoveBot (client, false, true, false, nmy);		// and start running
				return;										// We are done here now...
			}
		}
		num++;
		nmy = Nextent(nmy);									// If the client was'nt visible then continue the loop with the next client
	}

	//should only get here if all humans are dead
	bot->v.button0 = 0;		// So why waste ammo?
	bot->bot.chase = NULL;	// Guess we found noone to chase either... so we'll stand here and see if we find anyone the next frame
}
