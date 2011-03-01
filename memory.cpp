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

#include <iostream>
#include <fstream>
#include <cstring>
#include <elf.h>

#include "endian.h"
#include "memory.hpp"
#include "utils.hpp"


/*
 * Virtual space class
 */

VSpace::VSpace(u32 address, u32 size)
{
	/* Allocate buffer */
	buffer = new u8[size];

	/* Initialize buffer */
	if (buffer)
		memset(buffer, 0xFF, size);

	/* Set parameters */
	this->vaddr = address;
	this->size  = size;
}

VSpace::~VSpace(void)
{
	/* Free buffer */
	if (buffer)
		delete[] buffer;
}

u8 VSpace::Read8(u32 address)
{
	u32 addr = (address - vaddr);

	/* Return bye */
	return buffer[addr];
}

u16 VSpace::Read16(u32 address)
{
	u16 *buf = (u16 *)buffer;
	u32  idx = (address - vaddr) >> 1;

	/* Return half-word */
	return Swap16(buf[idx]);
}

u32 VSpace::Read32(u32 address)
{
	u32 *buf = (u32 *)buffer;
	u32  idx = (address - vaddr) >> 2;

	/* Return word */
	return Swap32(buf[idx]);
}

void VSpace::Write8(u32 address, u8 value)
{
	u32 addr = (address - vaddr);

	/* Write bye */
	buffer[addr] = value;
}

void VSpace::Write16(u32 address, u16 value)
{
	u16 *buf = (u16 *)buffer;
	u32  idx = (address - vaddr) >> 1;

	/* Write half-word */
	buf[idx] = Swap16(value);
}

void VSpace::Write32(u32 address, u32 value)
{
	u32 *buf = (u32 *)buffer;
	u32  idx = (address - vaddr) >> 2;

	/* Write word */
	buf[idx] = Swap32(value);
}

void VSpace::Memcpy(u32 dst, void *src, u32 size)
{
	u32 idx = (dst - vaddr);

	/* Copy data */
	memcpy(buffer + idx, src, size);
}

void VSpace::Memcpy(void *dst, u32 src, u32 size)
{
	u32 idx = (src - vaddr);

	/* Copy data */
	memcpy(dst, buffer + idx, size);
}


/*
 * Memory class
 */

vector<VSpace *> Memory::Spaces;


VSpace * Memory::Find(u32 address)
{
	vector<VSpace *>::iterator it;

	/* Find virtual space */
	for (it = Spaces.begin(); it < Spaces.end(); it++) {
		VSpace *space = *it;

		/* Check if address belongs to this virtual space */
		if (space->vaddr <= address &&
		    space->vaddr + space->size > address)
			return space;
	}

	/* Not found */
	return NULL;
}

bool Memory::Create(u32 vaddr, u32 size)
{
	VSpace *Space;

	/* Already exists */
	Space = Find(vaddr);
	if (Space)
		return true;

	/* Create virtual space */
	Space = new VSpace(vaddr, size);
	if (!Space)
		return false;

	/* Push virtual space */
	Spaces.push_back(Space);

	return true;
}

void Memory::Destroy(void)
{
	/* Pop virtual spaces */
	while (!Spaces.empty()) {
		VSpace *space;

		/* Pop virtual space */
		space = Spaces.back();
		Spaces.pop_back();

		/* Delete it */
		delete space;
	}
}

void Memory::Destroy(u32 vaddr)
{
	vector<VSpace *>::iterator it;

	/* Find virtual space */
	for (it = Spaces.begin(); it < Spaces.end(); it++) {
		VSpace *space = *it;

		/* Delete if found */
		if (space->vaddr == vaddr) {
			Spaces.erase(it);
			delete space;

			break;
		}
	}
}

bool Memory::LoadBinary(const char *filename, u32 &entry)
{
	char *buffer;
	u32   size;

	/* Read binary */
	buffer = Utils::FileRead(filename, size);
	if (!buffer)
		return false;

	/* Create virtual space */
	Create(0, size);

	/* Copy binary */
	Memcpy(0, buffer, size);

	/* Set entry point */
	entry = 0;

	/* Free buffer */
	delete buffer;

	return true;
}

