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

// host.c -- coordinates spawning and killing of local servers
#include <setjmp.h>
#include "quakedef.h"
#include "input.h"
#include "NeuralNets.h"
#include "Texture.h"
#include "FileManager.h"

/*
A server can allways be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.
 */

quakeparms_t host_parms;

bool host_initialized; // true if into command execution

double host_frametime;
double host_time;
double realtime; // without any filtering or bounding
double oldrealtime; // last frame run
int host_framecount;

int host_hunklevel;

int minimum_memory;

client_t *host_client; // current client

jmp_buf host_abortserver;

byte *host_basepal;
byte *host_colormap;

CVar host_framerate("host_framerate", "0"); // set for slow motion
CVar host_speeds("host_speeds", "0"); // set for running times
CVar sys_ticrate("sys_ticrate", "0.05");
CVar serverprofile("serverprofile", "0");
CVar fraglimit("fraglimit", "0", false, true);
CVar timelimit("timelimit", "0", false, true);
CVar teamplay("teamplay", "0", false, true);
CVar samelevel("samelevel", "0");
CVar noexit("noexit", "0", false, true);
CVar developer("developer", "0");
CVar skill("skill", "1"); // 0 - 3
CVar deathmatch("deathmatch", "0"); // 0, 1, or 2
CVar coop("coop", "0"); // 0 or 1
CVar pausable("pausable", "1");
CVar temp1("temp1", "0");
CVar max_fps("max_fps", "72", true); // set the max fps
CVar show_fps("show_fps", "1", true); // set for running times - muff

int fps_count;

void Host_EndGame(const char *message, ...) {
	va_list argptr;
	char string[1024];

	va_start(argptr, message);
	vsprintf(string, message, argptr);
	va_end(argptr);
	Con_DPrintf("Host_EndGame: %s\n", string);

	if (sv.active)
		Host_ShutdownServer(false);

	if (cls.state == ca_dedicated)
		Sys_Error("Host_EndGame: %s\n", string); // dedicated servers exit

	if (cls.demonum != -1)
		CL_NextDemo();
	else
		CL_Disconnect();

	longjmp(host_abortserver, 1);
}

/**
 * This shuts down both the client and server
 */
void Host_Error(const char *error, ...) {
	va_list argptr;
	char string[1024];
	static bool inerror = false;

	if (inerror)
		Sys_Error("Host_Error: recursively entered");
	inerror = true;

	SCR_EndLoadingPlaque(); // reenable screen updates

	va_start(argptr, error);
	vsprintf(string, error, argptr);
	va_end(argptr);
	Con_Printf("Host_Error: %s\n", string);

	if (sv.active)
		Host_ShutdownServer(false);

	if (cls.state == ca_dedicated)
		Sys_Error("Host_Error: %s\n", string); // dedicated servers exit

	CL_Disconnect();
	cls.demonum = -1;

	inerror = false;

	longjmp(host_abortserver, 1);
}

void Host_FindMaxClients(void) {
	svs.maxclients = 1;

	int i = COM_CheckParm("-dedicated");
	if (i) {
		cls.state = ca_dedicated;
		if (i != (com_argc - 1)) {
			svs.maxclients = atoi(com_argv[i + 1]);
		} else
			svs.maxclients = 8;
	} else
		cls.state = ca_disconnected;

	//qmb :64 clients
	//various number changes including max_scoreboard
	i = COM_CheckParm("-listen");
	if (i) {
		if (cls.state == ca_dedicated)
			Sys_Error("Only one of -dedicated or -listen can be specified");
		if (i != (com_argc - 1))
			svs.maxclients = atoi(com_argv[i + 1]);
		else
			svs.maxclients = 32;
	}
	if (svs.maxclients < 1)
		svs.maxclients = 32;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclientslimit < 32)
		svs.maxclientslimit = 32;
	svs.clients = (client_t *) Hunk_AllocName(svs.maxclientslimit * sizeof (client_t), "clients");

	if (svs.maxclients > 1)
		deathmatch.set(1.0f);
	else
		deathmatch.set(0.0f);
}

