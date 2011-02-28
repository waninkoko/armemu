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

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <elf.h>

#include "arm.hpp"
#include "endian.h"
#include "memory.hpp"
#include "utils.hpp"

/* Shift/Rotate macros */
#define LSL(x,y)	(x << y)
#define LSR(x,y)	(x >> y)
#define ASR(x,y)	((x & (1 << 31)) ? ((x >> y) | ((~0 >> (32 - y)) << (32 - y))) : (x >> y))
#define ROR(x,y)	((x >> y) | (x << (32 - y)))


ARM::ARM(void)
{
	/* Set pointers */
	sp = (u32 *)(r + 13);
	lr = (u32 *)(r + 14);
	pc = (u32 *)(r + 15);

	/* Unload */
	Unload();
}

bool ARM::CondCheck(u32 opcode)
{
	/* Check condition */
	switch(opcode >> 28) {
	case EQ:
		return cpsr.z;
	case NE:
		return !cpsr.z;
	case CS:
		return cpsr.c;
	case CC:
		return !cpsr.c;
	case MI:
		return cpsr.n;
	case PL:
		return !cpsr.n;
	case VS:
		return cpsr.v;
	case VC:
		return !cpsr.v;
	case HI:
		return (cpsr.c && !cpsr.z);
	case LS:
		return (!cpsr.c || cpsr.z);
	case GE:
		return (cpsr.n == cpsr.v);
	case LT:
		return (cpsr.n != cpsr.v);
	case GT:
		return (cpsr.n == cpsr.v && !cpsr.z);
	case LE:
		return (cpsr.n != cpsr.v || cpsr.z);
	case AL:
		return true;
	}

	return false;
}

void ARM::CondPrint(u32 opcode)
{
	/* Check condition */
	switch (opcode >> 28) {
	case EQ:
		printf("eq"); break;
	case NE:
		printf("ne"); break;
	case CS:
		printf("cs"); break;
	case CC:
		printf("cc"); break;
	case MI:
		printf("mi"); break;
	case PL:
		printf("pl"); break;
	case VS:
		printf("vs"); break;
	case VC:
		printf("vc"); break;
	case HI:
		printf("hi"); break;
	case LS:
		printf("ls"); break;
	case GE:
		printf("ge"); break;
	case LT:
		printf("lt"); break;
	case GT:
		printf("gt"); break;
	case LE:
		printf("le"); break;
	}
}

void ARM::SuffPrint(u32 opcode)
{
	if ((opcode >> 20) & 1)
		printf("s");
}

void ARM::ShiftPrint(u32 opcode)
{
	u32 amt = (opcode >> 7)  & 0x1F;

	if (!amt)
		return;

	switch ((opcode >> 5) & 3) {
	case 0:
		printf(",LSL#%d", amt);
		break;
	case 1:
		printf(",LSR#%d", amt);
		break;
	case 2:
		printf(",ASR#%d", amt);
		break;
	case 3:
		printf(",ROR#%d", amt);
		break;
	}
}

bool ARM::CarryFrom(u32 a, u32 b)
{
	return ((a + b) < a) ? true : false;
}

bool ARM::BorrowFrom(u32 a, u32 b)
{
	return (a < b) ? true : false;
}

bool ARM::OverflowFrom(u32 a, u32 b)
{
	u32 s = a + b;

	if((a & (1 << 31)) == (b & (1 << 31)) &&
	   (s & (1 << 31)) != (a & (1 << 31)))
		return true;

	return false;
}

u32 ARM::Addition(u32 a, u32 b)
{
	u32 result;

	/* Add values */
	result = a + b;

	/* Set flags */
	cpsr.c = CarryFrom(a, b);
	cpsr.v = OverflowFrom(a, b);
	cpsr.z = result == 0;
	cpsr.n = result >> 31;

	return result;
}

u32 ARM::Substract(u32 a, u32 b)
{
	u32 result;

	/* Substract values */
	result = a - b;

	/* Set flags */
	cpsr.c = !BorrowFrom(a, b);
	cpsr.v = OverflowFrom(a, -b);
	cpsr.z = result == 0;
	cpsr.n = result >> 31;

	return result;
}

u32 ARM::Shift(u32 opcode, u32 value)
{
	bool S = (opcode >> 20) & 1;

	u32 amt = (opcode >> 7)  & 0x1F;
	u32 result;

	if (!amt)
		return value;

	switch ((opcode >> 5) & 3) {
	case 0:
		if (S) cpsr.c = value & (1 << (32 - amt));

		result = LSL(value, amt);
		break;
	case 1:
		if (S) cpsr.c = value & (1 << (amt - 1));

		result = LSR(value, amt);
		break;
	case 2:
		if (S) cpsr.c = value & (1 << (amt - 1));

		result = ASR(value, amt);
		break;
	case 3:
		result = ROR(value, amt);
		break;

	default:
		result = value;
	}

	return result;
}

void ARM::Push(u32 value)
{
	/* Update SP */
	*sp -= sizeof(u32);

	/* Write value */
	Memory::Write32(*sp, value);
}

u32 ARM::Pop(void)
{
	u32 addr = *sp;

	/* Update SP */
	*sp += sizeof(u32);

	/* Read value */
	return Memory::Read32(addr);
}

