/*
 * ARM9 emulator - Main code
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

#include "arm.hpp"
#include "memory.hpp"
#include "utils.hpp"

/* Constants */
#define STACK_SIZE	(8 * 1024)	// 8KB stack


int main(int argc, char **argv)
{
	ARM Cpu;

	u32  entry;
	s32  steps;
	bool ret;

	/* Show usage */
	if (argc < 4) {
		cerr << "[USAGE]: " << argv[0] << " [b <binary file> | e <elf file>] <# of steps> (breakpoint)" << endl;
		return 1;
	}

	/* Read arguments */
	steps = Utils::StrToInt(argv[3]);

	/* Check mode */
	switch (argv[1][0]) {
	case 'b':
		/* Load binary */
		ret = Memory::LoadBinary(argv[2], entry);
		if (!ret) {
			cerr << "[ERROR]: Could not load the binary file!" << endl;
			return 1;
		}

		break;

	case 'e':
		/* Load ELF */
		ret = Memory::LoadELF(argv[2], entry);
		if (!ret) {
			cerr << "[ERROR]: Could not load the ELF file!" << endl;
			return 1;
		}

		break;

	default:
		cerr << "[ERROR]: Invalid option!" << endl;
		return 1;
	}

	if (argc >= 4) {
		s32 address = Utils::HexToInt(argv[4]);

		/* Add breakpoint */
		Cpu.BreakAdd(address);
	}

	/* Create stack */
	Memory::Create(0xFFFFFFFF - STACK_SIZE, STACK_SIZE);

	/* Set program counter */
	Cpu.SetPC(entry);

	/* Step CPU */
	while (steps-- && Cpu.Step());
	cout << endl;

	/* Dump registers */
	Cpu.DumpRegs();
	cout << endl;

	/* Dump stack */
	Cpu.DumpStack(8);

	/* Destroy virtual memory */
	Memory::Destroy();

	return 0;
}