void Host_InitLocal(void) {
	Host_InitCommands();

	CVar::registerCVar(&host_framerate);
	CVar::registerCVar(&host_speeds);
	CVar::registerCVar(&sys_ticrate);
	CVar::registerCVar(&serverprofile);
	CVar::registerCVar(&fraglimit);
	CVar::registerCVar(&timelimit);
	CVar::registerCVar(&teamplay);
	CVar::registerCVar(&samelevel);
	CVar::registerCVar(&noexit);
	CVar::registerCVar(&skill);
	CVar::registerCVar(&developer);
	CVar::registerCVar(&deathmatch);
	CVar::registerCVar(&coop);
	CVar::registerCVar(&pausable);
	CVar::registerCVar(&temp1);
	CVar::registerCVar(&show_fps);
	CVar::registerCVar(&max_fps);

	Host_FindMaxClients();
	host_time = 1.0; // so a think at time 0 won't get called
}

/**
 * Writes key bindings and archived cvars to config.cfg
 * 
 * Note: Dedicated servers don't parse and set the config.cfg cvars
 */
void Host_WriteConfiguration(void) {
	if (host_initialized & !isDedicated) {
		int handle = SystemFileManager::FileOpenWrite(va("%s/config.cfg", com_gamedir));
		Key_WriteBindings(handle);
		CVar::writeVariables(handle);
		SystemFileManager::FileClose(handle);
	}
}

/**
 * Sends text across to be displayed
 * 
 * FIXME: make this just a stuffed echo?
 */
void SV_ClientPrintf(const char *fmt, ...) {
	va_list argptr;
	char string[1024];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);
	
	MSG_WriteByte(&host_client->message, svc_print);
	MSG_WriteString(&host_client->message, string);
}

/**
 * Sends text to all active clients
 */
void SV_BroadcastPrintf(const char *fmt, ...) {
	va_list argptr;
	char string[1024];
	int i;

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	for (i = 0; i < svs.maxclients; i++)
		if (svs.clients[i].active && svs.clients[i].spawned) {
			MSG_WriteByte(&svs.clients[i].message, svc_print);
			MSG_WriteString(&svs.clients[i].message, string);
		}
}

/**
 * Send text over to the client to be executed
 */
void Host_ClientCommands(const char *fmt, ...) {
	va_list argptr;
	char string[1024];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&host_client->message, svc_stufftext);
	MSG_WriteString(&host_client->message, string);
}

/**
 * Called when the player is getting totally kicked off the host
 * 
 * if (crash = true), don't bother sending signofs
 */
void SV_DropClient(bool crash) {
	int saveSelf;
	int i;
	client_t *client;

	if (!crash) {
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage(host_client->netconnection)) {
			MSG_WriteByte(&host_client->message, svc_disconnect);
			NET_SendMessage(host_client->netconnection, &host_client->message);
		}

		if (host_client->edict && host_client->spawned) {
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			PR_ExecuteProgram(pr_global_struct->ClientDisconnect);
			pr_global_struct->self = saveSelf;
		}

		Sys_Printf("Client %s removed\n", host_client->name);
	}

	// break the net connection
	NET_Close(host_client->netconnection);
	host_client->netconnection = NULL;

	// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	net_activeconnections--;

	// send notification to all clients
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++) {
		if (!client->active)
			continue;
		MSG_WriteByte(&client->message, svc_updatename);
		MSG_WriteByte(&client->message, host_client - svs.clients);
		MSG_WriteString(&client->message, "");
		MSG_WriteByte(&client->message, svc_updatefrags);
		MSG_WriteByte(&client->message, host_client - svs.clients);
		MSG_WriteShort(&client->message, 0);
		MSG_WriteByte(&client->message, svc_updatecolors);
		MSG_WriteByte(&client->message, host_client - svs.clients);
		MSG_WriteByte(&client->message, 0);
	}
}

/**
 * This only happens at the end of a game, not between levels
 */
