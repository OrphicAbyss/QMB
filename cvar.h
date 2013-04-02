#ifndef CVAR_CPP_H
#define	CVAR_CPP_H

class CVar {
private:
	const char *name;
	const char *originalValue;
	char *sValue;
	bool archive;
	bool server;
	float fValue;
	int	iValue;
	bool bValue;
	bool registered;
	/**
	 * Initialises the cvar data (called by the constructors).
	 *
	 * @param name Name of the cvar
	 * @param sValue String value of the cvar
	 * @param archive True if the cvar is saved to configs
	 * @param server True if the cvar is sent from server to all clients when
	 *				  changed
	 */
	void init(const char *name, const char *sValue, bool archive, bool server);
	/**
	 * Parse the string value of the cvar into float, int and bool values.
     */
	void parseValue();
	/**
	 * Add the cvar to the list of cvars.
	 *
     * @param cvar cvar to add
     */
	static void addCVar(CVar *cvar);
public:
	CVar(const char *name, const char *sValue);
	CVar(const char *name, const char *sValue, bool archive);
	CVar(const char *name, const char *sValue, bool archive, bool server);
	/**
	 * Register the cvar so it can be looked up based on it's name.
     */
	void reg();
	/**
	 * Unregister the cvar.
     */
	void unreg();
	/**
	 * Set the value of the cvar.
	 *
     * @param value to set the cvar to
     */
	void set(const char *value);
	/**
	 * Set the value of the cvar.
	 *
     * @param value to set the cvar to
     */
	void set(float value);
	/**
	 * Set the value of the cvar.
	 *
     * @param value to set the cvar to
     */
	void set(bool value);
	/**
	 * Get the name of the cvar.
	 *
     * @return name of the cvar
     */
	const char *getName(void);
	/**
	 * Gets the string value of the cvar
	 *
     * @return the value of the cvar
     */
	char *getString(void);
	/**
	 * Gets the bool value of the cvar. Based off the int value of the cvar.
	 *
     * @return bool value of hte cvar
     */
	bool getBool(void);
	/**
	 * Gets the value of the cvar parsed to an int.
	 *
     * @return int value of the cvar
     */
	int getInt(void);
	/**
	 * Gets the value of the cvar parsed to a float.
	 *
     * @return float value of the cvar
     */
	float getFloat(void);
	/**
     * @return true if the cvar value is saved in a config file
     */
	bool isArchived(void);
	/**
     * @return true if the cvar value will be sent to clients if updated
     */
	bool isServer(void);
	/**
	 * Add a cvar to the list of cvars
	 *
	 * @param cvar The cvar to add
	 */
	static CVar *findCVar(const char *name);
	/**
	 * Search the list of cvars for one with the given name
	 *
	 * @param name The cvar name to search for
	 * @return The cvar object, or null if none found
	 */
	static CVar *findNextServerCVar (const char *name);
	/**
	 * Register a cvar by adding it to our list of active cvars. Ensures that there
	 * isn't already a cvar or command by that name.
	 *
	 * @param variable
	 */
	static void registerCVar(CVar *variable);
	/**
	 * Set a cvar value given the name of the cvar and the value to set it to.
	 *
	 * @param var_name The name of the cvar to set
	 * @param value The value to set the cvar to
	 */
	static void setValue(const char *name, const char *value);
	/**
	 * Get the float value of a cvar for the name passed in.
	 *
	 * @param name The name of the cvar
	 * @return Either the value of the cvar or 0 if the cvar doesn't exist or if it
	 *			doesn't have a float value (ie is a string value)
	 */
	static float getFloatValue(char *name);
	/**
	 * Get the string value of a cvar for the name passed in.
	 *
	 * @param name
	 * @return
	 */
	static const char *getStringValue(char *name);
	/**
	 * Attempt to auto-complete a cvar name given the partial value. If there are
	 * multiple matches, print the matched names and return NULL.
	 *
	 * @param partial The partial name to match against
	 * @return the fullname of the cvar, NULL if none match or if more than one match
	 */
	static const char *completeVariable(char *partial);
	/**
	 * Handles variable inspection and changing from the console
	 *
	 * @return true if the variable was found, false otherwise
	 */
	static bool consoleCommand(void);
	/**
	 * Writes lines containing "set variable value" for all variables with the
	 *  archive flag set to true.
	 *
	 * @param f File to write the variables to
	 */
	static void writeVariables(FILE *f);

	/**
	 * Clear out used zone memory, so that we can look for leaking memory more
	 * easily.
     */
	static void shutdown();
};

#endif	/* CVAR_CPP_H */

