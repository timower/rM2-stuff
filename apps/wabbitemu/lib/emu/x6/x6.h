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

#ifndef _TILEM_X6_H
#define _TILEM_X6_H

enum {
	PORT3,			/* mask of enabled interrupts */
	PORT4,			/* mapping mode, timer control */
	PORT5,			/* memory mapping bank A */
	PORT6,			/* memory mapping bank B */
	PORT7,			/* link port control */
	NUM_HW_REGS
};

#define HW_REG_NAMES { "port3", "port4", "port5", "port6", "port7" }

#define TIMER_INT (TILEM_NUM_SYS_TIMERS + 1)
#define NUM_HW_TIMERS 1

#define HW_TIMER_NAMES { "int" }

void x6_reset(TilemCalc* calc);
byte x6_z80_in(TilemCalc* calc, dword port);
void x6_z80_out(TilemCalc* calc, dword port, byte value);
void x6_z80_ptimer(TilemCalc* calc, int id);
void x6_z80_wrmem(TilemCalc* calc, dword addr, byte value);
byte x6_z80_rdmem(TilemCalc* calc, dword addr);
dword x6_mem_ltop(TilemCalc* calc, dword addr);
dword x6_mem_ptol(TilemCalc* calc, dword addr);

#endif
