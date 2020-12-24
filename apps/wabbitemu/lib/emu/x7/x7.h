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

#ifndef _TILEM_X7_H
#define _TILEM_X7_H

enum {
	PORT3,			/* mask of enabled interrupts */
	PORT4,			/* interrupt timer speed */
	PORT6,			/* memory mapping bank A */
	PORT7,			/* memory mapping bank B */
	NOEXEC,			/* no-exec sector map */
	PROTECTSTATE,		/* port protection state */
	NUM_HW_REGS
};

#define HW_REG_NAMES					\
	{ "port3", "port4", "port6", "port7", "noexec", \
	  "protectstate" }

#define TIMER_INT1 (TILEM_NUM_SYS_TIMERS + 1)
#define TIMER_INT2A (TILEM_NUM_SYS_TIMERS + 2)
#define TIMER_INT2B (TILEM_NUM_SYS_TIMERS + 3)
#define NUM_HW_TIMERS 3

#define HW_TIMER_NAMES { "int1", "int2a", "int2b" }

void x7_reset(TilemCalc* calc);
void x7_stateloaded(TilemCalc* calc, int savtype);
byte x7_z80_in(TilemCalc* calc, dword port);
void x7_z80_out(TilemCalc* calc, dword port, byte value);
void x7_z80_ptimer(TilemCalc* calc, int id);
void x7_z80_wrmem(TilemCalc* calc, dword addr, byte value);
byte x7_z80_rdmem(TilemCalc* calc, dword addr);
byte x7_z80_rdmem_m1(TilemCalc* calc, dword addr);
dword x7_mem_ltop(TilemCalc* calc, dword addr);
dword x7_mem_ptol(TilemCalc* calc, dword addr);

#endif
