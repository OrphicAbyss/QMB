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

//moved all bot connection and setup code into here

#include "quakedef.h"
#include "bot.h"

/*
=======
BotInit

This function is used to calcualte the maximum
amount of clients and to set some default values
=======
*/
void BotInit (void)
{
	client_t	*client;
	int			i;

	globot.world		= PROG_TO_EDICT(pr_global_struct->world);	// Find the world entity
	globot.MaxClients	= 0;										// Reset the MaxClients counter

	for (i=0, client=svs.clients; i<svs.maxclients /*&& i<16*/; i++, client++)	// Keep looping as long as there are clients
	{
		globot.MaxClients++;					// Increase MaxClients with 1

		client->edict->bot.ClientNo	= -1;		// We dont want anyone to have a client number until they have connected
		client->edict->bot.menudone	= false;	// Of course we have not gone past the Team Fortress menu yet
		client->edict->bot.Active	= false;	// Of course noone is active... the game havent started yet :)
		client->edict->bot.isbot	= false;	// And noone is a bot either... or human...
		client->edict->bot.delaytime= 0;		// Set the delay time to 0 (not dead)
	}

	for (i=1; i<globot.MaxClients; i++)
		globot.botactive[i] = false;

	if (globot.MaxClients > 32)					// We dont allow more then 32 players right now
		globot.MaxClients = 31;
}

/*
===========
PickBotName

This function is used to give each bot a different name
===========
*/
char *PickBotName (int r, char *name)
{
	switch (r){
	case 1:		sprintf(name, "%s", "(GloBot) Tomaz"); break;			// God of globot
	case 2:		sprintf(name, "%s", "(GloBot) FrikaC"); break;			// God of bots
	case 3:		sprintf(name, "%s", "(GloBot) LordHavoc"); break;		// God of engines
	case 4:		sprintf(name, "%s", "(GloBot) Dr Labman"); break;		// My favorite deity
	case 5:		sprintf(name, "%s", "(GloBot) Necrophilissimo"); break;	// QMB's QC coder (where are you now?)
	case 6:		sprintf(name, "%s", "(GloBot) Chrome"); break;			//people of quake
	case 7:		sprintf(name, "%s", "(GloBot) Planaria"); break;
	case 8:		sprintf(name, "%s", "(GloBot) KrimZon"); break;
	case 9:		sprintf(name, "%s", "(GloBot) Akuma"); break;
	case 10:	sprintf(name, "%s", "(GloBot) scar3crow"); break;
	case 11:	sprintf(name, "%s", "(GloBot) Quest"); break;			//cookies
	case 12:	sprintf(name, "%s", "(GloBot) CocoT"); break;			//boobies
	case 13:	sprintf(name, "%s", "(GloBot) KyleMac"); break;
	case 14:	sprintf(name, "%s", "(GloBot) Koolio"); break;
	case 15:	sprintf(name, "%s", "(GloBot) Krust"); break;
	case 16:	sprintf(name, "%s", "(GloBot) BramBo"); break;
	case 17:	sprintf(name, "%s", "(GloBot) Horn"); break;
	case 18:	sprintf(name, "%s", "(GloBot) c0burn"); break;
	case 19:	sprintf(name, "%s", "(GloBot) Heffo"); break;
	case 20:	sprintf(name, "%s", "(GloBot) Randy"); break;
	case 21:	sprintf(name, "%s", "(GloBot) Tei"); break;
	case 22:	sprintf(name, "%s", "(GloBot) Gleeb"); break;
	case 23:	sprintf(name, "%s", "(GloBot) painQuin"); break;
	case 24:	sprintf(name, "%s", "(GloBot) thedaemon"); break;
	case 25:	sprintf(name, "%s", "(GloBot) IceDagger"); break;
	case 26:	sprintf(name, "%s", "(GloBot) MrG"); break;
	case 27:	sprintf(name, "%s", "(GloBot) Electro"); break;
	case 28:	sprintf(name, "%s", "(GloBot) Rick"); break;
	case 29:	sprintf(name, "%s", "(GloBot) Ghostface"); break;
	case 30:	sprintf(name, "%s", "(GloBot) CheapAlert"); break;
	default:
		sprintf(name, "%s%i",  "(GloBot) ", r); break;
	}

	return name;
}

