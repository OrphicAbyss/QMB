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
/*
 memory allocation


H_??? The hunk manages the entire memory block given to quake.  It must be
contiguous.  Memory can be allocated from either the low or high end in a
stack fashion.  The only way memory is released is by resetting one of the
pointers.

Hunk allocations should be given a name, so the Hunk_Print () function
can display usage.

Hunk allocations are guaranteed to be 16 byte aligned.

The video buffers are allocated high to avoid leaving a hole underneath
server allocations when changing to a higher video mode.


Z_??? Zone memory functions used for small, dynamic allocations like text
strings from command input.  There is only about 48K for it, allocated at
the very bottom of the hunk.

Cache_??? Cache memory is for objects that can be dynamically loaded and
can usefully stay persistant between levels.  The size of the cache
fluctuates from level to level.

To allocate a cachable object


Temp_??? Temp memory is used for file loading and surface caching.  The size
of the cache memory is adjusted so that there is a minimum of 512k remaining
for temp memory.


------ Top of Memory -------

high hunk allocations

<--- high hunk reset point held by vid

video buffer

z buffer

surface cache

<--- high hunk used

cachable memory

<--- low hunk used

client and server low hunk allocations

<-- low hunk reset point held by host

startup hunk allocations

Zone block

----- Bottom of Memory -----



*/

void Memory_Init (void *buf, int size);

void *Hunk_Alloc (int size);		// returns 0 filled memory
void *Hunk_AllocName (int size, const char *name);

void *Hunk_HighAllocName (int size, const char *name);

int	Hunk_LowMark (void);
void Hunk_FreeToLowMark (int mark);

int	Hunk_HighMark (void);
void Hunk_FreeToHighMark (int mark);

void *Hunk_TempAlloc (int size);

void Hunk_Check (void);

/**
 * CacheObj represents allocated space for a cached object.
 *
 * TODO: Look into using a slotted memory system for Zone objects.
 */
class MemoryObj {
public:
	enum MemType { CACHE, ZONE, HUNK, TEMP };
private:
	static const int maxNameLength = 32;
	void *data;
	char name[maxNameLength];
	int size;
	MemType type;

	MemoryObj(MemType type, char *name, int size);
	~MemoryObj();
public:
	/**
     * @return Name of the memory object
     */
	char *getName();
	/**
     * @return A pointer to the memory data
     */
	void *getData();
	/**
     * @return Size of the allocated memory data
     */
	int getSize();
	/**
	 * Free the allocated memmory data
     */
	void freeData();
	/**
	 * Used to allocate cache memory
	 *
     * @param type Type of memory to allocate
     * @param name Name of the memory object
     * @param size Size of memory to allocate
     * @return Pointer to a memory object which includes the allocated memory
     */
	static MemoryObj *Alloc(MemType type, char *name, int size);
	/**
	 * Used to allocate zone memory.
	 *
	 * Note that zone memory doesn't use the MemoryObj wrappers. We use them
	 * internally to track the allocation and deallocation, but zone memory
	 * works more like malloc/free. This could be a problem when trying to move
	 * zone memory to reclaim lost space.
	 *
     * @param size Size of memory to allocate
     * @return Pointer to the allocated memory
     */
	static void *ZAlloc(int size);
	/**
	 * Deallocate the memory attached to the passed in MemoryObj.
	 *
     * @param obj The memory object to deallocate
     */
	static void Free(MemoryObj *obj);
	/**
	 * Deallocate the zone memory and the tracking object.
	 *
     * @param data Pointer to the memory allocated
     */
	static void ZFree(void *data);
	/**
	 * Not currently used, but in the future should be used to deallocate cache
	 * memory. Should not be used for Zone memory.
	 *
     * @param type Type of memory to flush.
     */
	static void Flush(MemType type);
	/**
	 * Print out all objects of the given memory type.
	 *
     * @param type Type of memory to print data for.
     */
	static void Print(MemType type);
	/**
	 * Print out a report of the amount of memory used by each type of memory.
     */
	static void Report();
};



