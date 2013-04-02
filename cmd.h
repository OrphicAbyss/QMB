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

// cmd.h -- Command buffer and command execution
//===========================================================================

/*
Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();
*/

/**
 * allocates an initial text buffer that will grow as needed
 */
void Cbuf_Init (void);

/**
 * as new commands are generated from the console or keybindings,
 * the text is added to the end of the command buffer.
 */
void Cbuf_AddText (const char *text);

/**
 * when a command wants to issue other commands immediately, the text is
 * inserted at the beginning of the buffer, before any remaining unexecuted
 * commands.
 */
void Cbuf_InsertText (const char *text);

/**
 * Pulls off \n terminated lines of text from the command buffer and sends
 * them through CmdArgs::Cmd_ExecuteString.  Stops when the buffer is empty.
 * Normally called once per frame, but may be explicitly invoked.
 * Do not call inside a command function!
 */
void Cbuf_Execute (void);

//===========================================================================

/*
Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

Commands can come from three sources, but the handler functions may choose
to disallow the action or forward it to a remote server if the source is
not appropriate.
*/
typedef void (*xcommand_t) (void);

class Cmd {
private:
	char *name;
	xcommand_t func;
	Cmd(const char *name, xcommand_t func);
	~Cmd();
public:
	char *getName();
	void call();
	xcommand_t getFunction();

	static void addCmd(const char *name, xcommand_t func);
	static Cmd *findCmd(const char *name);
	static char *completeCommand(char *partial);
	static bool consoleCommand();
	/**
	 * Clear out used zone memory, so that we can look for leaking memory more
	 * easily.
     */
	static void shutdown();
};

class Alias {
private:
	char *name;
	char *cmdString;
	Alias(const char *name, const char *cmdString);
	~Alias();
public:
	char *getName();
	char *getCmdString();

	static void addAlias(const char *name, const char *value);
	static void removeAlias(Alias *alias);
	static Alias *findAlias(const char *name);
	static char *completeAlias(const char *partial);
	static void printAliases();
	static bool consoleCommand();
	/**
	 * Clear out used zone memory, so that we can look for leaking memory more
	 * easily.
     */
	static void shutdown();
};

class MemoryObj;

class CmdArgs {
public:
	enum Source { UNDEFINED, CLIENT, COMMAND };
private:
	static const int maxArgs = 80;
	static char *cmd_null_string;
	static MemoryObj *argvMem[maxArgs];
	static char *argv[maxArgs];
	static char *cmd_args;
	static int argumentCount;
	static Source cmd_source;

	static void tokenizeString(char *text);
public:
	static void executeString(char *text, Source src);
	static char *getArg(int index);
	static char *Cmd_Args();
	static int getArgCount();
	static void forwardToServer();
	static int checkParm (char *parm);
	static Source getSource();
	static void setSource(Source src);
};

void	Cmd_Init (void);
void	Cmd_Print (char *text);

