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
// cmd.c -- Quake script command processing module

#include "quakedef.h"

#include <list>

using std::list;

static list<Cmd *> Cmds;

Cmd::Cmd(const char* name, xcommand_t func){
	this->name = (char *)MemoryObj::ZAlloc (Q_strlen(name)+1);
	Q_strcpy (this->name, name);
	this->func = func;
}

char *Cmd::getName(){
	return this->name;
}

void Cmd::call(){
	this->func();
}

xcommand_t Cmd::getFunction(){
	return this->func;
}

void Cmd::addCmd(const char* name, xcommand_t func){
	// fail if the command is a variable name
	if (CVar::findCVar(name)){
		Con_Printf ("Command: %s already defined as a cvar\n", name);
		return;
	}

	// fail if the command already exists
	if (Cmd::findCmd(name)){
		Con_Printf ("Command: %s already defined\n", name);
		return;
	}

	Cmd *cmd = new Cmd(name,func);
	Cmds.push_back(cmd);
}

Cmd *Cmd::findCmd(const char* name){
	list<Cmd *>::iterator i;

	for (i = Cmds.begin(); i != Cmds.end(); i++){
		Cmd *cmd = *i;

		if (0 == Q_strcmp(cmd->getName(),name)){
			return cmd;
		}
	}

	return NULL;
}

bool Cmd::consoleCommand(){
	Cmd *cmd = Cmd::findCmd(CmdArgs::getArg(0));

	if (cmd){
		cmd->call();
		return true;
	}
	return false;
}

char *Cmd::completeCommand(char* partial){
	list<Cmd *>::iterator i;
	Cmd *match = NULL;
	int sizeOfStr = Q_strlen(partial);
	bool multiple = false;

	for (i = Cmds.begin(); i != Cmds.end() && !multiple; i++){
		Cmd *cmd = *i;

		if (0 == Q_strncmp(cmd->getName(),partial,sizeOfStr)){
			if (match != NULL){
				multiple = true;
			} else {
				match = cmd;
			}
		}
	}

	if (multiple){
		bool first = true;
		Con_Printf("Commands: ");
		for (i = Cmds.begin(); i != Cmds.end(); i++){
			Cmd *cmd = *i;

			if (0 == Q_strncmp(cmd->getName(),partial,sizeOfStr)){
				if (first){
					first = false;
					Con_Printf(cmd->getName());
				} else {
					Con_Printf(", %s",cmd->getName());
				}
			}
		}
		Con_Printf("\n");
		match = NULL;
	}

	if (match != NULL)
		return match->getName();
	else
		return NULL;
}



static list<Alias *> Aliases;

Alias::Alias(const char* name, const char* cmdString){
	this->name = (char *)MemoryObj::ZAlloc (Q_strlen(name)+1);
	Q_strcpy (this->name, name);
	this->cmdString = (char *)MemoryObj::ZAlloc (Q_strlen(cmdString)+1);
	Q_strcpy (this->cmdString, cmdString);
}

void Alias::remove(){
	MemoryObj::ZFree(this->name);
	MemoryObj::ZFree(this->cmdString);
}

char *Alias::getName(){
	return this->name;
}

char *Alias::getCmdString(){
	return this->cmdString;
}

void Alias::addAlias(const char* name, const char* value){
	Alias *alias = new Alias(name,value);
	Aliases.push_back(alias);
}

void Alias::removeAlias(Alias* alias){
	Aliases.remove(alias);
	delete alias;
}

Alias *Alias::findAlias(const char* name){
	list<Alias *>::iterator i;

	for (i = Aliases.begin(); i != Aliases.end(); i++){
		Alias *alias = *i;

		if (0 == Q_strcmp(alias->getName(),name)){
			return alias;
		}
	}

	return NULL;
}

char *Alias::completeAlias(const char* partial){
	list<Alias *>::iterator i;
	Alias *match = NULL;
	int sizeOfStr = Q_strlen(partial);
	bool multiple = false;

	for (i = Aliases.begin(); i != Aliases.end() && !multiple; i++){
		Alias *alias = *i;

		if (0 == Q_strncmp(alias->getName(),partial,sizeOfStr)){
			if (match != NULL){
				multiple = true;
			} else {
				match = alias;
			}
		}
	}

	if (multiple){
		bool first = true;
		Con_Printf("Aliases: ");
		for (i = Aliases.begin(); i != Aliases.end(); i++){
			Alias *alias = *i;

			if (0 == Q_strncmp(alias->getName(),partial,sizeOfStr)){
				if (first){
					first = false;
					Con_Printf(alias->getName());
				} else {
					Con_Printf(", %s",alias->getName());
				}
			}
		}
		Con_Printf("\n");
		match = NULL;
	}

	if (match != NULL)
		return match->getName();
	else
		return NULL;
}

