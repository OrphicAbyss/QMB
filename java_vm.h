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
#include <jni.h>

extern JNIEnv* j_env;

//get the class
jclass JVM_FindClass(char *name);
jmethodID JVM_GetStaticMethod(jclass clazz, char *name, char *sig);
jmethodID JVM_GetMethod(jclass clazz, char *name, char *sig);

//convert a java string to a c string, remember to free the string after use.
char *JavaStringToCString(jstring str);
void FreeString(jstring str, const jbyte *s);
#endif