/*
============
UpdateClient

This function is used to "fake" a real
client so the bot shows up on the scoreboard
============
*/
void UpdateClient (client_t *client, int ClientNo)
{
	MSG_WriteByte	(&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte	(&sv.reliable_datagram, ClientNo);
	MSG_WriteString	(&sv.reliable_datagram, client->name);
	MSG_WriteByte	(&sv.reliable_datagram, svc_updatefrags);
	MSG_WriteByte	(&sv.reliable_datagram, ClientNo);
	MSG_WriteShort	(&sv.reliable_datagram, client->old_frags);
	MSG_WriteByte	(&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte	(&sv.reliable_datagram, ClientNo);
	MSG_WriteByte	(&sv.reliable_datagram, client->colors);
}

/*
==========
BotConnect

This function is used to connect the bot's
==========
*/
void BotConnect (client_t *client, int ClientNo, int color, char *name)
{
	edict_t	*self	= PROG_TO_EDICT(pr_global_struct->self);	// Make a backup of the current QC self
	edict_t	*bot	= client->edict;
	int		randombot, i;
	char	nameTemp[32];

	bot->bot.isbot			= true;								// And yes this is a bot
	bot->bot.Active			= true;								// and hes active
	bot->bot.enemy			= bot;								// Now why is he chasing himself?
	bot->bot.connecttime	= sv.time;
	bot->bot.ClientNo		= ClientNo;							// Now we get a clientnumber
	bot->bot.prev_angle_delta = 0;			// We havn't tried to turn yet

	randombot = ceil (RandomRange (0, 30));

	for(i=1;globot.botactive[randombot];i++)
		randombot = i;

	globot.botactive[randombot] = true;

	if (name[0] != '0')
		Q_strcpy (client->name, name);
	else
		Q_strcpy (client->name, PickBotName (randombot, nameTemp));

	if (color != 666)
	{
		client->colors		= color * 16 + color;				// The bot must have a color
		bot->v.team			= color + 1;						// And to be in a team
	}
	else
	{
		client->colors		= (randombot%16) * 16 + (randombot%16);	// The bot must have a color
		bot->v.team			= (randombot%16) + 1;					// And to be in a team
	}
	client->old_frags		= 0;								// And since he just joined he cant have got any frags yet

	bot->v.colormap			= ClientNo;							// Without this he wont be using any colored clothes
	bot->v.netname			= client->name - pr_strings;		// Everyone wants a name

	UpdateClient (client, ClientNo);							// Update the scoreboard

	pr_global_struct->self	= EDICT_TO_PROG(bot);				// Update the QC self to be the bot

	PR_ExecuteProgram (pr_global_struct->SetNewParms);			// Now call some QC functions
	PR_ExecuteProgram (pr_global_struct->ClientConnect);		// Now call some more QC functions
	PR_ExecuteProgram (pr_global_struct->PutClientInServer);	// Now call yet some more QC functions

	pr_global_struct->self	= EDICT_TO_PROG (self);				// Get back to the backup
}

/*
==============
NextFreeClient

This function is used to find an empty client spot
==============
*/
void NextFreeClient (void)
{
	client_t	*client;
	int			i, color;
	char		name[32];

	if (Cmd_Argc() == 2)
	{
		color = Q_atoi(Cmd_Argv(1));
		sprintf (name, "0");
	}
	else if (Cmd_Argc() == 3)
	{
		color = Q_atoi(Cmd_Argv(1));
		sprintf (name, "%s", Cmd_Argv(2));
	}
	else
	{
		color = 666;
		sprintf (name, "0");
	}

	for (i=0, client=svs.clients; i<svs.maxclients; i++, client++)	// Keep looping as long as there are free client slots
	{
		if (!client->edict->bot.Active)				// We found a free client slot
		{
			BotConnect (client, i, color, name);	// so why not connect a bot?
			return;									// We are done now
		}
	}

	SV_BroadcastPrintf ("Unable to connect a bot, server is full.\n");	// No free client slots = no more bots allowed
}

