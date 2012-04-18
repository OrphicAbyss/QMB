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

#include <stdio.h>
#include <windows.h>

#include "java_vm.h"

#include "quakedef.h"

//function prototypes
void JVM_SetupCallback(JNIEnv* env);
jint JNICALL Con_fPrintf (FILE *empty, char *fmt, ...);	//not sure if this works, someone test it?

static jint JNICALL jvfprintf(FILE *f, const char *fmt, va_list args)
{
	static char buf[2048];
	int result;

	// if your compiler doesn't do "vsnprintf", then you 
	// could add a #define up above like
	//   #ifdef MY_COMPILER
	//      #define vsnprintf(a, b, c, d) vsprintf(a, c, d)
	//   #endif
	// and just hope that the buffer is big enough, or
	// remove it all together like the comment up above mentions
	result = sprintf(&buf[0], fmt, args);

	Con_Printf("JVM error:\n");
	Con_Printf("%s", buf);
	return result;
}

//JavaVM global vars
JavaVM* jvm;
JNIEnv* j_env;

jclass JVM_FindClass(char *name) {
	jclass temp;

	temp = (*j_env)->FindClass(j_env, name);
	if(temp == NULL)
	{
		Con_Printf("&cF00JVM_Error: cannot find class: '%s'&r\n", name);
	}
	return temp;
}

jmethodID JVM_GetStaticMethod(jclass clazz, char *name, char *sig){
	jmethodID ID;
	
	ID = (*j_env)->GetStaticMethodID(j_env, clazz, name, sig);
	if(ID == NULL)
	{
		Con_Printf("&cF00JVM_Error: cannot find static method: '%s' with sig '%s'&r\n", name, sig);
		
	}
	return ID;
}

jmethodID JVM_GetMethod(jclass clazz, char *name, char *sig){
	jmethodID ID;
	
	ID = (*j_env)->GetMethodID(j_env, clazz, name, sig);
	if(ID == NULL)
	{
		Con_Printf("&cF00JVM_Error: cannot find method: '%s' with sig '%s'&r\n", name, sig);
		
	}
	return ID;
}

jint (*pfnCreateVM)(JavaVM **pvm, void **penv, void *args) = NULL;

void *JNU_FindCreateJavaVM(char *vmlibpath)
{
    HINSTANCE hVM = LoadLibrary(vmlibpath);
    if (hVM == NULL) {
        return NULL;
    }
     
    return GetProcAddress(hVM, "JNI_CreateJavaVM");
}

JNIEnv* JVM_CreateVM() {
	JNIEnv* env;
	JavaVMInitArgs args;
	JavaVMOption options[2];
	int retval = 0;
	char classpath[128];
	
	pfnCreateVM = JNU_FindCreateJavaVM("c:\\program files\\java\\jdk1.5.0\\jre\\server\\jvm.dll");

	args.version = JNI_VERSION_1_4;
	sprintf(&classpath[0], "-Djava.class.path=%s", host_parms.basedir);
	options[0].optionString = &classpath[0];
	options[1].optionString = "vfprintf";
	options[1].extraInfo = jvfprintf;//Con_fPrintf;
	args.options = options;
	args.nOptions = 2;
	args.ignoreUnrecognized = JNI_TRUE;

	if(pfnCreateVM == NULL)
	{
		retval = JNI_CreateJavaVM(&jvm, (void **)&env, &args);
	} else {
		retval = pfnCreateVM(&jvm, (void**)&env, &args);
	}


	if(retval != 0)
	{
		Con_Printf("Error: cannot create JVM - return value:%i", retval);
		return NULL;
	}

	return env;
}

int JVM_InvokeStatic(char *class_name, char *method_name, char *method_sig, jobjectArray *array){
	jclass classtofind;
	jmethodID methodtofind;
	jthrowable exc;

	if (j_env == NULL) return false;
	
	classtofind = JVM_FindClass(class_name);
	if (classtofind == NULL) return false;

	methodtofind = JVM_GetStaticMethod(classtofind, method_name, method_sig);
	if (methodtofind == NULL) return false;

	(*j_env)->CallStaticVoidMethod(j_env, classtofind, methodtofind, &array);
	
	//check for exception after running
	exc = (*j_env)->ExceptionOccurred(j_env);
	if (exc){
		(*j_env)->ExceptionDescribe(j_env);
		(*j_env)->ExceptionClear(j_env);
	}

	return true;
}

void JVM_InvokeClass() {
	jobjectArray applicationArgs;
	jstring applicationArg0;

	applicationArgs = (*j_env)->NewObjectArray(j_env, 1, JVM_FindClass("java/lang/String"), NULL);
	applicationArg0 = (*j_env)->NewStringUTF(j_env, "");
	(*j_env)->SetObjectArrayElement(j_env, applicationArgs, 0, applicationArg0);

	JVM_InvokeStatic("Quake", "main", "([Ljava/lang/String;)V", &applicationArgs);
}

void JVM_DistroyJavaVM(void){
	//not sure why anyone would want to do this but... meh
	(*jvm)->DestroyJavaVM(jvm);
}

void JVM_Quake_f(void){
	JVM_InvokeClass();
}

void JVM_Init (void){
	j_env = JVM_CreateVM();
	JVM_SetupCallback(j_env);

	Cmd_AddCommand ("java", JVM_Quake_f);
}

#endif