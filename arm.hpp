/*
 * ARM9 emulator - Processor emulator
 * 
 * Copyright (C) 2011 - Miguel Boton (Waninkoko)
 * Copyright (C) 2010 - crediar, megazig
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

#ifndef _ARM9_HPP_
#define _ARM9_HPP_

#include <vector>
#include "types.h"

using namespace std;


/* Condition codes */
enum {
	EQ = 0,
	NE = 1,
	CS = 2,
	CC = 3,
	MI = 4,
	PL = 5,
	VS = 6,
	VC = 7,
	HI = 8,
	LS = 9,
	GE = 10,
	LT = 11,
	GT = 12,
	LE = 13,
	AL = 14,
};

/* ARM class */
class ARM {
	/* Registers */
	u32 r[16];
	u32 *pc;
	u32 *lr;
	u32 *sp;

	/* Special registers */
	union {
		struct {
			bool n:1;
			bool z:1;
			bool c:1;
			bool v:1;

			unsigned pad:20;

			bool I:1;
			bool F:1;
			bool t:1;
			u16  mode:5;
		};
		u32 value;
	} cpsr;
	u32 spsr;

	/* Breakpoint list */
	vector<u32> breakpoint;

private:
	/* Condition functions */
	bool CondCheck (u32 opcode);
	bool CondCheck (u16 opcode);
	void CondPrint (u32 opcode);
	void CondPrint (u16 opcode);

	/* Print functions */
	void SuffPrint (u32 opcode);
	void ShiftPrint(u32 opcode);

	/* Helper functions */
	bool CarryFrom (u32 a, u32 b);
	bool BorrowFrom(u32 a, u32 b);
	bool OverflowFrom(u32 a, u32 b);

	/* Operation functions */
	u32  Addition (u32 a, u32 b);
	u32  Substract(u32 a, u32 b);
	u32  Shift(u32 opcode, u32 value);

	/* Stack functions */
	void Push(u32 value);
	u32  Pop (void);

	/* Parse functions */
	void Parse(void);
	void ParseThumb(void);

public:
	ARM(void);

	/* Load functions */
	bool LoadBinary(const char *filename);
	bool LoadELF(const char *filename);
	void Unload(void);

	/* Execute functions */
	bool Step(void);

	/* Debug functions */
	void BreakAdd (u32 address);
	void BreakDel (u32 address);
	bool BreakFind(u32 address);
	void DumpRegs(void);
	void DumpStack(u32 count);

	/* Register peek/poke */
	inline u32 PeekReg(u8 idx) {
		return r[idx];
	};

	inline void PokeReg(u8 idx, u32 val) {
		r[idx] = val;
	};
};

#endif /* _ARM9_HPP_ */
