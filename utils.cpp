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

#include <fstream>

#include "utils.hpp"

using namespace std;


char * Utils::FileRead(const char *filename, u32 &size)
{
	ifstream file;

	char *buffer;
	bool  ret;

	/* Open file */
	file.open(filename);

	/* Check if open */
	ret = file.is_open();
	if (!ret)
		return NULL;

	/* Get filesize */
	file.seekg(0, ios::end);
	size = file.tellg();
	file.seekg(0, ios::beg);

	/* Allocate buffer */
	buffer = new char[size];
	if (!buffer)
		return NULL;

	/* Read file */
	file.read(buffer, size);

	/* Close file */
	file.close();

	return buffer;
}

bool Utils::FileWrite(const char *filename, const char *buffer, u32 size)
{
	ofstream file;
	bool ret;

	/* Open file */
	file.open(filename);

	/* Check if open */
	ret = file.is_open();
	if (!ret)
		return false;

	/* Write file */
	file.write(buffer, size);

	/* Close file */
	file.close();

	return true;
}
