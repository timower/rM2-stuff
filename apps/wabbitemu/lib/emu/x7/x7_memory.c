/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2001 Solignac Julien
 * Copyright (C) 2004-2011 Benjamin Moody
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
#include <tilem.h>

#include "x7.h"

void x7_z80_wrmem(TilemCalc* calc, dword A, byte v)
{
	unsigned long pa;

	pa = (A & 0x3FFF) + 0x4000*calc->mempagemap[(A)>>14];

	if (pa<0x80000)
		tilem_flash_write_byte(calc, pa, v);

	else if (pa < 0x88000)
		*(calc->mem+pa) = v;
}

static inline byte readbyte(TilemCalc* calc, dword pa)
{
	static const byte protectbytes[6] = {0x00,0x00,0xed,0x56,0xf3,0xd3};
	int state = calc->hwregs[PROTECTSTATE];
	byte value;

	if (pa < 0x80000 && (calc->flash.state || calc->flash.busy))
		value = tilem_flash_read_byte(calc, pa);
	else
		value = *(calc->mem + pa);

	if (pa < 0x70000 || pa >= 0x80000)
		calc->hwregs[PROTECTSTATE] = 0;
	else if (state == 6)
		calc->hwregs[PROTECTSTATE] = 7;
	else if (state < 6 && value == protectbytes[state])
		calc->hwregs[PROTECTSTATE] = state + 1;
	else
		calc->hwregs[PROTECTSTATE] = 0;

	return (value);
}

byte x7_z80_rdmem(TilemCalc* calc, dword A)
{
	byte page;
	unsigned long pa;
	byte value;

	page = calc->mempagemap[A>>14];
	pa = 0x4000 * page + (A & 0x3FFF);
 
	if (TILEM_UNLIKELY(page == 0x1E && !calc->flash.unlock)) {
		tilem_warning(calc, "Reading from read-protected sector");
		return (0xff);
	}

	value = readbyte(calc, pa);
	return(value);
}

byte x7_z80_rdmem_m1(TilemCalc* calc, dword A)
{
	byte page;
	unsigned long pa;
	byte value;

	page = calc->mempagemap[A>>14];
	pa = 0x4000 * page + (A & 0x3FFF);

	if (TILEM_UNLIKELY(calc->hwregs[NOEXEC] & (1 << (page % 4)))) {
		tilem_warning(calc, "Executing in restricted Flash area");
		tilem_z80_exception(calc, TILEM_EXC_FLASH_EXEC);
	}

	if (TILEM_UNLIKELY(page == 0x1E && !calc->flash.unlock)) {
		tilem_warning(calc, "Reading from read-protected sector");
		tilem_z80_exception(calc, TILEM_EXC_RAM_EXEC);
	}

	value = readbyte(calc, pa);

	if (TILEM_UNLIKELY(value == 0xff && A == 0x0038)) {
		tilem_warning(calc, "No OS installed");
		tilem_z80_exception(calc, TILEM_EXC_FLASH_EXEC);
	}

	return (value);
}

dword x7_mem_ltop(TilemCalc* calc, dword A)
{
	byte page = calc->mempagemap[A >> 14];
	return ((page << 14) | (A & 0x3fff));
}

dword x7_mem_ptol(TilemCalc* calc, dword A)
{
	byte page = A >> 14;
	int i;

	for (i = 0; i < 4; i++) {
		if (calc->mempagemap[i] == page) {
			return ((i << 14) | (A & 0x3fff));
		}
	}

	return (0xffffffff);
}
