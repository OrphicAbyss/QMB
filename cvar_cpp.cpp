//extern "C" {
//#define cpp 1
#include "quakedef.h"
//#undef cpp
//}

#include "cvar_cpp.h"
#include <list>

static std::list<CVar *> CVars;

void clearCVars(void){
	std::list<CVar *>::iterator i = CVars.begin();

	while (i != CVars.end()){
		delete (*i);
		i = CVars.erase(i);
	}
}

/**
 * Add a cvar to the list of cvars
 *
 * @param cvar The cvar to add
 */
void CVar::addCVar(CVar *cvar){
	CVars.push_back(cvar);
}

/**
 * Search the list of cvars for one with the given name
 *
 * @param name The cvar name to search for
 * @return The cvar object, or null if none found
 */
CVar *CVar::findCVar (char *name){
	std::list<CVar *>::iterator i;

	for (i = CVars.begin(); i != CVars.end(); ){
		CVar *cvar = *i;

		if (Q_strcmp(cvar->getString(),name)){
			return cvar;
		}
	}

	return NULL;
}

/**
 * Register a cvar by adding it to our list of active cvars. Ensures that there
 * isn't already a cvar or command by that name.
 *
 * @param variable
 */
void CVar::registerCVar(CVar* variable){
// first check to see if it has allready been defined
	if (Cvar_FindVar (variable->getName())) {
		Con_Printf ("Can't register variable %s, allready defined\n", variable->name);
		return;
	}

// check for overlap with a command
	if (Cmd_Exists (variable->getName())){
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}

	// assign space for the string
	variable->reg();

// link the variable in
	addCVar(variable);
}

/**
 * Set a cvar value given the name of the cvar and the value to set it to.
 *
 * @param var_name The name of the cvar to set
 * @param value The value to set the cvar to
 */
void CVar::setValue(char *var_name, char *value){
	CVar *var = CVar::findCVar(var_name);

	if (var != NULL){
		var->set(value);
	}
}

/**
 * Get the float value of a cvar for the name passed in.
 *
 * @param name The name of the cvar
 * @return Either the value of the cvar or 0 if the cvar doesn't exist or if it
 *			doesn't have a float value (ie is a string value)
 */
float CVar::getFloatValue(char *name){
	CVar *var = CVar::findCVar(name);
	if (var != NULL)
		return var->getFloat();
	return 0;
}

/**
 * Get the string value of a cvar for the name passed in.
 *
 * @param name
 * @return
 */
const char *CVar::getStringValue(char *name){
	CVar *var = CVar::findCVar(name);
	if (var != NULL)
		return var->getString();
	return "";
}

/**
 * Attempt to auto-complete a cvar name given the partial value. If there are
 * multiple matches, print the matched names and return NULL.
 *
 * @param partial The partial name to match against
 * @return the fullname of the cvar, NULL if none match or if more than one match
 */
char *CVar::completeVariable(char *partial){
	std::list<CVar *>::iterator i;
	CVar *match = NULL;
	int sizeOfStr = Q_strlen(partial);
	bool multiple = false;

	for (i = CVars.begin(); i != CVars.end() && !multiple; ){
		CVar *cvar = *i;

		if (Q_strncmp(cvar->getString(),partial,sizeOfStr)){
			if (match != NULL){
				multiple = true;
			} else {
				match = cvar;
			}
		}
	}

	if (multiple){
		bool first = true;
		for (i = CVars.begin(); i != CVars.end(); ){
			CVar *cvar = *i;

			if (Q_strncmp(cvar->getString(),partial,sizeOfStr)){
				if (first){
					first = false;
					Con_Printf(cvar->getString());
				} else {
					Con_Printf(", %s",cvar->getString());
				}
			}
		}
		Con_Printf("\n");
		match = NULL;
	}

	return match->getString();
}

/**
 * Handles variable inspection and changing from the console
 *
 * @return true if the variable was found, false otherwise
 */
bool CVar::consoleCommand(void){
	CVar *v;

// check variables
	v = CVar::findCVar(Cmd_Argv(0));
	if (!v)
		return false;

// perform a variable print or set
	if (Cmd_Argc() == 1){
		Con_Printf ("\"%s\" is \"%s\"\n", v->getName(), v->getString());
	} else {
		v->set(Cmd_Argv(1));
	}
	return true;
}

/**
 * Writes lines containing "set variable value" for all variables with the
 *  archive flag set to true.
 *
 * @param f File to write the variables to
 */
void CVar::writeVariables (FILE *f)
{
	std::list<CVar *>::iterator i;

	for (i = CVars.begin(); i != CVars.end(); ){
		CVar *cvar = *i;

		if (cvar->isArchived())
			fprintf (f, "%s \"%s\"\n", cvar->getName(), cvar->getString());
	}
}

CVar::CVar(char *name, char *sValue){
	init(name, sValue, false, false);
}

CVar::CVar(char *name, char *sValue, bool archive){
	init(name, sValue, archive, false);
}

CVar::CVar(char *name, char *sValue, bool archive, bool server){
	init(name, sValue, archive, server);
}

/**
 * Initialises the cvar data (called by the constructors).
 *
 * @param name Name of the cvar
 * @param sValue String value of the cvar
 * @param archive True if the cvar is saved to configs
 * @param server True if the cvar is sent from server to all clients when
 *				  changed
 */
void CVar::init(char *name,char *sValue,bool archive,bool server){
	this->name = name;
	this->archive = archive;
	this->server = server;

	// use this until we register then make a copy of the string
	this->sValue = sValue;
	this->fValue = 0;
	this->iValue = 0;

	parseValue();
}

/**
 * Make a copy of the string into temp memory
 */
void CVar::reg(){
	char *value = this->sValue;
	this->sValue = (char *)Z_Malloc (Q_strlen(value)+1);
	Q_strcpy (this->sValue, value);
}

/**
 * Parse the string value into the float and integer value fields
 */
void CVar::parseValue(){
	this->fValue = Q_atof(this->sValue);
	this->iValue = Q_atoi(this->sValue);
}

/**
 * Set the string value of the CVar
 *
 * @param value The new string value
 */
void CVar::set(const char *value){
	bool changed;

	changed = Q_strcmp(this->sValue, value);

	// Allocate new space for the string
	Z_Free (this->sValue);
	this->sValue = (char *)Z_Malloc (Q_strlen(value)+1);
	Q_strcpy (this->sValue, value);
	// Convert the value to a float
	this->parseValue();

	if (this->server && changed) {
		if (sv.active)
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", this->name, this->sValue);
	}
}

char *CVar::getName(){
	return this->name;
}

char *CVar::getString(){
	return this->sValue;
}

int CVar::getInt(){
	return this->iValue;
}

float CVar::getFloat(){
	return this->fValue;
}

bool CVar::isArchived(){
	return this->archive;
}