/*
 * ARM9 emulator - Utility routines
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

#ifndef __UTILS_H__
#define __UTILS_H__

#include "types.h"


/* Utils class */
class Utils {
public:
	/* File functions */
	static char *FileRead (const char *filename, u32 &size);
	static bool  FileWrite(const char *filename, const char *buffer, u32 size);

	/* Conversion functions */
	static s32 StrToInt(const char *str);
	static s32 HexToInt(const char *str);
};

#endif /* __UTILS_H__ */
