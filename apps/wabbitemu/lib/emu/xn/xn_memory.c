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

#include "xn.h"

void xn_z80_wrmem(TilemCalc* calc, dword A, byte v)
{
	unsigned long pa;
	byte page;

	page = calc->mempagemap[A>>14];

	if (A & 0x8000) {
		if (A > (0xFFFF - 64 * calc->hwregs[PORT27]))
			page = 0x80;
		else if (A < (0x8000 + 64 * calc->hwregs[PORT28]))
			page = 0x81;
	}

	pa = (A & 0x3FFF) + 0x4000L*page;

	if (pa < 0x200000) {
		calc->z80.clock += calc->hwregs[FLASH_WRITE_DELAY];
		if (calc->flash.state == 3) {
			*(calc->mem+pa) = v;
			calc->flash.state = 0;
		}
	}
	else if (pa < 0x220000) {
		calc->z80.clock += calc->hwregs[RAM_WRITE_DELAY];
		*(calc->mem+pa) = v;
	}
}

static inline byte readbyte(TilemCalc* calc, dword pa)
{
	static const byte protectbytes[6] = {0x00,0x00,0xed,0x56,0xf3,0xd3};
	int state = calc->hwregs[PROTECTSTATE];
	byte value;

	value = *(calc->mem + pa);

	if (pa < 0x1B0000 || pa >= 0x200000
	    || (pa >= 0x1C0000 && pa < 0x1F0000))
		calc->hwregs[PROTECTSTATE] = 0;
	else if (state == 6)
		calc->hwregs[PROTECTSTATE] = 7;
	else if (state < 6 && value == protectbytes[state])
		calc->hwregs[PROTECTSTATE] = state + 1;
	else
		calc->hwregs[PROTECTSTATE] = 0;

	return (value);
}

byte xn_z80_rdmem(TilemCalc* calc, dword A)
{
	byte page;
	unsigned long pa;
	byte value;

	page = calc->mempagemap[A>>14];

	if (A & 0x8000) {
		if (A > (0xFFFF - 64 * calc->hwregs[PORT27]))
			page = 0x80;
		else if (A < (0x8000 + 64 * calc->hwregs[PORT28]))
			page = 0x81;
	}

	if (TILEM_UNLIKELY(page == 0x7E && !calc->flash.unlock)) {
		tilem_warning(calc, "Reading from read-protected sector");
		return (0xff);
	}

	pa = (A & 0x3FFF) + 0x4000L*page;

	if (pa < 0x200000)
		calc->z80.clock += calc->hwregs[FLASH_READ_DELAY];
	else
		calc->z80.clock += calc->hwregs[RAM_READ_DELAY];

	value = readbyte(calc, pa);
	return (value);
}

byte xn_z80_rdmem_m1(TilemCalc* calc, dword A)
{
	byte page;
	unsigned long pa;
	byte value;

	page = calc->mempagemap[A>>14];

	if (A & 0x8000) {
		if (A > (0xFFFF - 64 * calc->hwregs[PORT27]))
			page = 0x80;
		else if (A < (0x8000 + 64 * calc->hwregs[PORT28]))
			page = 0x81;
	}

	if (TILEM_UNLIKELY((page & 0x80)
			   && calc->hwregs[NO_EXEC_RAM] & (1 << (page&7)))) {
		tilem_warning(calc, "Executing in restricted RAM area");
		tilem_z80_exception(calc, TILEM_EXC_RAM_EXEC);
	}

	if (TILEM_UNLIKELY(page >= calc->hwregs[PORT22]
			   && page <= calc->hwregs[PORT23])) {
		tilem_warning(calc, "Executing in restricted Flash area");
		tilem_z80_exception(calc, TILEM_EXC_FLASH_EXEC);
	}

	if (TILEM_UNLIKELY(page == 0x7E && !calc->flash.unlock)) {
		tilem_warning(calc, "Reading from read-protected sector");
		return (0xff);
	}

	pa = (A & 0x3FFF) + 0x4000L*page;

	if (pa < 0x200000)
		calc->z80.clock += calc->hwregs[FLASH_EXEC_DELAY];
	else
		calc->z80.clock += calc->hwregs[RAM_EXEC_DELAY];

	value = readbyte(calc, pa);

	if (TILEM_UNLIKELY(value == 0xff && A == 0x0038)) {
		tilem_warning(calc, "No OS installed");
		tilem_z80_exception(calc, TILEM_EXC_FLASH_EXEC);
	}

	return (value);
}

dword xn_mem_ltop(TilemCalc* calc, dword A)
{
	byte page = calc->mempagemap[A >> 14];

	if (A & 0x8000) {
		if (A > (0xFFFF - 64 * calc->hwregs[PORT27]))
			page = 0x80;
		else if (A < (0x8000 + 64 * calc->hwregs[PORT28]))
			page = 0x81;
	}

	return ((page << 14) | (A & 0x3fff));
}

dword xn_mem_ptol(TilemCalc* calc, dword A)
{
	byte page = A >> 14;

	if (!page)
		return (A & 0x3fff);

	if (page == calc->mempagemap[1])
		return (0x4000 | (A & 0x3fff));

	if ((A & 0x3fff) < 64 * calc->hwregs[PORT28]) {
		if (page == 0x81)
			return (0x8000 | (A & 0x3fff));
	}
	else {
		if (page == calc->mempagemap[2])
			return (0x8000 | (A & 0x3fff));
	}

	if ((A & 0x3fff) >= (0x4000 - 64 * calc->hwregs[PORT27])) {
		if (page == 0x80)
			return (0xC000 | (A & 0x3fff));
	}
	else {
		if (page == calc->mempagemap[3])
			return (0xC000 | (A & 0x3fff));
	}

	return (0xffffffff);
}