void ARM::Parse(void)
{
	u32 opcode;

	printf("%08X [A] ", *pc);

	/* Read opcode */
	opcode = Memory::Read32(*pc);

	/* Update PC */
	*pc += sizeof(opcode);

	/* Registers */
	u32 Rn    = ((opcode >> 16) & 0xF);
	u32 Rd    = ((opcode >> 12) & 0xF);
	u32 Rm    = ((opcode >> 0) & 0xF);
	u32 Rs    = ((opcode >> 8) & 0xF);
	u32 Imm   = ((opcode >> 0) & 0xFF);
	u32 amt   = Rs << 1;

	/* Flags */
	bool I = (opcode >> 25) & 1;
	bool P = (opcode >> 24) & 1;
	bool U = (opcode >> 23) & 1;
	bool B = (opcode >> 22) & 1;
	bool W = (opcode >> 21) & 1;
	bool S = (opcode >> 20) & 1;
	bool L = (opcode >> 20) & 1;

	if (((opcode >> 8) & 0xFFFFF) == 0x12FFF) {
		bool link = (opcode >> 5) & 1;

		printf("b%sx", (link) ? "l" : "");
		CondPrint(opcode);
		printf(" r%d\n", Rm);

		if (!CondCheck(opcode))
			return;

		if (link) *lr = *pc;

		cpsr.t = r[Rm] & 1;
		*pc    = r[Rm] & ~1;

		return;
	}

	if ((opcode >> 24) == 0xEF) {
		u8 Imm = opcode & 0xFF;

		printf("swi 0x%02X\n", Imm);
		return;
	}

	switch ((opcode >> 26) & 0x3) {
	case 0: {
		switch ((opcode >> 21) & 0xF) {
		case 0: {		// AND
			printf("and");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, 0x%X", Rd, Rn, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = r[Rn] & ROR(Imm, amt);
			else
				r[Rd] = r[Rn] & Shift(opcode, r[Rm]);

			if (S) {
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 1: {		// EOR
			printf("eor");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d\n", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, 0x%X\n", Rd, Rn, ROR(Imm, amt));

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = r[Rn] ^ ROR(Imm, amt);
			else
				r[Rd] = r[Rn] ^ Shift(opcode, r[Rm]);

			if (S) {
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 2: {		// SUB
			printf("sub");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, #0x%X", Rd, Rn, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = r[Rn] - ROR(Imm, amt);
			else
				r[Rd] = r[Rn] - Shift(opcode, r[Rm]);

			if (S) {
				cpsr.c = (I) ? (r[Rn] >= ROR(Imm, amt)) : (r[Rn] < r[Rd]);
				cpsr.v = (I) ? ((r[Rn] >> 31) & ~(r[Rd] >> 31)) : ((r[Rn] >> 31) & ~(r[Rd] >> 31));
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 3: {		// RSB
			printf("rsb");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, #0x%X", Rd, Rn, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = ROR(Imm, amt) - r[Rn];
			else
				r[Rd] = Shift(opcode, r[Rm]) - r[Rn];

			if (S) {
				cpsr.c = (I) ? (r[Rn] > Imm) : (r[Rn] > r[Rm]);
				cpsr.v = (I) ? ((Imm >> 31) & ~((Imm - r[Rn]) >> 31)) : ((Imm >> 31) & ~((r[Rm] - r[Rn]) >> 31));
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 4: {		// ADD
			printf("add");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, #0x%X", Rd, Rn, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd]  = r[Rn] + ROR(Imm, amt);
			else
				r[Rd] = r[Rn] + Shift(opcode, r[Rm]);

			if (Rn == 15)
				r[Rd] += 4;

			if (S) {
				cpsr.c = r[Rd] < r[Rn];
				cpsr.v = (r[Rn] >> 31) & ~(r[Rd] >> 31);
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 5: {		// ADC
			printf("adc");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, #0x%X", Rd, Rn, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = r[Rn] + ROR(Imm, amt) + cpsr.c;
			else
				r[Rd] = r[Rn] + Shift(opcode, r[Rm]) + cpsr.c;

			if (S) {
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 6: {		// SBC
			printf("sbc");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, #0x%X", Rd, Rn, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = r[Rn] - ROR(Imm, amt) - !cpsr.c;
			else
				r[Rd] = r[Rn] - Shift(opcode, r[Rm]) - !cpsr.c;

			if (S) {
				cpsr.c = r[Rd] > r[Rn];
				cpsr.v = (r[Rn] >> 31) & ~(r[Rd] >> 31);
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 7: {		// RSC
			printf("rsc");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, #0x%X", Rd, Rn, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = ROR(Imm, amt) - r[Rn] - !cpsr.c;
			else
				r[Rd] = Shift(opcode, r[Rm]) - r[Rn] - !cpsr.c;

			if (S) {
				cpsr.c = (I) ? (r[Rd] > Imm) : (r[Rd] > r[Rm]);
				cpsr.v = (I) ? ((r[Rm] >> 31) & ~(r[Rd] >> 31)) : ((r[Rn] >> 31) & ~(r[Rd] >> 31));
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 8: {		// TST/MRS
			if (S) {
				u32 result;

				printf("tst");
				CondPrint(opcode);

				if (!I) {
					printf(" r%d, r%d\n", Rn, Rm);
					ShiftPrint(opcode);

					result = r[Rn] & Shift(opcode, r[Rm]);
				} else {
					printf(" r%d, #0x%X\n", Rn, ROR(Imm, amt));
					result = r[Rn] & ROR(Imm, amt);
				}

				cpsr.z = result == 0;
				cpsr.n = result >> 31;
			} else {
				printf("mrs r%d, cpsr\n", Rd);
				r[Rd] = cpsr.value;
			}

			return;
		}

		case 9: {		// TEQ/MSR
			if (S) {
				u32 result;

				printf("teq");
				CondPrint(opcode);

				if (!I) {
					printf(" r%d, r%d\n", Rn, Rm);
					ShiftPrint(opcode);

					result = r[Rn] ^ Shift(opcode, r[Rm]);
				} else {
					printf(" r%d, #0x%X\n", Rn, ROR(Imm, amt));
					result = r[Rn] ^ ROR(Imm, amt);
				}

				cpsr.z = result == 0;
				cpsr.n = result >> 31;
			} else {
				if (I) {
					printf("msr cpsr, r%d\n", Rm);
					cpsr.value = r[Rm];
				} else {
					printf("msr cpsr, 0x%08X\n", Imm);
					cpsr.value = Imm;
				}
			}

			return;
		}

		case 10: {		// CMP/MRS2
			if (S) {
				u32 value;

				printf("cmp");
				CondPrint(opcode);

				if (I) {
					value = ROR(Imm, amt);
					printf(" r%d, 0x%08X\n", Rn, value);
				} else {
					value = r[Rm];
					printf(" r%d, r%d\n", Rn, Rm);
				}

				if (CondCheck(opcode))
					Substract(r[Rn], value);
			} else
				printf("mrs2\n");

			return;
		}

		case 11: {		// CMN/MSR2
			if (S) {
				printf("cmn r%d, r%d\n", Rd, Rm);
				CondPrint(opcode);

				if (CondCheck(opcode))
					Addition(r[Rd], r[Rm]);
			} else
				printf("msr2\n");

			return;
		}

		case 12: {		// ORR
			printf("orr");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, #0x%X", Rd, Rn, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = r[Rn] | ROR(Imm, amt);
			else 
				r[Rd] = r[Rn] | Shift(opcode, r[Rm]);

			if (S) {
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 13: {		// MOV
			printf("mov");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d", Rd, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, #0x%X", Rd, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = ROR(Imm, amt);
			else
				r[Rd] = Shift(opcode, r[Rm]);

			if (S) {
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 14: {		// BIC
			printf("bic");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d, r%d", Rd, Rn, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, r%d, #0x%X", Rd, Rn, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = r[Rn] & ~(ROR(Imm, amt));
			else
				r[Rd] = r[Rd] & ~Shift(opcode, r[Rm]);

			if (S) {
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}

		case 15: {		// MVN
			printf("mvn");
			CondPrint(opcode);
			SuffPrint(opcode);

			if (!I) {
				printf(" r%d, r%d", Rd, Rm);
				ShiftPrint(opcode);
			} else
				printf(" r%d, #0x%X", Rd, ROR(Imm, amt));

			printf("\n");

			if (!CondCheck(opcode))
				return;

			if (I)
				r[Rd] = ~ROR(Imm, amt);
			else
				r[Rd] = ~Shift(opcode, r[Rm]);

			if (S) {
				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;
			}

			return;
		}
		}
	}

	case 1: {
		u32 value;

		if (L) {		// LDR
			u32 addy;

			printf("ldr%s", (B) ? "b" : "");
			CondPrint(opcode);
			printf(" r%d,", Rd);

			if (Rn == 15) {
				Imm   = opcode & 0xFFF;
				value = Memory::Read32(r[Rn] + Imm + sizeof(opcode));

				printf(" =0x%08X\n", value);

				if (!CondCheck(opcode))
					return;

				r[Rd] = value;
			} else {
				printf(" [r%d", Rn);

				if (I) {
					value = Shift(opcode, r[Rm]);

					printf(", %sr%d", (U) ? "" : "-", Rm);
					ShiftPrint(opcode);
				} else {
					value = ROR(Imm, amt);
					printf(", #%s0x%08X", (U) ? "" : "-", value);
				}
				printf("]\n");

				if (!CondCheck(opcode))
					return;

				if (U)
					addy = r[Rn] + value;
				else
					addy = r[Rn] - value;

				if (P && W)
					r[Rn] = addy;

				if (B)
					r[Rd] = Memory::Read8(r[Rn]);
				else
					r[Rd] = Memory::Read32(r[Rn]);

				if (!P && W)
					r[Rn] = addy;
			}

			return;
		} else {		// STR
			u32 addy;

			printf("str%s", (B) ? "b" : "");
			CondPrint(opcode);
			printf(" r%d, [r%d", Rd, Rn);

			if (I) {
				value = Shift(opcode, r[Rm]);

				printf(", %sr%d", (U) ? "" : "-", Rm);
				ShiftPrint(opcode);
			} else {
				value = ROR(Imm, amt);
				printf(", #%s0x%08X", (U) ? "" : "-", value);
			}
			printf("]\n");

			if (!CondCheck(opcode))
				return;

			if (U)
				addy = r[Rn] + value;
			else
				addy = r[Rn] - value;

			if (P && W)
				r[Rn] = addy;

			if (B)
				Memory::Write8(r[Rn], r[Rd]);
			else
				Memory::Write32(r[Rn], r[Rd]);

			if (!P && W)
				r[Rn] = addy;

			return;
		}
	}

	default:
		break;
	}

	switch ((opcode >> 25) & 7) {
	case 4: {		// LDM/STM
		u32 start, end;
		u32 count = 0;

		for (u32 i = 0; i < 16; i++) {
			if ((opcode >> i) & 1)
				count++;
		}

		switch ((opcode >> 22) & 7) {
		case 0:
			start = r[Rn] - (count << 2) + 4;
			end   = start - (count << 2);
			break;
		case 2:
			start = r[Rn];
			end   = start + (count << 2);
			break;
		case 4:
			start = r[Rn] - (count << 2);
			end   = start;
			break;
		case 6:
			start = r[Rn] + 4;
			end   = start + (count << 2);
			break;
		}

		if (L) {
			printf("ldm");
			if (Rn == 13)
				printf("%c%c", (P) ? 'e' : 'f', (I) ? 'd' : 'a');
			else
				printf("%c%c", (I) ? 'i' : 'd', (P) ? 'b' : 'a');
		} else {
			printf("stm");
			if (Rn == 13)
				printf("%c%c", (P) ? 'f' : 'e', (I) ? 'a' : 'd');
			else
				printf("%c%c", (I) ? 'i' : 'd', (P) ? 'b' : 'a');
		}

		if (Rn == 13)
			printf(" sp");
		else
			printf(" r%d", Rn);

		if (W) printf("!");
		printf(", {");

		for (u32 i = 0; i < 16; i++) {
			if ((opcode >> i) & 1) {
				if (L) {
					r[i]   = Memory::Read32(start);
					start += sizeof(u32);
				} else {
					Memory::Write32(start, r[i]);
					start += sizeof(u32);
				}

				printf("r%d,", i);
			}
		}

		if (W)
			r[Rn] = end;

		printf("}\n");
		return;
	}

	case 5: {		// B/BL
		bool link = opcode & (1 << 24);

		printf("b%s", (link) ? "l" : "");
		CondPrint(opcode);

		Imm = ((opcode & 0xFFFFFF) << 2) + sizeof(opcode);
		if (Imm & (1 << 25))
			Imm = ~(~Imm & 0xFFFFFF);

		printf(" 0x%08X\n", *pc + Imm);

		if (!CondCheck(opcode))
			return;

		if (link) *lr = *pc;
		*pc += Imm;

		return;
	}

	case 7: {		// MRC
		printf("mrc ...\n");
		return;
	}
	}

	printf("Unknown opcode! (0x%08X)\n", opcode);
}

void ARM::ParseThumb(void)
{
	u16 opcode;

	printf("%08X [T] ", *pc);

	/* Read opcode */
	opcode = Memory::Read16(*pc);

	/* Update PC */
	*pc += sizeof(opcode);

	if ((opcode >> 13) == 0) {
		u32 Imm = (opcode >> 6) & 0x1F;
		u32 Rn  = (opcode >> 6) & 7;
		u32 Rm  = (opcode >> 3) & 7;
		u32 Rd  = (opcode >> 0) & 7;

		switch ((opcode >> 11) & 3) {
		case 0: {		// LSL
			if (Imm > 0 && Imm <= 32) {
				cpsr.c = r[Rd] & (1 << (32 - Imm));
				r[Rd]  = LSR(r[Rd], Imm);
			}

			if (Imm > 32) {
				cpsr.c = 0;
				r[Rd]  = 0;
			}

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("lsl r%d, r%d, #0x%02X\n", Rd, Rm, Imm);
			return;
		}

		case 1: {		// LSR
			if (Imm > 0 && Imm <= 32) {
				cpsr.c = r[Rd] & (1 << (Imm - 1));
				r[Rd]  = LSR(r[Rd], Imm);
			}

			if (Imm > 32) {
				cpsr.c = 0;
				r[Rd]  = 0;
			}

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("lsr r%d, r%d, #0x%02X\n", Rd, Rm, Imm);
			return;
		}

		case 2: {		// ASR
			if (Imm > 0 && Imm <= 32) {
				cpsr.c = r[Rd] & (1 << (Imm - 1));
				r[Rd]  = ASR(r[Rd], Imm);
			}

			if (Imm > 32) {
				cpsr.c = 0;
				r[Rd]  = 0;
			}

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("asr r%d, r%d, #0x%02X\n", Rd, Rm, Imm);
			return;
		}

		case 3: {		// ADD, SUB
			if (opcode & 0x400) {
				Imm &= 7;

				if (opcode & 0x200) {
					r[Rd] = Substract(r[Rm], Imm);

					printf("sub r%d, r%d, #0x%02X\n", Rd, Rm, Imm);
					return;
				} else {
					r[Rd] = Addition(r[Rm], Imm);

					printf("add r%d, r%d, #0x%02X\n", Rd, Rm, Imm);
				}
			} else {
				if (opcode & 0x200) {
					r[Rd] = Substract(r[Rm], r[Rn]);

					printf("sub r%d, r%d, r%d\n", Rd, Rm, Rn);
					return;
				} else {
					r[Rd] = Addition(r[Rm], r[Rn]);

					printf("add r%d, r%d, r%d\n", Rd, Rm, Rn);
					return;
				}
			}

			return;
		}
		}
	}

	if ((opcode >> 13) == 1) {
		u32 Imm = (opcode & 0xFF);
		u32 Rn  = (opcode >> 8) & 7;

		switch ((opcode >> 11) & 3) {
		case 0: {		// MOV
			r[Rn] = Imm;

			cpsr.z = r[Rn] == 0;
			cpsr.n = r[Rn] >> 31;

			printf("mov r%d, #0x%02X\n", Rn, Imm);
			return;
		}

		case 1: {		// CMP
			Substract(r[Rn], Imm);

			printf("cmp r%d, #0x%02X\n", Rn, Imm);
			return;
		}

		case 2: {		// ADD
			r[Rn] = Addition(r[Rn], Imm);

			printf("add r%d, #0x%02X\n", Rn, Imm);
			return;
		}

		case 3: {		// SUB
			r[Rn] = Substract(r[Rn], Imm);

			printf("sub r%d, #0x%02X\n", Rn, Imm);
			return;
		}
		}
	}

	if ((opcode >> 10) == 0x10) {
		u32 Rd = opcode & 7;
		u32 Rm = (opcode >> 3) & 7;

		switch ((opcode >> 6) & 0xF) {
		case 0: {		// AND
			r[Rd] &= r[Rm];

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("and r%d, r%d\n", Rd, Rm);
			return;
		}

		case 1: {		// EOR
			r[Rd] ^= r[Rm];

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("eor r%d, r%d\n", Rd, Rm);
			return;
		}

		case 2: {		// LSL
			u8 shift = r[Rm] & 0xFF;

			if (shift > 0 && shift <= 32) {
				cpsr.c = r[Rd] & (1 << (32 - shift));
				r[Rd]  = LSL(r[Rd], shift);
			}

			if (shift > 32) {
				cpsr.c = 0;
				r[Rd]  = 0;
			}

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("lsl r%d, r%d\n", Rd, Rm);
			return;
		}

		case 3: {		// LSR
			u8 shift = r[Rm] & 0xFF;

			if (shift > 0 && shift <= 32) {
				cpsr.c = r[Rd] & (1 << (shift - 1));
				r[Rd]  = LSR(r[Rd], shift);
			}

			if (shift > 32) {
				cpsr.c = 0;
				r[Rd]  = 0;
			}

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("lsr r%d, r%d\n", Rd, Rm);
			return;
		}

		case 4: {		// ASR
			u8 shift = r[Rm] & 0xFF;

			if (shift > 0 && shift < 32) {
				cpsr.c = r[Rd] & (1 << (shift - 1));
				r[Rd]  = ASR(r[Rd], shift);
			}

			if (shift == 32) {
				cpsr.c = r[Rd] >> 31;
				r[Rd]  = 0;
			}

			if (shift > 32) {
				cpsr.c = 0;
				r[Rd]  = 0;
			}

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("asr r%d, r%d\n", Rd, Rm);
			return;
		}

		case 5: {		// ADC
			r[Rd] = Addition(r[Rd], r[Rm]);
			r[Rd] = Addition(r[Rd], cpsr.c);

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("adc r%d, r%d\n", Rd, Rm);
			return;
		}

		case 6: {		// SBC
			r[Rd] = Substract(r[Rd], r[Rm]);
			r[Rd] = Substract(r[Rd], !cpsr.c);

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("sbc r%d, r%d\n", Rd, Rm);
			return;
		}
			
		case 7: {		// ROR
			u8 shift = r[Rm] & 0xFF;

			while (shift >= 32)
				shift -= 32;

			if (shift) {
				cpsr.c = r[Rd] & (1 << (shift - 1));
				r[Rd]  = ROR(r[Rd], shift);
			}

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("ror r%d, r%d\n", Rd, Rm);
			return;			
		}

		case 8: {		// TST
			u32 result = r[Rd] & r[Rm];

			cpsr.z = result == 0;
			cpsr.n = result >> 31;

			printf("tst r%d, r%d\n", Rd, Rm);
			return;
		}

		case 9: {		// NEG
			r[Rd] = -r[Rm];

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("neg r%d, r%d\n", Rd, Rm);
			return;
		}

		case 10: {		// CMP
			Substract(r[Rd], r[Rm]);

			printf("cmp r%d, r%d\n", Rd, Rm);
			return;
		}

		case 11: {		// CMN/MVN
			if (opcode & 0x100) {
				r[Rd] = !r[Rm];

				cpsr.z = r[Rd] == 0;
				cpsr.n = r[Rd] >> 31;

				printf("mvn r%d, r%d\n", Rd, Rm);
			} else {
				Addition(r[Rd], r[Rm]);

				printf("cmn r%d, r%d\n", Rd, Rm);
			}

			return;
		}

		case 12: {		// ORR
			r[Rd] |= r[Rm];

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("orr r%d, r%d\n", Rd, Rm);
			return;
		}

		case 13: {		// MUL
			r[Rd] *= r[Rm];

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("mul r%d, r%d\n", Rd, Rm);
			return;
		}

		case 14: {		// BIC
			r[Rd] &= ~r[Rm];

			cpsr.z = r[Rd] == 0;
			cpsr.n = r[Rd] >> 31;

			printf("bic r%d, r%d\n", Rd, Rm);
			return;
		}
		}
	}

	if ((opcode >> 7) == 0x8F) {
		u32 Rm = (opcode >> 3) & 0xF;

		*lr = *pc | 1;
		
		cpsr.t = r[Rm] & 1;
		*pc    = r[Rm] & ~1;

		printf("blx r%d\n", Rm);
		return;
	}

	if ((opcode >> 10) == 0x11) {
		u32 Rd = ((opcode >> 4) & 8) | (opcode & 7);
		u32 Rm = ((opcode >> 3) & 0xF);

		switch ((opcode >> 8) & 3) {
		case 0: {		// ADD
			r[Rd] = Addition(r[Rd], r[Rm]);

			printf("add r%d, r%d\n", Rd, Rm);
			return;
		}

		case 1: {		// CMP
			Substract(r[Rd], r[Rm]);

			printf("cmp r%d, r%d\n", Rd, Rm);
			return;
		}

		case 2: {		// MOV (NOP)
			if (Rd == 8 && Rm == 8) {
				printf("nop");
				return;
			}

			r[Rd] = r[Rm];

			printf("mov r%d, r%d\n", Rd, Rm);
			return;
		}

		case 3: {		// BX
			cpsr.t = r[Rm] & 1;

			if (Rm == 15)
				*pc += sizeof(opcode);
			else
				*pc = r[Rm] & ~1;

			printf("bx r%d\n", Rm);
			return;
		}
		}
	}

	if ((opcode >> 11) == 9) {
		u32 Rd   = (opcode >> 8) & 7;
		u32 Imm  = (opcode & 0xFF);
		u32 addr = *pc + (Imm << 2) + sizeof(opcode);

		r[Rd] = Memory::Read32(addr);

		printf("ldr r%d, =0x%08X\n", Rd, r[Rd]);
		return;
	}

	if ((opcode >> 12) == 5) {
		u32 Rd = (opcode >> 0) & 7;
		u32 Rn = (opcode >> 3) & 7;
		u32 Rm = (opcode >> 6) & 7;

		switch ((opcode >> 9) & 7) {
		case 0: {		// STR
			u32 addr  = r[Rn] + r[Rm];
			u32 value = r[Rd];

			Memory::Write32(addr, value);

			printf("str r%d, [r%d, r%d]\n", Rd, Rn, Rm);
			return;
		}

		case 2: {		// STRB
			u32 addr  = r[Rn] + r[Rm];
			u8  value = r[Rd] & 0xFF;

			Memory::Write8(addr, value);

			printf("strb r%d, [r%d, r%d]\n", Rd, Rn, Rm);
			return;
		}

		case 4: {		// LDR
			u32 addr = r[Rn] + r[Rm];

			r[Rd] = Memory::Read32(addr);

			printf("ldr r%d, [r%d, r%d]\n", Rd, Rn, Rm);
			return;
		}

		case 6: {		// LDRB
			u32 addr = r[Rn] + r[Rm];

			r[Rd] = Memory::Read8(addr);

			printf("ldrb r%d, [r%d, r%d]\n", Rd, Rn, Rm);
			return;
		}
		}
	}

	if ((opcode >> 13) == 3) {
		u32 Rd  = (opcode >> 0) & 7;
		u32 Rn  = (opcode >> 3) & 7;
		u32 Imm = (opcode >> 6) & 7;

		if (opcode & 0x1000) {
			if (opcode & 0x800) {
				u32 addr = r[Rn] + (Imm << 2);

				r[Rd] = Memory::Read8(addr);

				printf("ldrb r%d, [r%d, 0x%02X]\n", Rd, Rn, Imm);
			} else {
				u32 addr  = r[Rn] + (Imm << 2);
				u8  value = r[Rd] & 0xFF;

				Memory::Write8(addr, value);

				printf("strb r%d, [r%d, 0x%02X]\n", Rd, Rn, Imm);
			}
		} else {
			if (opcode & 0x800) {
				u32 addr = r[Rn] + (Imm << 2);

				r[Rd] = Memory::Read32(addr);

				printf("ldr r%d, [r%d, 0x%02X]\n", Rd, Rn, Imm << 2);
			} else {
				u32 addr  = r[Rn] + (Imm << 2);
				u32 value = r[Rd];

				Memory::Write32(addr, value);

				printf("str r%d, [r%d, 0x%02X]\n", Rd, Rn, Imm << 2);
			}
		}

		return;
	}

	if ((opcode >> 12) == 8) {
		u32 Rd  = (opcode >> 0) & 7;
		u32 Rn  = (opcode >> 3) & 7;
		u32 Imm = (opcode >> 6) & 7;

		if (opcode & 0x800) {
			u32 addr = r[Rn] + (Imm << 1);

			r[Rd] = Memory::Read16(addr);

			printf("ldrh r%d, [r%d, 0x%02X]\n", Rd, Rn, Imm << 1);
		} else {
			u32 addr  = r[Rn] + (Imm << 1);
			u16 value = r[Rd];

			Memory::Write16(addr, value);

			printf("strh r%d, [r%d, 0x%02X]\n", Rd, Rn, Imm << 1);
		}

		return;
	}

	if ((opcode >> 12) == 9) {
		u32 Rd  = (opcode >> 8) & 7;
		u32 Imm = (opcode & 0xFF);

		if (opcode & 0x800) {
			u32 addr = *sp + (Imm << 2);

			r[Rd] = Memory::Read32(addr);

			printf("ldr r%d, [sp, 0x%02X]\n", Rd, Imm << 2);
		} else {
			u32 addr  = *sp + (Imm << 2);
			u16 value = r[Rd];

			Memory::Write32(addr, value);

			printf("str r%d, [sp, 0x%02X]\n", Rd, Imm << 2);
		}

		return;
	}

	if ((opcode >> 12) == 10) {
		u32 Rd  = (opcode >> 8) & 7;
		u32 Imm = (opcode & 0xFF);

		if (opcode & 0x800) {
			r[Rd] = *sp + (Imm << 2);

			printf("add r%d, sp, #0x%02X\n", Rd, Imm << 2);
		} else {
			r[Rd] = (*pc & ~2) + (Imm << 2);

			printf("add r%d, pc, #0x%02X\n", Rd, Imm << 2);
		}

		return;
	}

	if ((opcode >> 12) == 11) {
		switch ((opcode >> 9) & 7) {
		case 0: {		// ADD/SUB
			u32 Imm = (opcode & 0x7F);

			if (opcode & 0x80) {
				*sp -= Imm << 2;
				printf("sub sp, #0x%02X\n", Imm << 2);
			} else {
				*sp += Imm << 2;
				printf("add sp, #0x%02X\n", Imm << 2);
			}

			return;
		}

		case 2: {		// PUSH
			bool lrf = opcode & 0x100;
			bool pf  = false;

			if (lrf)
				Push(*lr);

			for (s32 i = 7; i >= 0; i--)
				if ((opcode >> i) & 1)
					Push(r[i]);

			printf("push {");

			for (s32 i = 0; i < 8; i++) {
				if ((opcode >> i) & 1) {
					if (pf) printf(",");
					printf("r%d", i);

					pf = true;
				}
			}

			if (lrf) {
				if (pf) printf(",");
				printf("lr");
			}

			printf("}\n");
			return;
		}

		case 6: {		// POP
			bool pcf = opcode & 0x100;
			bool pf  = false;

			if (pcf) {
				*pc    = Pop();
				cpsr.t = *pc & 1;
			}

			printf("pop {");

			for (s32 i = 0; i < 8; i++) {
				if ((opcode >> i) & 1) {
					if (pf) printf(",");
					printf("r%d", i);

					r[i] = Pop();
					pf   = true;
				}
			}

			if (pcf) {
				if (pf) printf(",");
				printf("pc");
			}

			printf("}\n");
			return;
		}
		}
	}

	if ((opcode >> 12) == 12) {
		u32 Rn = (opcode >> 8) & 7;

		if (opcode & 0x800) {
			printf("ldmia r%d!, {", Rn);

			for (u32 i = 0; i < 8; i++) {
				if ((opcode >> i) & 1) {
					r[i]   = Memory::Read32(r[Rn]);
					r[Rn] += sizeof(u32);
					
					printf("r%d,", i);
				}
			}

			printf("}\n");
			return;
		} else {
			printf("stmia r%d!, {", Rn);

			for (u32 i = 0; i < 8; i++) {
				if ((opcode >> i) & 1) {
					Memory::Write32(r[Rn], r[i]);
					r[Rn] += sizeof(u32);
					
					printf("r%d,", i);
				}
			}

			printf("}\n");
			return;
		}
	}

	if ((opcode >> 12) == 13) {
		u32 Imm = (opcode & 0xFF) << 1;

		if (Imm & 0x100)
			Imm = ~((~Imm) & 0xFF);

		Imm += 2;

		switch ((opcode >> 8) & 0xF) {
		case 0: {		// BEQ
			printf("beq 0x%08X\n", *pc + Imm);

			if (cpsr.z)
				*pc += Imm;

			return;
		}

		case 1: {		// BNE
			printf("bne 0x%08X\n", *pc + Imm);

			if (!cpsr.z)
				*pc += Imm;

			return;
		}

		case 2: {		// BCS
			printf("bcs 0x%08X\n", *pc + Imm);

			if (cpsr.c)
				*pc += Imm;

			return;
		}

		case 3: {		// BCC
			printf("bcc 0x%08X\n", *pc + Imm);

			if (!cpsr.c)
				*pc += Imm;

			return;
		}

		case 4: {		// BMI
			printf("bmi 0x%08X\n", *pc + Imm);

			if (cpsr.n)
				*pc += Imm;

			return;
		}

		case 5: {		// BPL
			printf("bpl 0x%08X\n", *pc + Imm);

			if (!cpsr.n)
				*pc += Imm;

			return;
		}

		case 6: {		// BVS
			printf("bvs 0x%08X\n", *pc + Imm);

			if (cpsr.v)
				*pc += Imm;

			return;
		}

		case 7: {		// BVC
			printf("bvc 0x%08X\n", *pc + Imm);

			if (!cpsr.v)
				*pc += Imm;

			return;
		}

		case 8: {		// BHI
			printf("bhi 0x%08X\n", *pc + Imm);

			if (!cpsr.z && cpsr.c)
				*pc += Imm;

			return;
		}

		case 9: {		// BLS
			printf("bls 0x%08X\n", *pc + Imm);

			if (cpsr.z || !cpsr.c)
				*pc += Imm;

			return;
		}

		case 10: {		// BGE
			printf("bge 0x%08X\n", *pc + Imm);

			if (cpsr.n == cpsr.v)
				*pc += Imm;

			return;
		}

		case 11: {		// BLT
			printf("blt 0x%08X\n", *pc + Imm);

			if (cpsr.n != cpsr.v)
				*pc += Imm;

			return;
		}

		case 12: {		// BGT
			printf("bgt 0x%08X\n", *pc + Imm);

			if (!cpsr.z &&
			     cpsr.n != cpsr.v)
				*pc += Imm;

			return;
		}

		case 13: {		// BLE
			printf("ble 0x%08X\n", *pc + Imm);

			if (cpsr.z ||
			    cpsr.n != cpsr.v)
				*pc += Imm;

			return;
		}

		case 14: {		// BAL
			printf("bal 0x%08X\n", *pc + Imm);

			*pc += Imm;

			return;
		}

		case 15: {		// SWI
			printf("swi 0x%02X\n", Imm >> 1);
			return;
		}
		}
	}

	if ((opcode >> 11) == 28) {
		u32 Imm = (opcode & 0x7FF) << 1;

		if (Imm & (1 << 11)) {
			Imm  = (~Imm) & 0xFFE;
			*pc -= Imm;
		} else
			*pc += Imm + 2;

		printf("b 0x%08X, 0x%X\n", *pc, Imm);
		return;
	}

	if ((opcode >> 11) == 0x1E) {
		u32  opc = Memory::Read16(*pc);

		u32  Imm = ((opcode & 0x7FF) << 12) | ((opc & 0x7FF) << 1);
		bool blx = ((opcode >> 11) & 3) == 3;

		*lr  = *pc + sizeof(opcode);
		*lr |= 1;

		if (Imm & (1 << 22)) {
			Imm  = (~Imm) & 0x7FFFFE;
			*pc -= Imm;
		} else
			*pc += Imm + 2;

		if (blx) {
			cpsr.t = 0;
			printf("blx 0x%08X\n", *pc);
		} else
			printf("bl 0x%08X\n", *pc);

		return;
	}

	printf("Unknown opcode! (0x%04X)\n", opcode);
}

bool ARM::LoadBinary(const char *filename)
{
	char *buffer;
	u32   size;

	/* Read binary */
	buffer = Utils::FileRead(filename, size);
	if (!buffer)
		return false;

	/* Create virtual space */
	Memory::Create(0, size);

	/* Copy binary */
	Memory::Memcpy(0, buffer, size);

	/* Set PC */
	*pc = 0;

	/* Free buffer */
	delete buffer;

	return true;
}

bool ARM::LoadELF(const char *filename)
{
	Elf32_Ehdr  ehdr;
	Elf32_Phdr *phdr;

	ifstream File;

	u32  entry, phoff;
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
		ret = Memory::Create(vaddr, memsz);
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
				Memory::Write32(vaddr + j, Swap32(value));
			}
		}
	}

	printf("\n");

	/* Set PC */
	*pc = entry;

out:
	/* Free array */
	delete[] phdr;

	/* Close file */
	File.close();

	return ret;
}

void ARM::Unload(void)
{
	/* Clean registers */
	memset(r, 0, sizeof(r));
	cpsr.value = spsr = 0;
}

bool ARM::Step(void)
{
	bool ret;

	/* Remove thumb bit */
	*pc &= ~1;

	/* Check breakpoint */
	ret = BreakFind(*pc);
	if (ret) {
		cout << "BREAKPOINT! (0x" << hex << *pc << ")" << endl;
		return false;
	}

	/* Parse instruction */
	if (cpsr.t)
		ParseThumb();
	else
		Parse();

	return true;
}

void ARM::BreakAdd(u32 address)
{
	bool ret;

	/* Check if exists */
	ret = BreakFind(address);

	/* Add breakpoint */
	if (!ret)
		breakpoint.push_back(address);
}

void ARM::BreakDel(u32 address)
{
	vector<u32>::iterator it;

	/* Search breakpoint */
	for (it = breakpoint.begin(); it != breakpoint.end(); it++) {
		/* Delete breakpoint */
		if (*it == address) {
			breakpoint.erase(it);
			return;
		}
	}
}

bool ARM::BreakFind(u32 address)
{
	vector<u32>::iterator it;

	/* Search breakpoint */
	for (it = breakpoint.begin(); it != breakpoint.end(); it++) {
		/* Found breakpoint */
		if (*it == address)
			return true;
	}

	return false;
}

void ARM::DumpRegs(void)
{
	cout << "REGISTERS DUMP:" << endl;
	cout << "===============" << endl;

	/* Print GPRs */
	for (u32 i = 0; i < 16; i += 2)
		printf("r%-2d: 0x%08X\t\tr%-2d: 0x%08X\n", i, r[i], i+1, r[i+1]);

	cout << endl;

	/* Print CPSR */
	cout << "cpsr: 0x" << hex << cpsr.value;
	cout << " (z: " << cpsr.z << ", n: " << cpsr.n << ", c: " << cpsr.c << ", v: " << cpsr.v
	     << ", I: " << cpsr.I << ", F: " << cpsr.F << ", t: " << cpsr.t << ", mode: " << cpsr.mode << ")" << endl;

	/* Print SPSR */
	cout << "spsr: 0x" << hex << spsr << endl;
}

void ARM::DumpStack(u32 count)
{
	cout << "STACK DUMP:" << endl;
	cout << "===========" << endl;

	/* Print stack */
	for (u32 i = 0; i < count; i++) {
		u32 addr = *sp + (i << 2);
		u32 value;

		/* Read stack */
		value = Memory::Read32(addr);

		/* Print value */
		printf("[%02d] 0x%08X\n", i, value);
	}
}
