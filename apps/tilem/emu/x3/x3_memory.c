/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2001 Solignac Julien
 * Copyright (C) 2004-2009 Benjamin Moody
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

#include "x3.h"

void x3_z80_wrmem(TilemCalc* calc, dword A, byte v)
{
	dword pa = 0x4000 * calc->mempagemap[(A)>>14] + (A & 0x3FFF);

	if (pa >= 0x40000) {
		*(calc->mem + pa) = v;
	}
}

byte x3_z80_rdmem(TilemCalc* calc, dword A)
{
	dword pa = 0x4000 * calc->mempagemap[(A)>>14] + (A & 0x3FFF);
	return (*(calc->mem + pa));
}

dword x3_mem_ltop(TilemCalc* calc, dword A)
{
	byte page = calc->mempagemap[A >> 14];
	return ((page << 14) | (A & 0x3fff));
}

dword x3_mem_ptol(TilemCalc* calc, dword A)
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
