
#ifndef QTYPES_H
#define QTYPES_H

typedef unsigned char qbyte;

#ifndef NULL
#define NULL ((void *)0)
#endif

//define	PARANOID			// speed sapping error checking
#ifdef _DEBUG
#define ASSERT(condition) if (!(condition)) Sys_Error("assertion (##condition) failed at " __FILE__ ":" __LINE__ "\n");
#else
#define ASSERT(condition)
#endif

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2

#endif
