#ifndef CVAR_CPP_H
#define	CVAR_CPP_H

class CVar {
	char *name;
	char *sValue;
	bool archive;
	bool server;
	float fValue;
	int	iValue;
	bool bValue;
	bool registered;
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
	void set(float value);
	void set(bool value);
	char *getName();
	char *getString();
	bool getBool();
	int getInt();
	float getFloat();
	bool isArchived();
	bool isServer();

	static CVar *findCVar(const char *name);
	static CVar *findNextServerCVar (const char *name);
	static void registerCVar(CVar *variable);
	static void setValue(const char *name, const char *value);
	static float getFloatValue(char *name);
	static const char *getStringValue(char *name);
	static char *completeVariable(char *partial);
	static void writeVariables(FILE *f);
	static bool consoleCommand(void);
};

#endif	/* CVAR_CPP_H */

