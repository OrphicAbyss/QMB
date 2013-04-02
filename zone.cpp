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
// Z_zone.c

#include "quakedef.h"
#include <list>

//============================================================================

#define	HUNK_SENTINAL	0x1df001ed

typedef struct {
	int sentinal;
	int size; // including sizeof(hunk_t), -1 = not allocated
	char name[8];
} hunk_t;

byte *hunk_base;
int hunk_size;

int hunk_low_used;
int hunk_high_used;

bool hunk_tempactive;
int hunk_tempmark;

/*
==============
Hunk_Check

Run consistancy and sentinal trahing checks
==============
 */
void Hunk_Check(void) {
	hunk_t *h;

	for (h = (hunk_t *) hunk_base; (byte *) h != hunk_base + hunk_low_used;) {
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + (byte *) h - hunk_base > hunk_size)
			Sys_Error("Hunk_Check: bad size");
		h = (hunk_t *) ((byte *) h + h->size);
	}
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
 */
void Hunk_Print(bool all) {
	hunk_t *h, *next, *endlow, *starthigh, *endhigh;
	int count, sum;
	int totalblocks;
	char name[9];

	name[8] = 0;
	count = 0;
	sum = 0;
	totalblocks = 0;

	h = (hunk_t *) hunk_base;
	endlow = (hunk_t *) (hunk_base + hunk_low_used);
	starthigh = (hunk_t *) (hunk_base + hunk_size - hunk_high_used);
	endhigh = (hunk_t *) (hunk_base + hunk_size);

	Con_Printf("          :%8i total hunk size\n", hunk_size);
	Con_Printf("-------------------------\n");

	while (1) {
		//
		// skip to the high hunk if done with low hunk
		//
		if (h == endlow) {
			Con_Printf("-------------------------\n");
			Con_Printf("          :%8i REMAINING\n", hunk_size - hunk_low_used - hunk_high_used);
			Con_Printf("-------------------------\n");
			h = starthigh;
		}

		//
		// if totally done, break
		//
		if (h == endhigh)
			break;

		//
		// run consistancy checks
		//
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + (byte *) h - hunk_base > hunk_size)
			Sys_Error("Hunk_Check: bad size");

		next = (hunk_t *) ((byte *) h + h->size);
		count++;
		totalblocks++;
		sum += h->size;

		//
		// print the single block
		//
		memcpy(name, h->name, 8);
		if (all)
			Con_Printf("%8p :%8i %8s\n", h, h->size, name);

		//
		// print the total
		//
		if (next == endlow || next == endhigh ||
				strncmp(h->name, next->name, 8)) {
			if (!all)
				Con_Printf("          :%8i %8s (TOTAL)\n", sum, name);
			count = 0;
			sum = 0;
		}

		h = next;
	}

	Con_Printf("-------------------------\n");
	Con_Printf("%8i total blocks\n", totalblocks);

}

/*
===================
Hunk_AllocName
===================
 */
