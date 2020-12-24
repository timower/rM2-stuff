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

#ifndef _TILEM_X2_H
#define _TILEM_X2_H

enum {
	HW_VERSION,		/* HW version: 0 = unknown
				   1 = original HW (1993-2000)
				   2 = revised HW (2001-2003) */
	PORT0,			/* port 0 (link port control) */
	PORT2,			/* port 2 (memory mapping) */
	PORT3,			/* mask of enabled interrupts */
	PORT4,			/* mapping mode + timer speed */
	NUM_HW_REGS
};

#define HW_REG_NAMES { "hw_version", "port0", "port2", "port3", "port4" }

#define TIMER_INT (TILEM_NUM_SYS_TIMERS + 1)
#define NUM_HW_TIMERS 1

#define HW_TIMER_NAMES { "int" }

void x2_reset(TilemCalc* calc);
byte x2_z80_in(TilemCalc* calc, dword port);
void x2_z80_out(TilemCalc* calc, dword port, byte value);
void x2_z80_ptimer(TilemCalc* calc, int id);
void x2_z80_wrmem(TilemCalc* calc, dword addr, byte value);
byte x2_z80_rdmem(TilemCalc* calc, dword addr);
dword x2_mem_ltop(TilemCalc* calc, dword addr);
dword x2_mem_ptol(TilemCalc* calc, dword addr);

#endif
