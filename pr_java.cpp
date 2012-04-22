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

#include <stdio.h>
#include <jni.h>
#include <windows.h>

#include "quakedef.h"

//include native callback header
//#include "java/client.h"

//javaVM pointer
JavaVM* jvm;
JNIEnv* j_env;

//for java vm errors, not sure if it works
jint JNICALL Con_fPrintf (FILE *empty, char *fmt, ...);

//callback to print text to the console
void JNICALL Con_PrintJava (JNIEnv *env, jclass this, jstring str){
	//get string and pass it on
	const jbyte *cstr;
	cstr = (*env)->GetStringUTFChars(env, str, NULL);
	if (cstr == NULL) {
		return; /* OutOfMemoryError already thrown */
	}

	Con_Printf("%s", cstr);

	(*env)->ReleaseStringUTFChars(env, str, cstr);
}

JNIEnv* JVM_CreateVM() {
	JNIEnv* env;
	JavaVMInitArgs args;
	JavaVMOption options[2];
	int retval = 0;
	char classpath[128];
	
	args.version = JNI_VERSION_1_4;
	sprintf(&classpath[0], "-Djava.class.path=%s", host_parms.basedir);
	options[0].optionString = &classpath[0];
	options[1].optionString = "vfprintf";
	options[1].extraInfo = &Con_fPrintf;
	args.options = options;
	args.nOptions = 2;
	args.ignoreUnrecognized = JNI_TRUE;
	retval = JNI_CreateJavaVM(&jvm, (void **)&env, &args);
	if(retval != 0)
	{
		Con_Printf("Error: cannot create JVM - return value:%i", retval);
		return NULL;
	}

	return env;
}

void JVM_SetupCallback(JNIEnv* env) {
	jclass client;
	JNINativeMethod nm;
	jint ret;

	if(env == NULL) return;

	client = (*env)->FindClass(env, "Quake");
	if(client == NULL)
	{
		Con_Printf("Error: cannot find class\n");
		return;
	}

	//setup callback functions here
	nm.name = "conPrint";
	nm.signature = "(Ljava/lang/String;)V";
	nm.fnPtr = Con_PrintJava;
	ret = (*env)->RegisterNatives(env, client, &nm, 1);
	if(ret != 0)
	{
		Con_Printf("Error: cannot find method: '%s' with args '%s' error num: %i\n", nm.name, nm.signature, ret);
		return;
	}
}

void JVM_InvokeClass(JNIEnv* env) {
	jclass client;
	jmethodID mainMethod;
	jobjectArray applicationArgs;
	jstring applicationArg0;

	if(env == NULL) return;

	client = (*env)->FindClass(env, "Quake");
	if(client == NULL)
	{
		Con_Printf("Error: cannot find class\n");
		return;
	}

	mainMethod = (*env)->GetStaticMethodID(env, client, "main", "([Ljava/lang/String;)V");
	if(mainMethod == NULL)
	{
		Con_Printf("Error: cannot find method: 'main'\n");
		return;
	}

	applicationArgs = (*env)->NewObjectArray(env, 1, (*env)->FindClass(env, "java/lang/String"), NULL);
	applicationArg0 = (*env)->NewStringUTF(env, "From-C-program");
	(*env)->SetObjectArrayElement(env, applicationArgs, 0, applicationArg0);
	(*env)->CallStaticVoidMethod(env, client, mainMethod, applicationArgs);


}

void JVM_DistroyJavaVM(void){
	//not sure why anyone would want to do this but... meh
	(*jvm)->DestroyJavaVM(jvm);
}

void JVM_Quake_f(void){
	JVM_InvokeClass(j_env);
}

void JVM_Init (void){
	j_env = JVM_CreateVM();
	JVM_SetupCallback(j_env);

	Cmd::addCmd("java", JVM_Quake_f);
}