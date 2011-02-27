/*
 * ARM9 emulator - Endianess routines
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

#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#include "types.h"

inline u16 Swap16(u16 val)
{
#if (defined(__HOST_LE__) && defined(__TARGET_LE__)) || \
    (defined(__HOST_BE__) && defined(__TARGET_BE__))
	return val;
#else
	return ((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8);
#endif
}

inline u32 Swap32(u32 val)
{
#if (defined(__HOST_LE__) && defined(__TARGET_LE__)) || \
    (defined(__HOST_BE__) && defined(__TARGET_BE__))
	return val;
#else
	return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) |
	       ((val & 0xFF000000) >> 24) | ((val & 0x00FF0000) >> 8);
#endif
}

#endif /* __ENDIAN_H__ */