void Host_ShutdownServer(bool crash) {
	int i;
	int count;
	sizebuf_t buf;
	char message[4];
	double start;

	if (!sv.active)
		return;

	sv.active = false;

	// stop all client sounds immediately
	if (cls.state == ca_connected)
		CL_Disconnect();

	// flush any pending messages - like the score!!!
	start = Sys_FloatTime();
	do {
		count = 0;
		for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++) {
			if (host_client->active && host_client->message.cursize) {
				if (NET_CanSendMessage(host_client->netconnection)) {
					NET_SendMessage(host_client->netconnection, &host_client->message);
					SZ_Clear(&host_client->message);
				} else {
					NET_GetMessage(host_client->netconnection);
					count++;
				}
			}
		}
		if ((Sys_FloatTime() - start) > 3.0)
			break;
	}	while (count);

	// make sure all the clients know we're disconnecting
	buf.data = (byte *) message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte(&buf, svc_disconnect);
	count = NET_SendToAll(&buf, 5);
	if (count)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

	for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
		if (host_client->active)
			SV_DropClient(crash);

	// clear structures
	memset(&sv, 0, sizeof (sv));
	memset(svs.clients, 0, svs.maxclientslimit * sizeof (client_t));
}

/**
 * This clears all the memory used by both the client and server, but does not 
 * reinitialize anything.
 */
void Host_ClearMemory(void) {
	Con_DPrintf("Clearing memory\n");
	Mod_ClearAll();
	if (host_hunklevel)
		Hunk_FreeToLowMark(host_hunklevel);

	cls.signon = 0;
	memset(&sv, 0, sizeof (sv));
	memset(&cl, 0, sizeof (cl));
}

//============================================================================

/**
 * Returns false if the time is too short to run a frame
 */
bool Host_FilterTime(float time) {
	realtime += time;

	if (max_fps.getInt() > 0) {
		// CAPTURE <anthony@planetquake.com>
		if (!cls.capturedemo) // only allow the following early return if not capturing:
			if (!cls.timedemo && realtime - oldrealtime < 1.0 / max_fps.getFloat())
				return false; // framerate is too high
	}

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	if (host_framerate.getFloat() > 0)
		host_frametime = host_framerate.getFloat();
	else { // don't allow really long or short frames
		if (host_frametime > 0.1)
			host_frametime = 0.1;
		if (host_frametime < 0.001)
			host_frametime = 0.001;
	}

	return true;
}

/**
 * Add them exactly as if they had been typed at the console
 */
void Host_GetConsoleCommands(void) {
	char *cmd;

	while (1) {
		cmd = Sys_ConsoleInput();
		if (!cmd)
			break;
		Cbuf_AddText(cmd);
	}
}

void Host_ServerFrame(void) {
	// run the world state
	pr_global_struct->frametime = host_frametime;

	// set the time and clear the general datagram
	SV_ClearDatagram();

	// check for new clients
	SV_CheckForNewClients();

	// read client messages
	SV_RunClients();

	// move things around and think
	// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
		SV_Physics();

	// send all messages to the clients
	SV_SendClientMessages();
}

/**
 * Runs all active servers
 */
