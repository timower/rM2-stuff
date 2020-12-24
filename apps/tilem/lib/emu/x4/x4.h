/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2009-2011 Benjamin Moody
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

#ifndef _TILEM_X4_H
#define _TILEM_X4_H

enum {
	PORT3,			/* mask of enabled interrupts */
	PORT4,			/* interrupt timer speed */
	PORT5,			/* memory mapping bank C */
	PORT6,			/* memory mapping bank A */
	PORT7,			/* memory mapping bank B */
	PORT8,			/* link assist mode flags */
	PORT9,			/* unknown (link assist settings?) */
	PORTA,			/* unknown (timeout value?) */
	PORTB,			/* unknown (timeout value?) */
	PORTC,			/* unknown (timeout value?) */
	PORTD,			/* unknown */
	PORTE,			/* unknown */
	PORTF,			/* unknown */

	PORT20,			/* CPU speed control */
	PORT21,			/* hardware type / RAM no-exec control */
	PORT22,			/* Flash no-exec lower limit */
	PORT23,			/* Flash no-exec upper limit */
	PORT25,			/* RAM no-exec lower limit */
	PORT26,			/* RAM no-exec upper limit */
	PORT27,			/* bank C forced-page-0 limit */
	PORT28,			/* bank B forced-page-1 limit */
	PORT29,			/* LCD port delay (6 MHz) */
	PORT2A,			/* LCD port delay (mode 1) */
	PORT2B,			/* LCD port delay (mode 2) */
	PORT2C,			/* LCD port delay (mode 3) */
	PORT2D,			/* unknown */
	PORT2E,			/* memory delay */
	PORT2F,			/* Duration of LCD wait timer */

	CLOCK_MODE,		/* clock mode */
	CLOCK_INPUT,		/* clock input register */
	CLOCK_DIFF,		/* clock value minus actual time */

	RAM_READ_DELAY,
	RAM_WRITE_DELAY,
	RAM_EXEC_DELAY,
	FLASH_READ_DELAY,
	FLASH_WRITE_DELAY,
	FLASH_EXEC_DELAY,
	LCD_PORT_DELAY,
	NO_EXEC_RAM_MASK,
	NO_EXEC_RAM_LOWER,
	NO_EXEC_RAM_UPPER,

	LCD_WAIT,		/* LCD wait timer active */
	PROTECTSTATE,		/* port protection state */
	NUM_HW_REGS
};

#define HW_REG_NAMES \
	{ "port3", "port4", "port5", "port6", "port7", "port8", "port9", \
	  "portA", "portB", "portC", "portD", "portE", "portF", "port20", \
	  "port21", "port22", "port23", "port25", "port26", "port27", \
	  "port28", "port29", "port2A", "port2B", "port2C", "port2D", \
	  "port2E", "port2F", "clock_mode", "clock_input", "clock_diff", \
	  "ram_read_delay", "ram_write_delay", "ram_exec_delay", \
	  "flash_read_delay", "flash_write_delay", "flash_exec_delay", \
	  "lcd_port_delay", "no_exec_ram_mask", "no_exec_ram_lower", \
	  "no_exec_ram_upper", "lcd_wait", "protectstate" }

#define TIMER_INT1 (TILEM_NUM_SYS_TIMERS + 1)
#define TIMER_INT2A (TILEM_NUM_SYS_TIMERS + 2)
#define TIMER_INT2B (TILEM_NUM_SYS_TIMERS + 3)
#define TIMER_LCD_WAIT (TILEM_NUM_SYS_TIMERS + 4)
#define NUM_HW_TIMERS 4

#define HW_TIMER_NAMES { "int1", "int2a", "int2b", "lcd_wait" }

void x4_reset(TilemCalc* calc);
void x4_stateloaded(TilemCalc* calc, int savtype);
byte x4_z80_in(TilemCalc* calc, dword port);
void x4_z80_out(TilemCalc* calc, dword port, byte value);
void x4_z80_ptimer(TilemCalc* calc, int id);
void x4_z80_wrmem(TilemCalc* calc, dword addr, byte value);
byte x4_z80_rdmem(TilemCalc* calc, dword addr);
byte x4_z80_rdmem_m1(TilemCalc* calc, dword addr);
dword x4_mem_ltop(TilemCalc* calc, dword addr);
dword x4_mem_ptol(TilemCalc* calc, dword addr);

#endif
