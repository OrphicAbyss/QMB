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

#ifdef JAVA

#include "java_vm.h"

#include "quakedef.h"

char *JavaStringToCString(jstring str){
	const jbyte *s;

	s = (*j_env)->GetStringUTFChars(j_env, str, NULL);
	if (s == NULL){
		Con_DPrintf("Java Error: out of memeory");
		return NULL; // OutOfMemoryError already thrown
	}
	return (char *)s;
}

void FreeString(jstring str, const jbyte *s){
	(*j_env)->ReleaseStringUTFChars(j_env, str, s);
}

/*
=================
Java_bPrint

broadcast print to everyone on server
=================
*/
void JNICALL Java_bPrint(JNIEnv *env, jclass this, jstring str)
{
	const jbyte	*s;

	s = JavaStringToCString(str);
	SV_BroadcastPrintf ("%s", s);
	FreeString(str, s);
}

/*
=================
Java_cPrint

center print to client
=================
*/
void JNICALL Java_cPrint(JNIEnv *env, jclass this, jstring str, jint client)
{
	const jbyte	*s;
	
	//qmb :globot
	edict_t		*ent;

	Con_Printf("Java: center print\n");

	ent = G_EDICT(client+1);
	if (!ent->bot.isbot)
	{
		s = JavaStringToCString(str);

		if (client < 0 || client >= svs.maxclients)
		{
			Con_Printf ("Java Quake: tried to sprint to a non-client\n");
			return;
		}
		
		MSG_WriteChar (&svs.clients[client].message, svc_centerprint);
		MSG_WriteString (&svs.clients[client].message, (char *)s);

		FreeString(str, s);
	}
}

/*
=================
Java_sPrint

sprint to client
=================
*/
void JNICALL Java_sPrint(JNIEnv *env, jclass this, jstring str, jint client)
{
	const jbyte	*s;
	
	//qmb :globot
	edict_t		*ent;

	Con_Printf("Java: s print\n");

	ent = G_EDICT(client+1);
	if (!ent->bot.isbot)
	{
		s = JavaStringToCString(str);

		if (client < 0 || client >= svs.maxclients)
		{
			Con_Printf ("Java Quake: tried to sprint to a non-client\n");
			return;
		}
			
		MSG_WriteChar (&svs.clients[client].message, svc_print);
		MSG_WriteString (&svs.clients[client].message, (char *)s);

		FreeString(str, s);
	}
}

/*
=================
Java_conPrint

print to console on server
=================
*/
void JNICALL Java_conPrint (JNIEnv *env, jclass this, jstring str){
	//get string and pass it on
	const jbyte *s;

	s = JavaStringToCString(str);
	Con_Printf("%s", s);
	FreeString(str, s);
}

/*
=================
Java_conPrint

print to console on server
=================
*/
void JNICALL Java_condPrint (JNIEnv *env, jclass this, jstring str){
	//get string and pass it on
	const jbyte *s;

	s = JavaStringToCString(str);
	Con_DPrintf("%s", s);
	FreeString(str, s);
}

/*
=================
Java_cvar

float cvar (string)
=================
*/
jfloat JNICALL Java_getCVar (JNIEnv *env, jclass this, jstring CVar)
{
	const jbyte *cvar_name;
	jfloat value;

	cvar_name = JavaStringToCString(CVar);

	Con_Printf("getting cvar: %s\n", cvar_name);

	value = Cvar_VariableValue ((char *)cvar_name);
	FreeString(CVar, cvar_name);

	return value;
}

/*
=================
Java_cvar_set

float cvar (string)
=================
*/
void JNICALL Java_setCVar (JNIEnv *env, jclass this, jstring CVar, jstring Value)
{
	const jbyte *cvar_name, *cvar_value;
	
	cvar_name = JavaStringToCString(CVar);
	cvar_value = JavaStringToCString(Value);

	Con_Printf("settting cvar: %s\n", cvar_name);

	Cvar_Set((char *)cvar_name, (char *)cvar_value);

	FreeString(CVar, cvar_name);
	FreeString(Value, cvar_value);
}

//java particle stuff
extern void Java_DrawParticle(JNIEnv *env, jclass this, float x, float y, float z, float size, float r, float g, float b, float a, float rotation);

void Java_glTexCoord2f(JNIEnv *env, jclass this, jfloat s, jfloat t){
	glTexCoord2f(s,t);
}

void Java_glVertex3f(JNIEnv *env, jclass this, jfloat x, jfloat y, jfloat z){
	glVertex3f(x,y,z);
}

//list of native methods: method name, method signiture, native function pointer
JNINativeMethod nm[] = {
"conPrint",		"(Ljava/lang/String;)V",					Java_conPrint,
"condPrint",	"(Ljava/lang/String;)V",					Java_condPrint,
"bPrint",		"(Ljava/lang/String;)V",					Java_bPrint,
"cPrint",		"(Ljava/lang/String;I)V",					Java_cPrint,
"sPrint",		"(Ljava/lang/String;I)V",					Java_sPrint,
"getCVar",		"(Ljava/lang/String;)F",					Java_getCVar,
"setCVar",		"(Ljava/lang/String;Ljava/lang/String;)V",	Java_setCVar,
"glTexCoord2f",	"(FF)V",									Java_glTexCoord2f,
"glVertex3f",	"(FFF)V",									Java_glVertex3f,
"drawParticle",	"(FFFFFFFFF)V",								Java_DrawParticle,
"","",NULL	//MUST BE AT END, used as a marker to see how many functions to add
};

void JVM_SetupCallback(JNIEnv* env) {
	jclass quake;
	jint ret;
	int numMethods;

	if(env == NULL) return;

	quake = (*env)->FindClass(env, "Quake");
	if(quake == NULL)
	{
		Con_Printf("Error: cannot find class\n");
		return;
	}

	//work out the number of native methods to add
	//assumes at least one function
	for (numMethods = 1; nm[numMethods].fnPtr != NULL; numMethods++);

	Con_Printf("Java: Registering %i callback functions\n", numMethods);
	ret = (*env)->RegisterNatives(env, quake, &nm[0], numMethods);
	if(ret != 0)
	{
		Con_Printf("Error: cannot find method, error: %i\n", ret);
		return;
	}
}

#endif