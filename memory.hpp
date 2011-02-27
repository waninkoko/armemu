/*
 * ARM9 emulator - Memory subsystem
 * 
 * Copyright (C) 2011 - Miguel Boton (Waninkoko)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#include <vector>
#include "types.h"

using namespace std;


/* Virtual space class */
class VSpace {
	/* Buffer */
	u8 *buffer;

public:
	/* Parameters */
	u32 vaddr;
	u32 size;

public:
	 VSpace(u32 vaddr, u32 size);
	~VSpace(void);

	/* Read functions */
	u8  Read8 (u32 address);
	u16 Read16(u32 address);
	u32 Read32(u32 address);

	/* Write functions */
	void Write8 (u32 address, u8  value);
	void Write16(u32 address, u16 value);
	void Write32(u32 address, u32 value);

	/* Memcpy functions */
	void Memcpy(u32 dst, void *src, u32 size);
	void Memcpy(void *dst, u32 src, u32 size);
};

/* Memory class */
class Memory {
	/* Virtual spaces */
	static vector<VSpace *> Spaces;

private:
	static VSpace * Find(u32 address);

public:
	/* Create/Destroy spaces */
	static bool Create (u32 vaddr, u32 size);
	static void Destroy(u32 vaddr);

	/* Read functions */
	static u8  Read8 (u32 address);
	static u16 Read16(u32 address);
	static u32 Read32(u32 address);

	/* Write functions */
	static void Write8 (u32 address, u8  value);
	static void Write16(u32 address, u16 value);
	static void Write32(u32 address, u32 value);

	/* Memcpy functions */
	static void Memcpy(u32 dst, void *src, u32 size);
	static void Memcpy(void *dst, u32 src, u32 size);
};

#endif /* __MEMORY_HPP__ */