void *Hunk_AllocName(int size, const char *name) {
	hunk_t *h;

//#ifdef PARANOID
	Hunk_Check();
//#endif

	if (size < 0)
		Sys_Error("Hunk_Alloc: bad size: %i", size);

	size = sizeof (hunk_t) + ((size + 15)&~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
		Sys_Error("Hunk_Alloc: failed on %i bytes", size);

	h = (hunk_t *) (hunk_base + hunk_low_used);
	hunk_low_used += size;

	memset(h, 0, size);

	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	strncpy(h->name, name, 8);

	return (void *) (h + 1);
}

/*
===================
Hunk_Alloc
===================
 */
void *Hunk_Alloc(int size) {
	return Hunk_AllocName(size, "unknown");
}

int Hunk_LowMark(void) {
	return hunk_low_used;
}

void Hunk_FreeToLowMark(int mark) {
	if (mark < 0 || mark > hunk_low_used)
		Sys_Error("Hunk_FreeToLowMark: bad mark %i", mark);
	memset(hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;
}

int Hunk_HighMark(void) {
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark(hunk_tempmark);
	}

	return hunk_high_used;
}

void Hunk_FreeToHighMark(int mark) {
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark(hunk_tempmark);
	}
	if (mark < 0 || mark > hunk_high_used)
		Sys_Error("Hunk_FreeToHighMark: bad mark %i", mark);
	memset(hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
	hunk_high_used = mark;
}

/*
===================
Hunk_HighAllocName
===================
 */
void *Hunk_HighAllocName(int size, const char *name) {
	hunk_t *h;

	if (size < 0)
		Sys_Error("Hunk_HighAllocName: bad size: %i", size);

	if (hunk_tempactive) {
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = false;
	}

#ifdef PARANOID
	Hunk_Check();
#endif

	size = sizeof (hunk_t) + ((size + 15)&~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size) {
		Con_Printf("Hunk_HighAlloc: failed on %i bytes\n", size);
		return NULL;
	}

	hunk_high_used += size;

	h = (hunk_t *) (hunk_base + hunk_size - hunk_high_used);

	memset(h, 0, size);
	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	strncpy(h->name, name, 8);

	return (void *) (h + 1);
}

/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
=================
 */
void *Hunk_TempAlloc(int size) {
	void *buf;

	size = (size + 15)&~15;

	if (hunk_tempactive) {
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = false;
	}

	hunk_tempmark = Hunk_HighMark();

	buf = Hunk_HighAllocName(size, "temp");

	hunk_tempactive = true;

	return buf;
}

/*
===============================================================================
CACHE & ZONE MEMORY
===============================================================================
 */

MemoryObj::MemoryObj(MemType type, const char *name, int size) {
	memset(this->name, 0, maxNameLength);
	strncpy(this->name, name, maxNameLength);
	this->type = type;
	this->data = malloc(size);
	this->size = size;
}

MemoryObj::~MemoryObj() {
	if (data)
		free(data);

	data = NULL;
}

void MemoryObj::freeData() {
	if (data)
		free(data);

	data = NULL;
}

char *MemoryObj::getName() {
	return name;
}

void *MemoryObj::getData() {
	return data;
}

int MemoryObj::getSize() {
	return size;
}

using std::list;

static list<MemoryObj *> zoneObjects;
static list<MemoryObj *> cacheObjects;

MemoryObj *MemoryObj::Alloc(MemType type, const char* name, int size) {
	MemoryObj *obj = new MemoryObj(type, name, size);

	switch (type) {
		case ZONE:
			Con_DPrintf("Allocated Zone Object: %s (Size: %8d)\n", obj->getName(), obj->getSize());
			zoneObjects.push_back(obj);
			break;
		case CACHE:
			//Con_DPrintf ("Allocated Cache Object: %s (Size: %8d)\n", obj->getName(), obj->getSize());
			cacheObjects.push_back(obj);
			break;
		default:
			Con_Printf("Unknown memory type being allocated.");
			break;
	}

	return obj;
}

void *MemoryObj::ZAlloc(int size) {
	MemoryObj *obj = MemoryObj::Alloc(ZONE, "Zone", size);
	return obj->data;
}

void MemoryObj::Free(MemoryObj *obj) {
	obj->freeData();
}

void MemoryObj::ZFree(void *data) {
	list<MemoryObj *>::iterator start = zoneObjects.begin();
	list<MemoryObj *>::iterator finish = zoneObjects.end();
	list<MemoryObj *>::iterator i;
	bool found = false;

	for (i = start; i != finish && !found; i++) {
		MemoryObj *obj = *i;
		if (obj->data == data) {
			i = zoneObjects.erase(i);
			obj->freeData();
			delete obj;
			found = true;
		}
	}

	if (!found) {
		Con_Printf("ZONE: Unable to find memory object pointing to the allocated data.\n");
	}
}

void MemoryObj::Flush(MemType type) {
	list<MemoryObj *>::iterator start;
	list<MemoryObj *>::iterator finish;
	list<MemoryObj *>::iterator i;

	switch (type) {
		case ZONE:
			start = zoneObjects.begin();
			finish = zoneObjects.end();
			break;
		case CACHE:
			start = cacheObjects.begin();
			finish = cacheObjects.end();
			break;
		default:
			Con_Printf("Unknown memory type being allocated.");
			return;
	}

	for (i = start; i != finish; i++) {
		MemoryObj *obj = *i;
		obj->freeData();
	}
}

void MemoryObj::Print(MemType type) {
	list<MemoryObj *>::iterator start;
	list<MemoryObj *>::iterator finish;
	list<MemoryObj *>::iterator i;
	long size = 0;

	switch (type) {
		case ZONE:
			start = zoneObjects.begin();
			finish = zoneObjects.end();
			break;
		case CACHE:
			start = cacheObjects.begin();
			finish = cacheObjects.end();
			break;
		default:
			Con_Printf("Unknown memory type being allocated.");
			return;
	}

	Con_Printf("Cache Size Debug\n---------------------------\n");
	for (i = start; i != finish; i++) {
		MemoryObj *obj = *i;
		Con_Printf("%8d : %s\n", obj->getSize(), obj->getName());
		size += obj->getSize();
	}
	Con_Printf("---------------------------\nTotal size: %8i\n---------------------------\n", size);
}

void MemoryObj::Report() {
	list<MemoryObj *>::iterator i;
	int size = 0;
	for (i = cacheObjects.begin(); i != cacheObjects.end(); i++) {
		MemoryObj *obj = *i;
		size += obj->getSize();
	}
	Con_Printf("%4.1f megabyte data cache\n", size / (float) (1024 * 1024));

	size = 0;
	for (i = zoneObjects.begin(); i != zoneObjects.end(); i++) {
		MemoryObj *obj = *i;
		size += obj->getSize();
	}
	Con_Printf("%4.1f kilobyte data zone\n", size / (float) (1024));
}

/**
 * Cache_Flush
 *
 * Throw everything out, so new data will be demand cached
 */
void Cache_Flush(void) {
	MemoryObj::Flush(MemoryObj::CACHE);
}

/**
 * Cache_Print
 */
void Cache_Print(void) {
	MemoryObj::Print(MemoryObj::CACHE);
}

/**
 * Cache_Print
 */
void Zone_Print(void) {
	MemoryObj::Print(MemoryObj::ZONE);
}

/**
 * Cache_Report
 */
void Memory_Report(void) {
	MemoryObj::Report();
}

/**
 * Cache_Init
 */
void Cache_Init(void) {
	Cmd::addCmd("flush", Cache_Flush);
	Cmd::addCmd("cache_print", Cache_Print);
	Cmd::addCmd("zone_print", Zone_Print);
	Cmd::addCmd("memory_report", Memory_Report);
}

//============================================================================

/*
========================
Memory_Init
========================
 */
void Memory_Init(void *buf, int size) {
	int p;
	int zonesize = 0xc000;

	hunk_base = (byte *) buf;
	hunk_size = size;
	hunk_low_used = 0;
	hunk_high_used = 0;

	p = COM_CheckParm("-zone");
	if (p) {
		if (p < com_argc - 1)
			zonesize = atoi(com_argv[p + 1]) * 1024;
		else
			Sys_Error("Memory_Init: you must specify a size in KB after -zone");
	}

	Cache_Init();
}