void Alias::printAliases(){
	list<Alias *>::iterator i;

	Con_Printf ("Current alias commands:\n");

	for (i = Aliases.begin(); i != Aliases.end(); i++){
		Alias *alias = *i;

		Con_Printf ("%s : %s\n", alias->getName(), alias->getCmdString());
	}
}

bool Alias::consoleCommand(){
	Alias *alias = Alias::findAlias(CmdArgs::CmdArgs::getArg(0));

	if (alias){
		Cbuf_InsertText (alias->getCmdString());
		return true;
	}
	return false;
}

qboolean	cmd_wait;

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
	cmd_wait = true;
}

/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

sizebuf_t	cmd_text;

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	SZ_Alloc (&cmd_text, 8192);		// space for commands and script files
}


/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (const char *text)
{
	int		l;

	l = Q_strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Con_Printf ("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write (&cmd_text, text, Q_strlen (text));
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (const char *text)
{
	char	*temp;
	int		templen;

// copy off any commands still remaining in the exec buffer
	templen = cmd_text.cursize;
	if (templen){
		temp = (char *)MemoryObj::ZAlloc (templen);
		Q_memcpy (temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);
	}
	else
		temp = NULL;	// shut up compiler

// add the entire text of the file
	Cbuf_AddText (text);

// add the copied off data
	if (templen)
	{
		SZ_Write (&cmd_text, temp, templen);
		MemoryObj::ZFree(temp);
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int		i;
	char	*text;
	char	line[1024];
	int		quotes;

	while (cmd_text.cursize)
	{
// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i=0 ; i< cmd_text.cursize ; i++)
		{
			if (text[i] == '"')
				quotes++;
			if ( !(quotes&1) &&  text[i] == ';')
				break;	// don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}


		Q_memcpy (line, text, i);
		line[i] = 0;

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec, alias) can insert data at the
// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			Q_memcpy (text, text+i, cmd_text.cursize);
		}

// execute the command line
		CmdArgs::executeString(line, CmdArgs::COMMAND);

		if (cmd_wait)
		{	// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds_f (void)
{
	int		i, j;
	int		s;
	char	*text, *build, c;

	if (CmdArgs::getArgCount () != 1)
	{
		Con_Printf ("stuffcmds : execute command line parameters\n");
		return;
	}

// build the combined string to parse from
	s = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		s += Q_strlen (com_argv[i]) + 1;
	}
	if (!s)
		return;

	text = (char *)MemoryObj::ZAlloc (s+1);
	text[0] = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		Q_strcat (text,com_argv[i]);
		if (i != com_argc-1)
			Q_strcat (text, " ");
	}

// pull out the commands
	build = (char *)MemoryObj::ZAlloc (s+1);
	build[0] = 0;

	for (i=0 ; i<s-1 ; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j=i ; (text[j] != '+') && (text[j] != '-') && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;

			Q_strcat (build, text+i);
			Q_strcat (build, "\n");
			text[j] = c;
			i = j-1;
		}
	}

	if (build[0])
		Cbuf_InsertText (build);

	MemoryObj::ZFree (text);
	MemoryObj::ZFree (build);
}


/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
	char	*f;
	int		mark;

	if (CmdArgs::CmdArgs::getArgCount () != 2)
	{
		Con_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	mark = Hunk_LowMark ();
	f = (char *)COM_LoadHunkFile (CmdArgs::CmdArgs::getArg(1));
	if (!f)
	{
		Con_Printf ("couldn't exec %s\n",CmdArgs::CmdArgs::getArg(1));
		return;
	}
	Con_Printf ("execing %s\n",CmdArgs::CmdArgs::getArg(1));

	Cbuf_InsertText (f);
	Hunk_FreeToLowMark (mark);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	int		i;

	for (i=1 ; i<CmdArgs::CmdArgs::getArgCount() ; i++)
		Con_Printf ("%s ",CmdArgs::CmdArgs::getArg(i));
	Con_Printf ("\n");
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
void Cmd_Alias_f (void)
{
	char		cmd[1024];
	int			i, maxArgs;
	const char	*aliasName;

	Q_memset(cmd,0,1024);

	if (CmdArgs::CmdArgs::getArgCount() == 1){
		Alias::printAliases();
		return;
	}

	aliasName = CmdArgs::CmdArgs::getArg(1);

	// if the alias already exists, delete the old one
	Alias *a = Alias::findAlias(aliasName);
	if (a){
		Alias::removeAlias(a);
	}

// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	maxArgs = CmdArgs::CmdArgs::getArgCount();
	for (i=2 ; i< maxArgs ; i++)
	{
		Q_strcat (cmd, CmdArgs::CmdArgs::getArg(i));
		if (i != maxArgs)
			Q_strcat (cmd, " ");
	}
	Q_strcat (cmd, "\n");

	//Create an alias
	Alias::addAlias(aliasName,cmd);
}

/*
=============================================================================
					COMMAND EXECUTION
=============================================================================
*/

/**
 * Cmd_Init
 */
void Cmd_Init (void) {
//
// register our commands
//
	Cmd::addCmd("stuffcmds",Cmd_StuffCmds_f);
	Cmd::addCmd("exec",Cmd_Exec_f);
	Cmd::addCmd("echo",Cmd_Echo_f);
	Cmd::addCmd("alias",Cmd_Alias_f);
	Cmd::addCmd("cmd", CmdArgs::forwardToServer);
	Cmd::addCmd("wait", Cmd_Wait_f);
}

char *CmdArgs::cmd_null_string = "";
char *CmdArgs::argv [80] = {};
char *CmdArgs::cmd_args = NULL;
int CmdArgs::argumentCount = 0;
CmdArgs::Source CmdArgs::cmd_source = COMMAND;

int CmdArgs::getArgCount(void){
	return argumentCount;
}

char *CmdArgs::getArg(int arg){
	if (arg >= argumentCount )
		return cmd_null_string;
	return argv[arg];
}

char *CmdArgs::Cmd_Args(){
	return cmd_args;
}

CmdArgs::Source CmdArgs::getSource(){
	return cmd_source;
}

void CmdArgs::setSource(Source src){
	cmd_source = src;
}

/**
 * Parses the given string into command line tokens.
 */
void CmdArgs::tokenizeString(char *text){
	int		i;

// clear the args from the last string
	for (i=0 ; i<argumentCount ; i++)
		MemoryObj::ZFree (getArg(i));

	argumentCount = 0;
	cmd_args = NULL;

	while (true){
		// skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\n'){
			text++;
		}

		// a newline separates commands in the buffer
		if (*text == '\n'){
			text++;
			break;
		}

		// if we have reached the end of the text, return
		if (!*text)
			return;

		if (argumentCount == 1)
			 cmd_args = text;

		text = COM_Parse(text);
		if (!text)
			return;

		if (argumentCount < maxArgs){
			argv[argumentCount] = (char *)MemoryObj::ZAlloc (Q_strlen(com_token)+1);
			Q_strcpy (argv[argumentCount], com_token);
			argumentCount++;
		}
	}

}

/**
 * A complete command line has been parsed, so try to execute it
 */
void CmdArgs::executeString(char *text, Source src)
{
	setSource(src);
	tokenizeString(text);

	// execute the command line
	if (!CmdArgs::getArgCount())
		return;		// no tokens

	// check commands
	if (Cmd::consoleCommand())
		return;

	// check aliases
	if (Alias::consoleCommand())
		return;

	// check cvar
	if (!CVar::consoleCommand())
		Con_Printf ("Unknown command \"%s\"\n", getArg(0));
}


/**
 * Sends the entire command line over to the server
 */
void CmdArgs::forwardToServer(){
	if (cls.state != ca_connected)
	{
		Con_Printf ("Can't \"%s\", not connected\n", getArg(0));
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

	MSG_WriteByte (&cls.message, clc_stringcmd);
	if (Q_strcasecmp(getArg(0), "cmd") != 0)
	{
		SZ_Print (&cls.message, getArg(0));
		SZ_Print (&cls.message, " ");
	}
	if (getArgCount() > 1)
		SZ_Print (&cls.message, CmdArgs::Cmd_Args());
	else
		SZ_Print (&cls.message, "\n");
}

/**
 * Returns the position (1 to argc-1) in the command's argument list where the
 *  given parameter appears, or 0 if not present
 */
int CmdArgs::checkParm (char *parm){
	int i;

	if (!parm)
		Sys_Error ("Cmd_CheckParm: NULL");

	for (i = 1; i < CmdArgs::getArgCount(); i++)
		if (! Q_strcasecmp (parm, CmdArgs::getArg (i)))
			return i;

	return 0;
}
