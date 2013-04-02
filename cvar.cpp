#include "quakedef.h"

#include <list>

using std::list;

static list<CVar *> CVars;

void CVar::addCVar(CVar *cvar) {
	CVars.push_back(cvar);
}

CVar *CVar::findCVar (const char *name) {
	list<CVar *>::iterator i;

	for (i = CVars.begin(); i != CVars.end(); i++) {
		CVar *cvar = *i;

		if (0 == Q_strcmp(cvar->getName(),name)) {
			return cvar;
		}
	}

	return NULL;
}

void CVar::shutdown() {
	list<CVar *>::iterator i;

	for (i = CVars.begin(); i != CVars.end(); i++) {
		CVar *cvar = *i;

		cvar->unreg();
	}
}

CVar *CVar::findNextServerCVar (const char *name) {
	list<CVar *>::iterator i;
	bool found = false;

	if (name != NULL) {
		//Find the old cvar first
		for (i = CVars.begin(); i != CVars.end(); i++) {
			CVar *cvar = *i;

			if (0 == Q_strcmp(cvar->getName(),name)) {
				found = true;
			}
		}
	} else {
		// Start search for the server replicated CVars at start
		found = true;
		i = CVars.begin();
	}

	if (found){
		//Find the next server cvar
		for (; i != CVars.end(); i++) {
			CVar *cvar = *i;

			if (cvar->isServer()) {
				return cvar;
			}
		}
	}

	return NULL;
}

void CVar::registerCVar(CVar* variable) {
	// first check to see if it has already been defined
	if (CVar::findCVar(variable->getName())) {
		Con_Printf ("Can't register variable %s, allready defined\n", variable->name);
		return;
	}

	// check for overlap with a command
	if (Cmd::findCmd(variable->getName())) {
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}

	// assign space for the string
	variable->reg();

// link the variable in
	addCVar(variable);
}

void CVar::setValue(const char *var_name, const char *value) {
	CVar *var = CVar::findCVar(var_name);

	if (var != NULL) {
		var->set(value);
	}
}

float CVar::getFloatValue(char *name) {
	CVar *var = CVar::findCVar(name);
	if (var != NULL)
		return var->getFloat();
	return 0;
}

const char *CVar::getStringValue(char *name) {
	CVar *var = CVar::findCVar(name);
	if (var != NULL)
		return var->getString();
	return "";
}

const char *CVar::completeVariable(char *partial) {
	list<CVar *>::iterator i;
	CVar *match = NULL;
	int sizeOfStr = Q_strlen(partial);
	bool multiple = false;

	for (i = CVars.begin(); i != CVars.end() && !multiple; i++) {
		CVar *cvar = *i;

		if (0 == Q_strncmp(cvar->getName(),partial,sizeOfStr)) {
			if (match != NULL){
				multiple = true;
			} else {
				match = cvar;
			}
		}
	}

	if (multiple) {
		bool first = true;
		Con_Printf("CVars: ");
		for (i = CVars.begin(); i != CVars.end(); i++) {
			CVar *cvar = *i;

			if (0 == Q_strncmp(cvar->getName(),partial,sizeOfStr)) {
				if (first){
					first = false;
					Con_Printf(cvar->getName());
				} else {
					Con_Printf(", %s",cvar->getName());
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

bool CVar::consoleCommand(void) {
	CVar *var;

// check variables
	var = CVar::findCVar(CmdArgs::getArg(0));
	if (!var)
		return false;

// perform a variable print or set
	if (CmdArgs::getArgCount() == 1){
		Con_Printf ("\"%s\" is \"%s\"\n", var->getName(), var->getString());
	} else {
		var->set(CmdArgs::getArg(1));
	}
	return true;
}

void CVar::writeVariables (FILE *f) {
	list<CVar *>::iterator i;

	for (i = CVars.begin(); i != CVars.end(); i++) {
		CVar *cvar = *i;

		if (cvar->isArchived())
			fprintf (f, "%s \"%s\"\n", cvar->getName(), cvar->getString());
	}
}

CVar::CVar(const char *name, const char *sValue) {
	init(name, sValue, false, false);
}

CVar::CVar(const char *name, const char *sValue, bool archive) {
	init(name, sValue, archive, false);
}

CVar::CVar(const char *name, const char *sValue, bool archive, bool server) {
	init(name, sValue, archive, server);
}

void CVar::init(const char *name, const char *sValue, bool archive, bool server) {
	this->name = name;
	this->archive = archive;
	this->server = server;
	this->registered = false;

	// use this until we register then make a copy of the string
	this->originalValue = sValue;
	this->sValue = NULL;
	this->fValue = 0;
	this->iValue = 0;
	this->bValue = false;
}

/**
 * Make a copy of the string into temp memory
 */
void CVar::reg() {
	if (!this->registered){
		registered = true;
		const char *value = this->originalValue;
		this->sValue = (char *)MemoryObj::ZAlloc (Q_strlen(value)+1);
		Q_strcpy (this->sValue, value);
		this->originalValue = NULL;

		parseValue();
	}
}

/**
 * Clear out used memory
 */
void CVar::unreg() {
	MemoryObj::ZFree(this->sValue);
}

/**
 * Parse the string value into the float and integer value fields
 */
void CVar::parseValue() {
	this->fValue = Q_atof(this->sValue);
	this->iValue = Q_atoi(this->sValue);
	this->bValue = this->iValue != 0;
}

/**
 * Set the string value of the CVar
 *
 * @param value The new string value
 */
void CVar::set(const char *value) {
	if (this->sValue == NULL) {
		Con_Printf("No string value set for: %s\n",this->name);
	}

	bool changed = Q_strcmp(this->sValue, value);
	// If it's a new value
	if (changed != 0) {
		// Overwrite if it's the same length
		if (Q_strlen(this->sValue) == Q_strlen(value)){
			Q_strcpy(this->sValue, value);
		} else {
			// Allocate new space for the string
			MemoryObj::ZFree(this->sValue);
			this->sValue = (char *)MemoryObj::ZAlloc(Q_strlen(value)+1);
			Q_strcpy(this->sValue, value);
		}
		// Convert the value to a float
		this->parseValue();

		if (this->server && changed) {
			if (sv.active)
				SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", this->name, this->sValue);
		}
	}
}

void CVar::set(float value) {
	char strValue[32];

	snprintf(strValue,32,"%f",value);
	this->set(strValue);
}

void CVar::set(bool value) {
	if (value)
		this->set("1");
	else
		this->set("0");
}

const char *CVar::getName(void) {
	return this->name;
}

char *CVar::getString(void) {
	return this->sValue;
}

bool CVar::getBool(void) {
	return this->bValue;
}

int CVar::getInt(void) {
	return this->iValue;
}

float CVar::getFloat(void) {
	return this->fValue;
}

bool CVar::isArchived(void) {
	return this->archive;
}

bool CVar::isServer(void) {
	return this->server;
}
