#ifndef CVAR_CPP_H
#define	CVAR_CPP_H

class CVar {
	char *name;
	char *sValue;
	bool archive;
	bool server;
	float fValue;
	int	iValue;
private:
	void init(char *name,char *sValue,bool archive,bool server);
	void parseValue();
	static void addCVar(CVar *cvar);
public:
	CVar(char *name, char *sValue);
	CVar(char *name, char *sValue, bool archive);
	CVar(char *name, char *sValue, bool archive, bool server);

	void reg();
	void set(const char *value);
	char *getName();
	char *getString();
	int getInt();
	float getFloat();
	bool isArchived();

	static CVar *findCVar(char *name);
	static void registerCVar(CVar *variable);
	static void setValue(char *name, char *value);
	static float getFloatValue(char *name);
	static const char *getStringValue(char *name);
	static char *completeVariable(char *partial);
	static void writeVariables(FILE *f);
	static bool consoleCommand(void);
};

#endif	/* CVAR_CPP_H */