bool Memory::LoadELF(const char *filename, u32 &entry)
{
	Elf32_Ehdr  ehdr;
	Elf32_Phdr *phdr;

	ifstream File;

	u32  phoff;
	u16  phnum, shnum;
	bool ret;

	/* Open file */
	File.open(filename);

	/* Check if open */
	ret = File.is_open();
	if (!ret)
		return false;

	/* Read ELF header */
	File.read((char *)&ehdr, sizeof(ehdr));

	/* Header parameters */
	phnum = Swap16(ehdr.e_phnum);
	phoff = Swap32(ehdr.e_phoff);
	shnum = Swap16(ehdr.e_shnum);
	entry = Swap32(ehdr.e_entry);

	printf("Entry point: 0x%08X\n", entry);

	/* Allocate array */
	phdr = new Elf32_Phdr[phnum];
	if (!phdr) {
		ret = false;
		goto out;
	}

	/* Read program headers */
	File.seekg(phoff, ios::beg);
	File.read ((char *)phdr, sizeof(*phdr) * phnum);

	printf("\n");
	printf("Program headers:\n");
	printf("================\n");

	for (u32 i = 0; i < phnum; i++) {
		u32 filesz = Swap32(phdr[i].p_filesz);
		u32 memsz  = Swap32(phdr[i].p_memsz);
		u32 offset = Swap32(phdr[i].p_offset);
		u32 paddr  = Swap32(phdr[i].p_paddr);
		u32 vaddr  = Swap32(phdr[i].p_vaddr);
		u32 flags  = Swap32(phdr[i].p_flags);

		printf("[%d] off    0x%08X vaddr 0x%08X paddr 0x%08X\n", i, offset, vaddr, paddr);
		printf("    filesz 0x%08X memsz 0x%08X flags %06X\n",   filesz, memsz, flags);

		/* Create virtual space */
		ret = Create(vaddr, memsz);
		if (!ret)
			goto out;

		/* Read data from file */
		if (filesz) {
			/* Move to offset */
			File.seekg(offset, ios::beg);

			for(u32 j = 0; j < filesz; j += 4) {
				u32 value;

				/* Read word */
				File.read((char *)&value, sizeof(value));

				/* Write word */
				Write32(vaddr + j, Swap32(value));
			}
		}
	}

	printf("\n");

out:
	/* Free array */
	delete[] phdr;

	/* Close file */
	File.close();

	return ret;
}

u8 Memory::Read8(u32 address)
{
	VSpace *Space;

	/* Find virtual space */
	Space = Find(address);
	if (!Space)
		return -1;

	/* Read byte */
	return Space->Read8(address);
}

u16 Memory::Read16(u32 address)
{
	VSpace *Space;

	/* Find virtual space */
	Space = Find(address);
	if (!Space)
		return -1;

	/* Read half-word */
	return Space->Read16(address);
}

u32 Memory::Read32(u32 address)
{
	VSpace *Space;

	/* Find virtual space */
	Space = Find(address);
	if (!Space)
		return -1;

	/* Read word */
	return Space->Read32(address);
}

void Memory::Write8(u32 address, u8 value)
{
	VSpace *Space;

	/* Find virtual space */
	Space = Find(address);
	if (!Space)
		return;

	/* Write byte */
	Space->Write8(address, value);
}

void Memory::Write16(u32 address, u16 value)
{
	VSpace *Space;

	/* Find virtual space */
	Space = Find(address);
	if (!Space)
		return;

	/* Write half-word */
	Space->Write16(address, value);
}

void Memory::Write32(u32 address, u32 value)
{
	VSpace *Space;

	/* Find virtual space */
	Space = Find(address);
	if (!Space)
		return;

	/* Write word */
	Space->Write32(address, value);
}

void Memory::Memcpy(u32 dst, void *src, u32 size)
{
	VSpace *Space;

	/* Find virtual space */
	Space = Find(dst);
	if (!Space)
		return;

	/* Copy data */
	Space->Memcpy(dst, src, size);
}

void Memory::Memcpy(void *dst, u32 src, u32 size)
{
	VSpace *Space;

	/* Find virtual space */
	Space = Find(src);
	if (!Space)
		return;

	/* Copy data */
	Space->Memcpy(dst, src, size);
}
