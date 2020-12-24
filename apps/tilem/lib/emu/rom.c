/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2009 Benjamin Moody
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include "tilem.h"

static int find_string(const char *str, FILE *romfile,
		       dword start, dword limit)
{
	char buf[256];
	int pos = 0;
	int len, i;

	len = strlen(str);

	fseek(romfile, (long int) start, SEEK_SET);

	for (i = 0; i < len-1; i++) {
		buf[pos] = fgetc(romfile);
		pos = (pos+1)%256;
		limit--;
	}

	while (limit > 0 && !feof(romfile) && !ferror(romfile)) {
		buf[pos] = fgetc(romfile);
		pos = (pos+1)%256;
		limit--;

		for (i = 0; i < len; i++) {
			if (str[i] != buf[(pos + 256 - len + i)%256])
				break;
		}
		if (i == len)
			return 1;
	}
	return 0;
}

char tilem_guess_rom_type(FILE* romfile)
{
	unsigned long initpos;
	dword size;
	char result;

	initpos = ftell(romfile);

	fseek(romfile, 0L, SEEK_END);
	size = ftell(romfile);

	if (size >= 0x8000 && size < 0x9000) {
		/* 32k: TI-81 (old or new) */
		result = TILEM_CALC_TI81;
	}
	else if (size >= 0x20000 && size < 0x2C000) {
		/* 128k: TI-82 or TI-86 */
		if (find_string("CATALOG", romfile, 0, 0x20000))
			result = TILEM_CALC_TI85;
		else
			result = TILEM_CALC_TI82;
	}
	else if (size >= 0x40000 && size < 0x4C000) {
		/* 256k: TI-83 (or a variant) or TI-86 */
		if (!find_string("TI82", romfile, 0, 0x40000))
			result = TILEM_CALC_TI86;
		else if (find_string("Termin\x96", romfile, 0, 0x40000))
			result = TILEM_CALC_TI76;
		else
			result = TILEM_CALC_TI83;
	}
	else if (size >= 0x80000 && size < 0x8C000) {
		/* 512k: TI-83 Plus or TI-73 */
		if (find_string("TI-83 Plus", romfile, 0, 8 * 0x4000))
			result = TILEM_CALC_TI83P;
		else
			result = TILEM_CALC_TI73;
	}
	else if (size >= 0x100000 && size < 0x124000) {
		/* 1024k: TI-84 Plus */
		result = TILEM_CALC_TI84P;
	}
	else if (size >= 0x200000 && size < 0x224000) {
		/* 2048k: TI-83 Plus SE, TI-84 Plus SE */
		if (find_string("\xed\xef", romfile, 0x1FC000, 0x4000))
			result = TILEM_CALC_TI84P_NSPIRE;
		else if (find_string("Operating", romfile, 0x1FC000, 0x4000))
			result = TILEM_CALC_TI84P_SE;
		else
			result = TILEM_CALC_TI83P_SE;
	}
	else {
		result = 0;
	}

	fseek(romfile, initpos, SEEK_SET);
	return result;
}