void _Host_Frame(float time) {
	static double time1 = 0;
	static double time2 = 0;
	static double time3 = 0;
	int pass1, pass2, pass3;

	if (setjmp(host_abortserver))
		return; // something bad happened, or the server disconnected

	// keep the random time dependent
	rand();

	// decide the simulation time
	if (!Host_FilterTime(time))
		return; // don't run too fast, or packets will flood out

	// get new key events
	Sys_SendKeyEvents();

	// allow mice or other external controllers to add commands
	IN_Commands();

	// process console commands
	Cbuf_Execute();

	NET_Poll();

	// if running the server locally, make intentions now
	if (sv.active)
		CL_SendCmd();

	//-------------------
	// server operations
	//-------------------

	// check for commands typed to the host
	Host_GetConsoleCommands();

	if (sv.active)
		Host_ServerFrame();

	//-------------------
	// client operations
	//-------------------

	// if running the server remotely, send intentions now after
	// the incoming messages have been read
	if (!sv.active)
		CL_SendCmd();

	host_time += host_frametime;

	// fetch results from server
	if (cls.state == ca_connected) {
		CL_ReadFromServer();
	}

	// update video
	if (host_speeds.getBool())
		time1 = Sys_FloatTime();

	SCR_UpdateScreen();

	if (host_speeds.getBool())
		time2 = Sys_FloatTime();

	// update audio
	if (cls.signon == SIGNONS) {
		S_Update(r_origin, vpn, vright, vup);
		CL_DecayLights();
	} else
		S_Update(vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	CDAudio_Update();

	if (host_speeds.getBool()) {
		pass1 = (time1 - time3)*1000;
		time3 = Sys_FloatTime();
		pass2 = (time2 - time1)*1000;
		pass3 = (time3 - time2)*1000;
		Con_Printf("%3i tot %3i server %3i gfx %3i snd\n",
				pass1 + pass2 + pass3, pass1, pass2, pass3);
	}

	host_framecount++;
}

void Host_Frame(float time) {
	double time1, time2;
	static double timetotal;
	static int timecount;
	int i, c, m;

	if (!serverprofile.getBool()) {
		_Host_Frame(time);
		return;
	}

	time1 = Sys_FloatTime();
	_Host_Frame(time);
	time2 = Sys_FloatTime();

	timetotal += time2 - time1;
	timecount++;

	if (timecount < 1000)
		return;

	m = timetotal * 1000 / timecount;
	timecount = 0;
	timetotal = 0;
	c = 0;
	for (i = 0; i < svs.maxclients; i++) {
		if (svs.clients[i].active)
			c++;
	}

	fps_count++;

	Con_Printf("serverprofile: %2i clients %2i msec\n", c, m);
}

//============================================================================

void Bot_Init(void);
void Host_Init(quakeparms_t *parms) {
	if (standard_quake)
		minimum_memory = MINIMUM_MEMORY;
	else
		minimum_memory = MINIMUM_MEMORY_LEVELPAK;

	if (COM_CheckParm("-minmemory"))
		parms->memsize = minimum_memory;

	host_parms = *parms;

	if (parms->memsize < minimum_memory)
		Sys_Error("Only %4.1f megs of memory available, can't execute game", parms->memsize / (float) 0x100000);

	com_argc = parms->argc;
	com_argv = parms->argv;

	Memory_Init(parms->membase, parms->memsize);
	Cbuf_Init();
	Cmd::Init();
	V_Init();
	NN_init();
	COM_Init();
	Host_InitLocal();
	W_LoadWadFile("gfx.wad");
	Key_Init();
	Con_Init();
	M_Init();
	PR_Init();
	Mod_Init();
	NET_Init();
	SV_Init();
	Bot_Init();

	Con_Printf("Exe: "__TIME__" "__DATE__"\n");
	Con_Printf("%4.1f megabyte heap\n", parms->memsize / (1024 * 1024.0));

	if (cls.state != ca_dedicated) {
		host_basepal = (byte *) COM_LoadHunkFile("gfx/palette.lmp");
		if (!host_basepal)
			Sys_Error("Couldn't load gfx/palette.lmp");
		host_colormap = (byte *) COM_LoadHunkFile("gfx/colormap.lmp");
		if (!host_colormap)
			Sys_Error("Couldn't load gfx/colormap.lmp");

		IN_Init();
		VID_Init(host_basepal);
		Draw_Init();
		SCR_Init();
		R_Init();
		TextureManager::Init();
		S_Init();
		CDAudio_Init();
		Sbar_Init();
		CL_Init();
	}

	Cbuf_InsertText("exec quake.rc\n");

	Hunk_AllocName(0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark();

	host_initialized = true;

	Sys_Printf("========Quake Initialized=========\n");
}

/**
 * FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
 * to run quit through here before the final hand off to the sys code.
 */
extern bool con_debuglog;
void Con_CloseDebugLog(void);
void Host_Shutdown(void) {
	static bool isdown = false;

	if (isdown) {
		printf("recursive shutdown\n");
		return;
	}
	isdown = true;

	// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	Host_WriteConfiguration();

	CDAudio_Shutdown();
	NET_Shutdown();
	S_Shutdown();
	IN_Shutdown();
	NN_deinit();

	TextureManager::clearAllTextures();
	Alias::shutdown();
	Cmd::shutdown();
	CVar::shutdown();
	MemoryObj::Flush(MemoryObj::ZONE);

	if (cls.state != ca_dedicated) {
		VID_Shutdown();
	}
	if (con_debuglog) {
		Con_CloseDebugLog();
	}
}
